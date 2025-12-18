/*
 * Interface board Factory App
 * - Start as open AP named RDB-<MAC>
 * - Start mDNS as hostname "interface-board"
 * - Start HTTP server with endpoints:
 *   GET / -> UI
 *   GET /version -> running app info
 *   GET /partitions -> ota partition info
 *   POST /upload -> upload firmware to selected ota partition
 *   POST /set_boot?target=ota_0|ota_1 -> set boot partition and restart
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_image_format.h"
#include "esp_spiffs.h"
#include <algorithm>

#ifdef TAG
#undef TAG
#endif
#define TAG "RDB Factory"

/// @brief Drains a request body (non fatal)
/// @param req
static void drain_remaining_body(httpd_req_t *req)
{
	const size_t DRAIN_BUFSZ = 1024;
	static char drainbuf[DRAIN_BUFSZ];
	int64_t remaining = req->content_len;
	// If content_len is not set (-1) you might still want to try a non-blocking drain
	ESP_LOGW(__func__, "Draining request content : %ld bytes", remaining);
	while (remaining > 0)
	{
		ssize_t r = httpd_req_recv(req, drainbuf, std::min((size_t)remaining, (size_t)DRAIN_BUFSZ));
		if (r <= 0)
			break;
		remaining -= r;
	}
}

/// @brief Declares the MDNS instances and sets it up on the Hotspot wifi
/// @param
static void start_mdns(void)
{
	mdns_init();
	mdns_hostname_set("right-display");
	mdns_instance_name_set("Right Display Factory");
}

/// @brief Handler for the general index GET request
/// @param req GET /simple.min.css
/// @return
static esp_err_t stylesheet_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);

	// Look for the css file in the spiffs partition (needs to be mounted beforehand)
	FILE *f = fopen("/spiffs/simple.min.css", "r");
	if (!f)
	{
		ESP_LOGE(TAG, "Failed to open index.html");
		httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
		return ESP_FAIL;
	}
	// Set HTTP response to text/css type
	httpd_resp_set_type(req, "text/css");

	// Chunk the file to send in buffers of 512 bytes and send it away until the end of the file.
	char buf[512];
	while (fgets(buf, sizeof(buf), f))
	{
		httpd_resp_send_chunk(req, buf, strlen(buf));
	}
	// Terminate with a null chunk to signal end of stream
	httpd_resp_send_chunk(req, NULL, 0);
	// Close the file
	fclose(f);
	return ESP_OK;
}

/// @brief Handler for the general index GET request
/// @param req GET /
/// @return
static esp_err_t index_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);

	// Look for the html file in the spiffs partition (needs to be mounted beforehand)
	FILE *f = fopen("/spiffs/index.html", "r");
	if (!f)
	{
		ESP_LOGE(TAG, "Failed to open index.html");
		httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
		return ESP_FAIL;
	}
	// Set HTTP response to text/html type
	httpd_resp_set_type(req, "text/html");

	// Chunk the file to send in buffers of 512 bytes and send it away until the end of the file.
	char buf[512];
	while (fgets(buf, sizeof(buf), f))
	{
		httpd_resp_send_chunk(req, buf, strlen(buf));
	}
	// Terminate with a null chunk to signal end of stream
	httpd_resp_send_chunk(req, NULL, 0);
	// Close the file
	fclose(f);
	return ESP_OK;
}

/// @brief Handler to the /version fetch from the load event in JS
/// @param req GET /version
/// @return
static esp_err_t version_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);

	const esp_app_desc_t *app_desc = esp_app_get_description();
	// Case where somehow the app description of the factory partition is corrupted.
	if (app_desc == NULL)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No valid app description found ! Reflash over UART/USB");
		ESP_LOGE(TAG, "No Factory app description was found in the partition.");
		return ESP_FAIL;
	}

	// Compose a 256 chars buffer with a JSON payload
	char buf[256];
	snprintf(buf, sizeof(buf), "{\"project_name\":\"%s\",\"version\":\"%s\",\"date\":\"%s-%s\"}",
			 app_desc ? app_desc->project_name : "",
			 app_desc ? app_desc->version : "",
			 app_desc ? app_desc->date : "",
			 app_desc ? app_desc->time : "");
	// Set response type and send the buffer away to the event listener
	ESP_LOGI(__func__, "Response to request : %s", buf);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, buf);
	return ESP_OK;
}

/// @brief Handler to refresh the partitions information
/// @param req GET request with URI "/partitions"
/// @return
static esp_err_t partitions_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);
	// Shortlisted to only the interesting partition names
	// const char *labels[] = {"nvs", "phy_init", "factory", "ota_0", "ota_1", "ota_data", "storage", "nvs_odo"};
	const char *labels[] = {"factory", "ota_0", "ota_1"};
	const int nlabels = sizeof(labels) / sizeof(labels[0]);
	// Output buffer is 1024 chars max
	char buf[1024];

	size_t off = 0;
	off += snprintf(buf + off, sizeof(buf) - off, "{");
	ESP_LOGD(__func__, "Offset : %u @ %u\nBuffer : %s", off, __LINE__, buf);
	// Cycle through the target partition names
	for (int i = 0; i < nlabels; ++i)
	{
		const char *lab = labels[i];
		ESP_LOGI(__func__, "Searching for %s", lab);
		const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, lab);
		// Partition not found
		if (!p)
		{
			ESP_LOGI(__func__, "No partition found with name %s", lab);
			continue;
		}
		// Partition found
		// Append to buf with offset off
		off += snprintf(buf + off, sizeof(buf) - off, "\"%s\":{\"address\":\"0x%08x\",\"size\":%u,\"type\":%d,\"subtype\":%d",
						lab, (unsigned int)p->address, (unsigned int)p->size, p->type, p->subtype);
		ESP_LOGD(__func__, "Offset : %u @ %u\nBuffer : %s", off, __LINE__, buf);
		// If the label is any of those three special cases, we try to read the header of the app in the partition
		if (strcmp(lab, "factory") == 0 || strcmp(lab, "ota_0") == 0 || strcmp(lab, "ota_1") == 0)
		{
			esp_app_desc_t desc;
			esp_ota_img_states_t partState;

			if (esp_ota_get_partition_description(p, &desc) == ESP_OK)
			{
				ESP_LOGI(__func__, "Partition %s : found app header", lab);
				off += snprintf(buf + off, sizeof(buf) - off, " ,\"project_name\":\"%s\",\"version\":\"%s\",\"date\":\"%s-%s\" ",
								desc.project_name, desc.version, desc.date, desc.time);
				ESP_LOGD(__func__, "Offset : %u @ %u\nBuffer : %s", off, __LINE__, buf);
			}
			// Add partition state if not factory APP
			if (p->type == ESP_PARTITION_TYPE_APP && p->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY)
			{
				esp_err_t noState = esp_ota_get_state_partition(p, &partState);

				char partStateEnum[24];
				int partStateEnum_length;
				if (noState != ESP_OK)
				{
					ESP_LOGW(__func__, "Could not get partition %s state:%s", p->label, esp_err_to_name(noState));
					partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "UNKNOWN");
				}
				else
				{
					switch (partState)
					{
					case ESP_OTA_IMG_ABORTED:
						partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "ABORTED");
						break;
					case ESP_OTA_IMG_INVALID:
						partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "INVALID");
						break;
					case ESP_OTA_IMG_NEW:
						partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "NEW");
						break;
					case ESP_OTA_IMG_PENDING_VERIFY:
						partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "PENDING");
						break;
					case ESP_OTA_IMG_UNDEFINED:
						partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "UNDEFINED");
						break;
					case ESP_OTA_IMG_VALID:
						partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "VALID");
						break;
					default:
						partStateEnum_length = snprintf(partStateEnum, sizeof(partStateEnum), "NO STATE");
						break;
					}
				}

				off += snprintf(buf + off, sizeof(buf) - off, " ,\"state\":\"%s\"", partStateEnum);
				ESP_LOGD(__func__, "Offset : %u @ %u\nBuffer : %s", off, __LINE__, buf);
			}
		}
		// Terminating the JSON payload
		off += snprintf(buf + off, sizeof(buf) - off, "},");
	}
	// Remove any stray comma
	if (off > 1 && buf[off - 1] == ',')
	{
		buf[off - 1] = '\0';
		off--;
	}
	off += snprintf(buf + off, sizeof(buf) - off, "}");
	ESP_LOGD(__func__, "Offset : %u @ %u\nBuffer : %s", off, __LINE__, buf);
	// Package JSON and respond to request
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, buf);
	return ESP_OK;
}

/// @brief Handler to fetch from NVS the previously booted partition. Not 100% accurate all the time
/// @param req GET /prevboot
/// @return
static esp_err_t prevboot_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);
	ESP_LOGW(__func__, "Dumping state information.");
	const esp_partition_t *bootPartition = esp_ota_get_boot_partition();
	const esp_partition_t *ota0Partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota_0");
	const esp_partition_t *ota1Partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "ota_1");
	esp_ota_img_states_t ota0State, ota1State;

	nvs_handle_t h;
	nvs_open("storage", NVS_READONLY, &h);
	// Defaulting to the factory app partition
	int8_t prevBootID = -1;
	esp_err_t nvs_err = nvs_get_i8(h, "lastPart", &prevBootID);
	if (nvs_err != ESP_OK)
		ESP_LOGW(__func__, "Error reading lastPart : %s", esp_err_to_name(nvs_err));

	nvs_close(h);
	// Prepare short buffers for the lastPartition and nextPartition
	char lastPart[10];
	char nextPart[10];

	switch (prevBootID)
	{
	case -1: // Last boot was in factory
		strncpy(lastPart, "factory", sizeof(lastPart));
		strncpy(nextPart, "ota_0", sizeof(nextPart));
		break;
	case 0: // Last boot was ota_0, ext ota should be in 1
		strncpy(lastPart, "ota_0", sizeof(lastPart));
		strncpy(nextPart, "ota_1", sizeof(nextPart));
		break;
	case 1: // Opposite case to 0
		strncpy(lastPart, "ota_1", sizeof(lastPart));
		strncpy(nextPart, "ota_0", sizeof(nextPart));
		break;
	default:
		break;
	}

	// DEBUG
	const esp_partition_t *lastbooted = esp_ota_get_last_invalid_partition();
	if (lastbooted != NULL)
		ESP_LOGI(__func__, "Last invalidated partition : %s", lastbooted->label);
	else
		ESP_LOGI(__func__, "Could not retrieve last invalidated partition.");

	// Some override if some states can be picked up
	esp_err_t staterr;
	if (ota0Partition != NULL)
	{
		staterr = esp_ota_get_state_partition(ota0Partition, &ota0State);
		if (staterr == ESP_OK)
		{
			ESP_LOGI(__func__, "OTA 0 state : %u ", ota0State);
			if (ota0State == ESP_OTA_IMG_ABORTED || ota0State == ESP_OTA_IMG_INVALID)
				strncpy(nextPart, "ota_0", sizeof(nextPart));
		}
		else
			ESP_LOGW(__func__, "Could not determine the state of ota_0");
	}
	if (ota1Partition != NULL)
	{
		staterr = esp_ota_get_state_partition(ota1Partition, &ota1State);
		if (staterr == ESP_OK)
		{
			ESP_LOGI(__func__, "OTA 1 state : %u ", ota1State);
			if (ota1State == ESP_OTA_IMG_ABORTED || ota1State == ESP_OTA_IMG_INVALID)
				strncpy(nextPart, "ota_1", sizeof(nextPart));
		}
		else
			ESP_LOGW(__func__, "Could not determine the state of ota_1");
	}
	// Prepare json response
	char buf[256];
	snprintf(buf, sizeof(buf), "{\"bootPart\":\"%s\",\"prevRunningPart\":\"%s\",\"recOTAPart\":\"%s\"}", bootPartition->label, lastPart, nextPart);
	ESP_LOGI(__func__, "Response to request : %s", buf);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, buf);
	return ESP_OK;
}

// /// @brief Handler for the request to get odometer and trip values
// /// @param req GET /odometer
// /// @return
// static esp_err_t odometer_get_handler(httpd_req_t *req)
// {
// 	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);

// 	uint32_t odometer_m = 0;
// 	uint32_t trip_m = 0;
// 	nvs_handle_t h;
// 	esp_err_t err = ESP_FAIL;

// 	/* Try reading from partition "nvs_odo" with namespace "storage", fall back to default */
// 	err = nvs_open_from_partition("nvs_odo", "storage", NVS_READONLY, &h);
// 	if (err != ESP_OK)
// 	{
// 		ESP_LOGW(__func__, "Could not open the nvs_odo partition with namespace storage, attempting to open the default nvs partition.");
// 		err = nvs_open("storage", NVS_READONLY, &h);
// 		if (err != ESP_OK)
// 		{
// 			ESP_LOGW(__func__, "Could not open the default nvs partition.");
// 			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
// 			return ESP_FAIL;
// 		}
// 	}
// 	ESP_LOGI(__func__, "Partition successfully opened.");
// 	if (nvs_get_u32(h, "odometer_m", &odometer_m) == ESP_ERR_NVS_NOT_FOUND)
// 	{
// 		ESP_LOGW(__func__, "Could not find the odometer_m key in nvs.");
// 		odometer_m = UINT32_MAX;
// 	}
// 	if (nvs_get_u32(h, "trip_m", &trip_m) == ESP_ERR_NVS_NOT_FOUND)
// 	{
// 		ESP_LOGW(__func__, "Could not find the trip_m key in nvs.");
// 		trip_m = UINT32_MAX;
// 	}
// 	nvs_close(h);
// 	// Preparing the JSON object for response
// 	char out[128];
// 	snprintf(out, sizeof(out), "{\"odometer_m\":%lu,\"trip_m\":%lu}", odometer_m, trip_m);
// 	httpd_resp_set_type(req, "application/json");
// 	httpd_resp_sendstr(req, out);
// 	return ESP_OK;
// }

/// @brief Handler for the Reboot button
/// @param req POST /reboot
/// @return
static esp_err_t reboot_post_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);

	httpd_resp_sendstr(req, "Rebooting");
	vTaskDelay(pdMS_TO_TICKS(200));
	esp_restart();
	return ESP_OK;
}

/// @brief Handler for the Upload and Flash request (doUpload())
/// @param req POST /upload?target=&size=
/// @return
static esp_err_t upload_post_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s Length: %u", req->method, req->uri, req->content_len);

	// Check the query length is correct
	size_t query_length = httpd_req_get_url_query_len(req);
	if (query_length == 0)
	{
		ESP_LOGE(__func__, "Query had no length");
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);
		return ESP_FAIL;
	}
	char query_buffer[query_length + 1];
	size_t file_size;
	char target[5 + 1];
	char file_size_str[10];
	// Retrieve the query string
	if (httpd_req_get_url_query_str(req, query_buffer, sizeof(query_buffer)) != ESP_OK)
	{
		ESP_LOGE(__func__, "Could not retrieve URL Query string");
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);
		return ESP_FAIL;
	}
	ESP_LOGI(__func__, "Query string: %s", query_buffer);

	// Check for filesize, first as a string then as a number
	if (httpd_query_key_value(query_buffer, "size", file_size_str, sizeof(file_size_str)) != ESP_OK)
	{
		ESP_LOGE(__func__, "Could not retrieve file size string");
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);
		return ESP_FAIL;
	}
	file_size = atol(file_size_str);
	ESP_LOGI(__func__, "File size from URL : %u", file_size);

	// Check for target partition then
	if (httpd_query_key_value(query_buffer, "target", target, sizeof(target)) != ESP_OK)
	{
		ESP_LOGE(__func__, "Could not retrieve target partition");
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);
		return ESP_FAIL;
	}
	ESP_LOGI(__func__, "Target partition: %s", target);

	// Prepare pointer towards the partition that will be updated
	const esp_partition_t *update_partition = NULL;

	// Check which ECU it is and take appropriate preparation steps
	if (strstr(target, "ota_0")) // Target is OTA 0
	{
		ESP_LOGI(__func__, "Prepping for ota_0");
		update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "ota_0");
	}
	else if (strstr(target, "ota_1")) // Target is OTA 1
	{
		ESP_LOGI(__func__, "Prepping for ota_1");
		update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "ota_1");
	}
	else // Target is unknown
	{
		ESP_LOGE(__func__, "No valid target found, aborting");
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Target partition unknown");
		return ESP_FAIL;
	}

	// Check on partition before proceeding further
	if (!update_partition)
	{
		ESP_LOGE(__func__, "Could not find target partition");
		drain_remaining_body(req);
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}

	ESP_LOGI(__func__, "Summary before flashing : content is %u bytes, file size is %u bytes", req->content_len, file_size);
	// Exit point here if req->content_len<file_size
	if (req->content_len != file_size)
	{
		ESP_LOGE(__func__, "File size does not correspond to content length");
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size does not match content length");
		return ESP_FAIL;
	}

	// Allocate some buffer to receive in segments of 4K
	const size_t buf_size = 4096;
	char *buf = static_cast<char *>(malloc(buf_size));
	if (!buf)
	{
		ESP_LOGE(__func__, "Could not allocate reception buffer");
		drain_remaining_body(req);
		httpd_resp_send_500(req);
		return ESP_ERR_NO_MEM;
	}

	// Reject if file size is below 4K (suss)
	if (file_size < 4096)
	{
		ESP_LOGE(__func__, "File size is below 4K, too small");
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size is suspiciously too small");
		free(buf);
		return ESP_FAIL;
	}

	// Main process

	int receivedBytes = 0;						 // Tracker for received Bytes out of content_len
	uint32_t writtenBytes = 0;					 // Written bytes by OTA
	bool image_header_OK = false;				 // Indicates if the image header check has been performed
	const esp_app_desc_t *img_descriptor = NULL; // Will be used to load the metadata of the app being loaded.
	esp_app_desc_t image_metadata;				 // Long term storage of the image header
	int bytesToReceive = 0;						 // In-loop target for httpd retrieval
	int bytesRetrieved = 0;						 // Actual number of retrieved bytes waiting to be sent (max 4096)
	esp_ota_handle_t OTA_handle;
	bool OTA_started = false;
	esp_err_t OTA_err;

	while (writtenBytes < file_size)
	{
		// Request a new chunk only if previous chunk is exhausted
		if (writtenBytes == receivedBytes)
		{
			bytesToReceive = std::min(req->content_len - receivedBytes, buf_size); // In order not to exceed content_len
			bytesRetrieved = httpd_req_recv(req, buf, bytesToReceive);			   // Attempt to receive
			if (bytesRetrieved <= 0)											   // Edge case of timeout or no more content. Might benefit from not retrying for ever
			{
				if (bytesRetrieved == HTTPD_SOCK_ERR_TIMEOUT)
					continue; // Go for another loop in case of timeout
				// Implicitely else and exit otherwise
				ESP_LOGE(__func__, "Chunk retrieval failed, error %d", bytesRetrieved);
				free(buf);
				drain_remaining_body(req);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Chunk retrieval failed.");
				return ESP_FAIL;
			}
			receivedBytes += bytesRetrieved;
			ESP_LOGI(__func__, "Received new chunk of length %d, expected %d, total received %u out of %u, file size %u", bytesRetrieved, bytesToReceive, receivedBytes, req->content_len, file_size);
		}

		// Check first chunk for image header
		if (!image_header_OK)
		{
			ESP_LOGI(__func__, "Scanning for OxABCD5432");
			uint32_t *scan = (uint32_t *)buf; // Start at edge of buf
			uint32_t scan_end = buf_size / 4; // Stop at end of buf
			for (uint32_t i = 0; i < scan_end; i++)
			{
				ESP_LOGI(__func__, "Scanning 0x%04lx", (uint32_t)scan + i);
				if (scan[i] == 0xABCD5432)
				{
					ESP_LOGI(__func__, "Found starter bytes.");
					img_descriptor = (const esp_app_desc_t *)&scan[i];
					break;
				}
			}
			if (!img_descriptor) // Image was not found
			{
				ESP_LOGE(__func__, "Could not find valid image header in first chunk, aborting.");
				free(buf);
				drain_remaining_body(req);
				httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No valid image header in file");
				return ESP_FAIL;
			}
			// Image is found, copy it for later
			memcpy(&image_metadata, img_descriptor, sizeof(esp_app_desc_t));
			image_header_OK = true;
		}

		// If the OTA is somehow not yet started
		if (!OTA_started)
		{
			OTA_err = esp_ota_begin(update_partition, file_size, &OTA_handle);
			if (OTA_err != ESP_OK)
			{
				ESP_LOGE(__func__, "Could not start OTA, aborting.");
				free(buf);
				drain_remaining_body(req);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not start OTA!");
				return ESP_FAIL;
			}
			ESP_LOGI(__func__, "OTA starting...");
			OTA_started = true;
		}
		// Write segment
		OTA_err = esp_ota_write(OTA_handle, (const void *)buf, bytesRetrieved);
		if (OTA_err != ESP_OK)
		{
			ESP_LOGE(__func__, "Could not write OTA segment.");
			free(buf);
			drain_remaining_body(req);
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not write OTA segment.");
			esp_ota_abort(OTA_handle);
			OTA_started = false;
			return ESP_FAIL;
		}

		writtenBytes += bytesRetrieved;
	}
	// Normal exit
	ESP_LOGI(__func__, "Written : %lu bytes, file size target %lu", writtenBytes, file_size);
	free(buf);
	OTA_started = false;

	// Attempt to validate the OTA image written
	OTA_err = esp_ota_end(OTA_handle);
	if (OTA_err != ESP_OK)
	{
		esp_ota_abort(OTA_handle);
		ESP_LOGE(__func__, "Written OTA image could not be validated.");
		// Erase partition here
		OTA_err = esp_partition_erase_range(update_partition, 0, update_partition->size);
		if (OTA_err != ESP_OK)
		{
			ESP_LOGE(__func__, "Could not erase partition %s", update_partition->label);
		}
		drain_remaining_body(req);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
		return ESP_FAIL;
	}
	else
	{
		OTA_err = esp_ota_set_boot_partition(update_partition);
		if (OTA_err != ESP_OK)
		{
			ESP_LOGW(__func__, "Could not set new update partition %s as next boot.", update_partition->label);
		}
	}

	drain_remaining_body(req);

	/* Respond with JSON including app metadata in id "result"*/
	char out[256];
	snprintf(out, sizeof(out), "{\"status\":\"OK\",\"project_name\":\"%s\",\"version\":\"%s\",\"date_time\":\"%s-%s\"}",
			 image_metadata.project_name, image_metadata.version, image_metadata.date, image_metadata.time);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, out);
	return ESP_OK;
}

/// @brief Handler to set the boot partition and reboot on it
/// @param req /set_boot?target= POST from dropdown menu
/// @return
static esp_err_t set_boot_post_handler(httpd_req_t *req)
{
	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);

	char target[32] = {0};
	if (strstr(req->uri, "target=ota_1"))
	{
		strncpy(target, "ota_1", sizeof(target));
	}
	else if (strstr(req->uri, "target=ota_0"))
	{
		strncpy(target, "ota_0", sizeof(target));
	}
	else
	{
		strncpy(target, "factory", sizeof(target));
	}
	ESP_LOGI(__func__, "Boot target : %s", target);
	// Look for that partition
	const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, target);
	if (!part)
	{
		ESP_LOGE(__func__, "Could not find boot target %s", target);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Partition not found");
		return ESP_FAIL;
	}
	esp_err_t err = esp_ota_set_boot_partition(part);
	if (err != ESP_OK)
	{
		ESP_LOGE(__func__, "Set boot failed with code %u", err);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_set_boot_partition failed");
		return ESP_FAIL;
	}
	httpd_resp_sendstr(req, "Boot partition set. Rebooting...");
	vTaskDelay(pdMS_TO_TICKS(500));
	esp_restart();
	return ESP_OK;
}

// /// @brief Allows editing trip and odometer (in meters) in the NVS
// /// @param req POST /set_trip_odo?new_trip= or /set_trip_odo?new_odo=
// /// @return
// static esp_err_t set_trip_odo_post_handler(httpd_req_t *req)
// {
// 	ESP_LOGI(__func__, "Req: %d URI: %s", req->method, req->uri);
// 	nvs_handle_t h;
// 	esp_err_t nvserr = nvs_open_from_partition("nvs_odo", "storage", NVS_READWRITE, &h);
// 	if (nvserr != ESP_OK)
// 	{
// 		ESP_LOGE(__func__, "Could not open nvs_odo -> storage, error %s", esp_err_to_name(nvserr));
// 		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not load nvs_odo NVS");
// 		return ESP_FAIL;
// 	}
// 	// Prepare pointers to substrings
// 	const char *new_trip_ptr = strstr(req->uri, "new_trip=");
// 	const char *new_odo_ptr = strstr(req->uri, "new_odo=");
// 	long detectedNumber = 0;
// 	// Check if "new_trip=" is a valid pointer (detected)
// 	if (new_trip_ptr != NULL)
// 	{
// 		// Construct pointer at the first supposed numerical character
// 		const char *tripToParse = new_trip_ptr + sizeof("new_trip=") - 1;
// 		// atol is not checking anything. Let's check that at least the first character is either numerical or +-
// 		if ((tripToParse[0] == '-') || (tripToParse[0] == '+') || ((tripToParse[0] >= '0') && (tripToParse[0] <= '9')))
// 		{
// 			ESP_LOGI(__func__, "Numeral or compatible detected, atol sorta safe to use.");
// 			detectedNumber = atol(tripToParse);
// 			if (detectedNumber < 0)
// 				detectedNumber *= -1;
// 			if (detectedNumber > UINT32_MAX)
// 				detectedNumber = UINT32_MAX;
// 			nvs_set_u32(h, "trip_m", (uint32_t)detectedNumber);
// 			ESP_LOGI(__func__, "Trip updated to %lu m", (uint32_t)detectedNumber);
// 			nvs_commit(h);
// 		}
// 		else
// 		{
// 			ESP_LOGE(__func__, "Invalid starting character %s for atol, reporting error", tripToParse[0]);
// 			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid characters in trip set request");
// 			nvs_close(h);
// 			return ESP_FAIL;
// 		}
// 	}
// 	if (new_odo_ptr != NULL)
// 	{
// 		const char *odoToParse = new_odo_ptr + sizeof("new_odo=") - 1;
// 		if ((odoToParse[0] == '-') || (odoToParse[0] == '+') || ((odoToParse[0] >= '0') && (odoToParse[0] <= '9')))
// 		{
// 			detectedNumber = atol(odoToParse);
// 			if (detectedNumber < 0)
// 				detectedNumber *= -1;
// 			if (detectedNumber > UINT32_MAX)
// 				detectedNumber = UINT32_MAX;
// 			nvs_set_u32(h, "odometer_m", (uint32_t)detectedNumber);
// 			ESP_LOGI(__func__, "Odometer updated to %lu m", (uint32_t)detectedNumber);
// 			nvs_commit(h);
// 		}
// 		else
// 		{
// 			ESP_LOGE(__func__, "Invalid starting character %s for atol, reporting error", odoToParse[0]);
// 			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid characters in odo set request");
// 			nvs_close(h);
// 			return ESP_FAIL;
// 		}
// 	}
// 	if (new_odo_ptr == NULL && new_trip_ptr == NULL) //Gibberish in the request
// 	{
// 		ESP_LOGE(__func__,"Invalid update request, no new_trip or new_odo.");
// 		httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Malformed request, no correct tags");
// 		nvs_close(h);
// 		return ESP_FAIL;
// 	}
// 	// Close NVS if nothing returned before
// 	nvs_close(h);
// 	httpd_resp_send(req, HTTPD_200, sizeof(HTTPD_200));
// 	return ESP_OK;
// }

/// @brief Web Server initialization handler
/// @param  None
/// @return Handle to webserver if successfully started, NULL otherwise.
static httpd_handle_t start_webserver(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_uri_handlers = 20;
	config.stack_size = 8192;
	httpd_handle_t server = NULL;
	if (httpd_start(&server, &config) != ESP_OK)
	{
		ESP_LOGE(__func__, "Failed to start webserver");
		return NULL;
	}
	httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_get_handler};
	httpd_register_uri_handler(server, &index_uri);

	httpd_uri_t style_uri = {.uri = "/simple.min.css", .method = HTTP_GET, .handler = stylesheet_get_handler};
	httpd_register_uri_handler(server, &style_uri);

	httpd_uri_t version_uri = {.uri = "/version", .method = HTTP_GET, .handler = version_get_handler};
	httpd_register_uri_handler(server, &version_uri);
	httpd_uri_t parts_uri = {.uri = "/partitions", .method = HTTP_GET, .handler = partitions_get_handler};
	httpd_register_uri_handler(server, &parts_uri);
	httpd_uri_t upload_uri = {.uri = "/upload", .method = HTTP_POST, .handler = upload_post_handler};
	httpd_register_uri_handler(server, &upload_uri);
	httpd_uri_t setboot_uri = {.uri = "/set_boot", .method = HTTP_POST, .handler = set_boot_post_handler};
	httpd_register_uri_handler(server, &setboot_uri);
	httpd_uri_t reboot_uri = {.uri = "/reboot", .method = HTTP_POST, .handler = reboot_post_handler};
	httpd_register_uri_handler(server, &reboot_uri);
	// httpd_uri_t odo_uri = {.uri = "/odometer", .method = HTTP_GET, .handler = odometer_get_handler};
	// httpd_register_uri_handler(server, &odo_uri);
	httpd_uri_t bootPart_uri = {.uri = "/prevboot", .method = HTTP_GET, .handler = prevboot_get_handler};
	httpd_register_uri_handler(server, &bootPart_uri);
	// httpd_uri_t set_trip_odo_uri = {.uri = "/set_trip_odo", .method = HTTP_POST, .handler = set_trip_odo_post_handler};
	// httpd_register_uri_handler(server, &set_trip_odo_uri);
	return server;
}

/// @brief Start WIFI AP
/// @param
static void start_ap_mode(void)
{
	esp_netif_create_default_wifi_ap();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);
	wifi_config_t wifi_config = {0};
	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	char ssid[32];
	snprintf(ssid, sizeof(ssid), "RDB-%02X%02X%02X", mac[3], mac[4], mac[5]);
	strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
	wifi_config.ap.ssid_len = strlen(ssid);
	wifi_config.ap.max_connection = 4;
	wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	esp_wifi_set_mode(WIFI_MODE_AP);
	esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
	esp_wifi_start();
	ESP_LOGI(TAG, "Started AP with SSID '%s'", ssid);
}

extern "C" void app_main(void)
{
	ESP_LOGI(__func__, "Init default NVS");
	esp_err_t err = nvs_flash_init();
	if (err != ESP_OK)
		ESP_LOGE(__func__, "Cannot init default NVS");
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_LOGE(__func__, "Could not init default NVS, erasing and retrying.");
		nvs_flash_erase();
		nvs_flash_init();
	}
	// ESP_LOGI(__func__, "Init ODO NVS");
	// err = nvs_flash_init_partition("nvs_odo");
	// if (err != ESP_OK)
	// 	ESP_LOGE(__func__, "Cannot init odo NVS");
	// if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	// {
	// 	ESP_LOGE(__func__, "Could not init nvs_odo NVS, erasing and retrying. - %ld", err);
	// 	nvs_flash_erase_partition("nvs_odo");
	// 	nvs_flash_init_partition("nvs_odo");
	// }

	esp_netif_init();
	esp_event_loop_create_default();

	// Mount SPIFFS
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = "storage",
		.max_files = 5,
		.format_if_mount_failed = true};
	err = esp_vfs_spiffs_register(&conf);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(err));
		return;
	}
	ESP_LOGI(TAG, "SPIFFS mounted successfully");

	start_ap_mode();
	start_mdns();
	start_webserver();
}