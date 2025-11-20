/*
 * Interface board Factory App
 * - Start as open AP named ITF-<MAC>
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

#ifdef TAG
#undef TAG
#endif
#define TAG "factory_app"

static void start_mdns(void)
{
	mdns_init();
	mdns_hostname_set("interface-board");
	mdns_instance_name_set("Interface Board Factory");
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
	FILE *f = fopen("/spiffs/index.html", "r");
	if (!f)
	{
		ESP_LOGE(TAG, "Failed to open index.html");
		httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
		return ESP_FAIL;
	}

	httpd_resp_set_type(req, "text/html");

	char buf[512];
	while (fgets(buf, sizeof(buf), f))
	{
		httpd_resp_send_chunk(req, buf, strlen(buf));
	}
	httpd_resp_send_chunk(req, NULL, 0);
	fclose(f);
	return ESP_OK;
}

static esp_err_t version_get_handler(httpd_req_t *req)
{
	const esp_app_desc_t *app_desc = esp_app_get_description();
	char buf[256];
	snprintf(buf, sizeof(buf), "{\"project_name\":\"%s\",\"version\":\"%s\",\"date\":\"%s\"}",
			 app_desc ? app_desc->project_name : "", app_desc ? app_desc->version : "", app_desc ? app_desc->date : "");
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, buf);
	return ESP_OK;
}

static esp_err_t partitions_get_handler(httpd_req_t *req)
{
	const char *labels[] = {"nvs", "phy_init", "factory", "ota_0", "ota_1", "ota_data", "storage"};
	const int nlabels = sizeof(labels) / sizeof(labels[0]);
	char buf[1024];
	size_t off = 0;
	off += snprintf(buf + off, sizeof(buf) - off, "{");
	for (int i = 0; i < nlabels; ++i)
	{
		const char *lab = labels[i];
		const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, lab);
		if (!p)
			continue;
		off += snprintf(buf + off, sizeof(buf) - off, "\"%s\":{\"address\":\"0x%08x\",\"size\":%u,\"type\":%d,\"subtype\":%d",
						lab, (unsigned int)p->address, (unsigned int)p->size, p->type, p->subtype);
		if (strcmp(lab, "factory") == 0 || strcmp(lab, "ota_0") == 0 || strcmp(lab, "ota_1") == 0)
		{
			esp_app_desc_t desc;
			if (esp_ota_get_partition_description(p, &desc) == ESP_OK)
			{
				off += snprintf(buf + off, sizeof(buf) - off, " ,\"project_name\":\"%s\",\"version\":\"%s\",\"date\":\"%s\" ",
								desc.project_name, desc.version, desc.date);
			}
		}
		/* close object */
		/* trim trailing comma if last */
		off += snprintf(buf + off, sizeof(buf) - off, "},");
	}
	/* remove trailing comma if present */
	if (off > 1 && buf[off - 1] == ',')
	{
		buf[off - 1] = '\0';
		off--;
	}
	off += snprintf(buf + off, sizeof(buf) - off, "}");
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, buf);
	return ESP_OK;
}

static esp_err_t odometer_get_handler(httpd_req_t *req)
{
	int32_t odometer_m = 0;
	int32_t trip_m = 0;
	nvs_handle_t h;
	esp_err_t err = ESP_FAIL;

	/* Try reading from partition "nvs_odo" with namespace "storage", fall back to default */
	err = nvs_open_from_partition("nvs_odo", "storage", NVS_READONLY, &h);
	if (err != ESP_OK)
	{
		err = nvs_open("storage", NVS_READONLY, &h);
		if (err != ESP_OK)
		{
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
			return ESP_FAIL;
		}
	}
	if (nvs_get_i32(h, "odometer_m", &odometer_m) == ESP_ERR_NVS_NOT_FOUND)
		odometer_m = 0;
	if (nvs_get_i32(h, "trip_m", &trip_m) == ESP_ERR_NVS_NOT_FOUND)
		trip_m = 0;
	nvs_close(h);

	char out[128];
	snprintf(out, sizeof(out), "{\"odometer_m\":%ld,\"trip_m\":%ld}", odometer_m, trip_m);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, out);
	return ESP_OK;
}

static esp_err_t reboot_post_handler(httpd_req_t *req)
{
	httpd_resp_sendstr(req, "Rebooting");
	vTaskDelay(pdMS_TO_TICKS(200));
	esp_restart();
	return ESP_OK;
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
	int remaining = req->content_len;
	if (remaining <= 0)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid content length");
		return ESP_FAIL;
	}
	const size_t buf_size = 4096;
	char *buf = malloc(buf_size);
	if (!buf)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to allocate buffer");
		return ESP_FAIL;
	}

	const esp_partition_t *update_partition = NULL;
	size_t read_total = 0;
	ssize_t r = httpd_req_recv(req, buf, buf_size);
	if (r <= 0)
	{
		free(buf);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to receive request");
		return ESP_FAIL;
	}
	read_total += r;

	char target_label[32] = {0};
	/* Extract target from query string: /upload?target=ota_0 or /upload?target=ota_1 */
	if (strstr(req->uri, "target=ota_1"))
	{
		strncpy(target_label, "ota_1", sizeof(target_label) - 1);
	}
	else if (strstr(req->uri, "target=ota_0"))
	{
		strncpy(target_label, "ota_0", sizeof(target_label) - 1);
	}
	else
	{
		strncpy(target_label, "ota_0", sizeof(target_label) - 1);
	}
	ESP_LOGI(TAG, "Upload target partition: %s", target_label);
	update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, target_label);
	if (!update_partition)
	{
		free(buf);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Target partition not found");
		return ESP_FAIL;
	}

	/* Find start of file body in first chunk */
	char *body_start = strstr(buf, "\r\n\r\n");
	if (body_start)
		body_start += 4;
	else
		body_start = buf;
	size_t to_write = r - (body_start - buf);

	/* Manual app descriptor extraction: scan for magic 0xABCD5432 which is part of the descriptor */
	const esp_app_desc_t *img_desc = NULL;
	if (to_write >= sizeof(esp_app_desc_t))
	{
		uint32_t *scan = (uint32_t *)body_start;
		uint32_t scan_end = (to_write - sizeof(esp_app_desc_t)) / 4;
		for (uint32_t i = 0; i < scan_end; i++)
		{
			if (scan[i] == 0xABCD5432)
			{
				img_desc = (const esp_app_desc_t *)&scan[i];
				break;
			}
		}
	}
	if (!img_desc)
	{
		free(buf);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Uploaded binary contains no app descriptor (rejecting)");
		return ESP_FAIL;
	}

	/* Copy descriptor locally before freeing the upload buffer */
	esp_app_desc_t local_desc;
	memcpy(&local_desc, img_desc, sizeof(esp_app_desc_t));

	/* Begin OTA now that image looks valid */
	esp_ota_handle_t ota_handle = 0;
	esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
	if (err != ESP_OK)
	{
		free(buf);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_begin failed");
		return ESP_FAIL;
	}

	if (to_write > 0)
	{
		esp_ota_write(ota_handle, (const void *)body_start, to_write);
	}
	while (read_total < (size_t)req->content_len)
	{
		int to_read = buf_size;
		if ((size_t)to_read > (size_t)req->content_len - read_total)
			to_read = req->content_len - read_total;
		r = httpd_req_recv(req, buf, to_read);
		if (r <= 0)
			break;
		esp_ota_write(ota_handle, buf, r);
		read_total += r;
	}
	free(buf);
	err = esp_ota_end(ota_handle);
	if (err != ESP_OK)
	{
		esp_ota_abort(ota_handle);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_end failed");
		return ESP_FAIL;
	}

	/* Respond with JSON including app metadata */
	char out[256];
	snprintf(out, sizeof(out), "{\"status\":\"ok\",\"project_name\":\"%s\",\"version\":\"%s\",\"date\":\"%s\"}",
			 local_desc.project_name, local_desc.version, local_desc.date);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, out);
	return ESP_OK;
}

static esp_err_t set_boot_post_handler(httpd_req_t *req)
{
	char target[32] = {0};
	if (strstr(req->uri, "target=ota_1"))
		strncpy(target, "ota_1", sizeof(target));
	else
		strncpy(target, "ota_0", sizeof(target));
	const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, target);
	if (!part)
	{
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Partition not found");
		return ESP_FAIL;
	}
	esp_err_t err = esp_ota_set_boot_partition(part);
	if (err != ESP_OK)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_set_boot_partition failed");
		return ESP_FAIL;
	}
	httpd_resp_sendstr(req, "Boot partition set. Rebooting...");
	vTaskDelay(pdMS_TO_TICKS(500));
	esp_restart();
	return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t server = NULL;
	if (httpd_start(&server, &config) != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to start webserver");
		return NULL;
	}
	httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_get_handler};
	httpd_register_uri_handler(server, &index_uri);
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
	httpd_uri_t odo_uri = {.uri = "/odometer", .method = HTTP_GET, .handler = odometer_get_handler};
	httpd_register_uri_handler(server, &odo_uri);
	return server;
}



static void start_ap_mode(void)
{
	esp_netif_create_default_wifi_ap();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);
	wifi_config_t wifi_config = {0};
	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	char ssid[32];
	snprintf(ssid, sizeof(ssid), "ITF-%02X%02X%02X", mac[3], mac[4], mac[5]);
	strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
	wifi_config.ap.ssid_len = strlen(ssid);
	wifi_config.ap.max_connection = 4;
	wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	esp_wifi_set_mode(WIFI_MODE_AP);
	esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
	esp_wifi_start();
	ESP_LOGI(TAG, "Started AP with SSID '%s'", ssid);
}

void app_main(void)
{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		nvs_flash_erase();
		nvs_flash_init();
	}
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