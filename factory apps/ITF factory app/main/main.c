/*
 * Binocan Factory App
 * - Read WiFi creds from NVS (namespace "storage")
 * - Scan for SSID; connect as STA if available
 * - Fallback to open AP named BINOCAN-<MAC>
 * - Start mDNS as hostname "binocan"
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
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
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

static const char *TAG = "factory_app";

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

static const char index_html[] =
"<!doctype html>\n"
"<html><head><meta charset=\"utf-8\"><title>Binocan Factory</title></head><body>"
"<h1>Binocan Factory App</h1>"
"<div id=\"info\">Loading...</div>"
"<h2>Partitions</h2>"
"<button onclick=\"fetch('/partitions').then(r=>r.json()).then(j=>{document.getElementById('info').innerText=JSON.stringify(j,null,2)})\">Refresh</button>"
"<button onclick=\"fetch('/reboot',{method:'POST'}).then(()=>alert('Rebooting'))\">Reboot</button>"
"<h2>Upload Firmware</h2>"
"<form id=\"uploadForm\" method=\"POST\" enctype=\"multipart/form-data\">"
"Target: <select name=\"target\" id=\"target\"><option value=\"ota_0\">ota_0</option><option value=\"ota_1\">ota_1</option></select><br><br>"
"File: <input type=\"file\" name=\"file\" id=\"file\"><br><br>"
"<button type=\"button\" onclick=\"doUpload()\">Upload</button>"
"</form>"
"<progress id=\"uploadProgress\" value=\"0\" max=\"100\" style=\"width:300px;display:block;margin-top:8px\"></progress>"
"<pre id=\"result\"></pre>"
"<h2>Set Boot Partition</h2>"
"<select id=\"bootsel\"><option value=\"ota_0\">ota_0</option><option value=\"ota_1\">ota_1</option></select>"
"<button onclick=\"fetch('/set_boot?target='+document.getElementById('bootsel').value,{method:'POST'}).then(r=>r.text()).then(t=>alert(t))\">Set Boot and Reboot</button>"
"<script>function doUpload(){var f=document.getElementById('file').files[0];if(!f){alert('Choose file');return;}var t=document.getElementById('target').value;var fd=new FormData();fd.append('file',f);var xhr=new XMLHttpRequest();xhr.open('POST','/upload?target='+t,true);xhr.upload.onprogress=function(e){if(e.lengthComputable){document.getElementById('uploadProgress').value=Math.round(e.loaded/e.total*100);}};xhr.onload=function(){document.getElementById('result').innerText=xhr.responseText;document.getElementById('uploadProgress').value=0;};xhr.onerror=function(){alert('Upload failed');document.getElementById('uploadProgress').value=0;};xhr.send(fd);}fetch('/version').then(r=>r.json()).then(j=>{document.getElementById('info').innerText=JSON.stringify(j,null,2)})</script>"
"</body></html>";

static esp_err_t read_wifi_credentials(char *ssid, size_t ssid_len, char *password, size_t pass_len)
{
	nvs_handle_t h;
	esp_err_t err = nvs_open("storage", NVS_READONLY, &h);
	if (err != ESP_OK) return err;
	size_t required = ssid_len;
	err = nvs_get_str(h, "wifi_ssid", ssid, &required);
	if (err != ESP_OK) {
		nvs_close(h);
		return err;
	}
	required = pass_len;
	err = nvs_get_str(h, "wifi_password", password, &required);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		password[0] = '\0';
		err = ESP_OK;
	}
	nvs_close(h);
	return err;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
							   int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) {
		if (event_id == WIFI_EVENT_STA_START) {
			esp_wifi_connect();
		} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
			xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
			ESP_LOGI(TAG, "Disconnected, retrying...");
			esp_wifi_connect();
		}
	} else if (event_base == IP_EVENT) {
		if (event_id == IP_EVENT_STA_GOT_IP) {
			xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
		}
	}
}

static bool scan_for_ssid(const char *ssid)
{
	wifi_scan_config_t scan_config = { .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true };
	esp_err_t err = esp_wifi_scan_start(&scan_config, true);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Scan start failed: %s", esp_err_to_name(err));
		return false;
	}
	uint16_t ap_num = 20;
	wifi_ap_record_t ap_info[20];
	err = esp_wifi_scan_get_ap_records(&ap_num, ap_info);
	if (err != ESP_OK) return false;
	for (int i = 0; i < ap_num; ++i) {
		if (strcmp((char*)ap_info[i].ssid, ssid) == 0) {
			ESP_LOGI(TAG, "Found SSID %s", ssid);
			return true;
		}
	}
	return false;
}

static void start_mdns(void)
{
	mdns_init();
	mdns_hostname_set("binocan");
	mdns_instance_name_set("Binocan Factory");
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
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
	const char *labels[] = { "nvs", "phy_init", "factory", "ota_0", "ota_1", "ota_data", "storage" };
	const int nlabels = sizeof(labels) / sizeof(labels[0]);
	char buf[1024];
	size_t off = 0;
	off += snprintf(buf + off, sizeof(buf) - off, "{");
	for (int i = 0; i < nlabels; ++i) {
		const char *lab = labels[i];
		const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, lab);
		if (!p) continue;
		off += snprintf(buf + off, sizeof(buf) - off, "\"%s\":{\"address\":\"0x%08x\",\"size\":%u,\"type\":%d,\"subtype\":%d",
				lab, (unsigned int)p->address, (unsigned int)p->size, p->type, p->subtype);
		if (strcmp(lab, "ota_0") == 0 || strcmp(lab, "ota_1") == 0) {
			esp_app_desc_t desc;
			if (esp_ota_get_partition_description(p, &desc) == ESP_OK) {
				off += snprintf(buf + off, sizeof(buf) - off, " ,\"project_name\":\"%s\",\"version\":\"%s\",\"date\":\"%s\" ",
								desc.project_name, desc.version, desc.date);
			}
		}
		/* close object */
		/* trim trailing comma if last */
		off += snprintf(buf + off, sizeof(buf) - off, "},");
	}
	/* remove trailing comma if present */
	if (off > 1 && buf[off-1] == ',') {
		buf[off-1] = '\0';
		off--;
	}
	off += snprintf(buf + off, sizeof(buf) - off, "}");
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, buf);
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
	if (remaining <= 0) {
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid content length");
		return ESP_FAIL;
	}
	const size_t buf_size = 4096;
	char *buf = malloc(buf_size);
	if (!buf) {
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to allocate buffer");
		return ESP_FAIL;
	}

	const esp_partition_t *update_partition = NULL;
	size_t read_total = 0;
	ssize_t r = httpd_req_recv(req, buf, buf_size);
	if (r <= 0) {
		free(buf);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to receive request");
		return ESP_FAIL;
	}
	read_total += r;

	char target_label[32] = {0};
	/* Extract target from query string: /upload?target=ota_0 or /upload?target=ota_1 */
	if (strstr(req->uri, "target=ota_1")) {
		strncpy(target_label, "ota_1", sizeof(target_label) - 1);
	} else if (strstr(req->uri, "target=ota_0")) {
		strncpy(target_label, "ota_0", sizeof(target_label) - 1);
	} else {
		strncpy(target_label, "ota_0", sizeof(target_label) - 1);
	}
	ESP_LOGI(TAG, "Upload target partition: %s", target_label);
	update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, target_label);
	if (!update_partition) {
		free(buf);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Target partition not found");
		return ESP_FAIL;
	}

	/* Find start of file body in first chunk */
	char *body_start = strstr(buf, "\r\n\r\n");
	if (body_start) body_start += 4; else body_start = buf;
	size_t to_write = r - (body_start - buf);

	/* Manual app descriptor extraction: scan for magic 0xABCD5432 which precedes the descriptor */
	const esp_app_desc_t *img_desc = NULL;
	if (to_write >= sizeof(esp_app_desc_t)) {
		uint32_t *scan = (uint32_t *)body_start;
		uint32_t scan_end = (to_write - sizeof(esp_app_desc_t)) / 4;
		for (uint32_t i = 0; i < scan_end; i++) {
			if (scan[i] == 0xABCD5432) {
				img_desc = (const esp_app_desc_t *)&scan[i + 1];
				break;
			}
		}
	}
	if (!img_desc) {
		free(buf);
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Uploaded binary contains no app descriptor (rejecting)");
		return ESP_FAIL;
	}

	/* Begin OTA now that image looks valid */
	esp_ota_handle_t ota_handle = 0;
	esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
	if (err != ESP_OK) {
		free(buf);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_begin failed");
		return ESP_FAIL;
	}

	if (to_write > 0) {
		esp_ota_write(ota_handle, (const void*)body_start, to_write);
	}
	while (read_total < (size_t)req->content_len) {
		int to_read = buf_size;
		if ((size_t)to_read > (size_t)req->content_len - read_total) to_read = req->content_len - read_total;
		r = httpd_req_recv(req, buf, to_read);
		if (r <= 0) break;
		esp_ota_write(ota_handle, buf, r);
		read_total += r;
	}
	free(buf);
	err = esp_ota_end(ota_handle);
	if (err != ESP_OK) {
		esp_ota_abort(ota_handle);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_end failed");
		return ESP_FAIL;
	}

	/* Respond with JSON including app metadata */
	char out[256];
	snprintf(out, sizeof(out), "{\"status\":\"ok\",\"project_name\":\"%s\",\"version\":\"%s\",\"date\":\"%s\"}",
			 img_desc->project_name, img_desc->version, img_desc->date);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, out);
	return ESP_OK;
}

static esp_err_t set_boot_post_handler(httpd_req_t *req)
{
	char target[32] = {0};
	if (strstr(req->uri, "target=ota_1")) strncpy(target, "ota_1", sizeof(target));
	else strncpy(target, "ota_0", sizeof(target));
	const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, target);
	if (!part) {
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Partition not found");
		return ESP_FAIL;
	}
	esp_err_t err = esp_ota_set_boot_partition(part);
	if (err != ESP_OK) {
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
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start webserver");
		return NULL;
	}
	httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
	httpd_register_uri_handler(server, &index_uri);
	httpd_uri_t version_uri = { .uri = "/version", .method = HTTP_GET, .handler = version_get_handler };
	httpd_register_uri_handler(server, &version_uri);
	httpd_uri_t parts_uri = { .uri = "/partitions", .method = HTTP_GET, .handler = partitions_get_handler };
	httpd_register_uri_handler(server, &parts_uri);
	httpd_uri_t upload_uri = { .uri = "/upload", .method = HTTP_POST, .handler = upload_post_handler };
	httpd_register_uri_handler(server, &upload_uri);
	httpd_uri_t setboot_uri = { .uri = "/set_boot", .method = HTTP_POST, .handler = set_boot_post_handler };
	httpd_register_uri_handler(server, &setboot_uri);
	httpd_uri_t reboot_uri = { .uri = "/reboot", .method = HTTP_POST, .handler = reboot_post_handler };
	httpd_register_uri_handler(server, &reboot_uri);
	return server;
}

static bool try_connect_sta(const char *ssid, const char *password)
{
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);
	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
	esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
	wifi_config_t wifi_config = {0};
	strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)-1);
	if (password && password[0]) strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password)-1);
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	esp_wifi_start();
	if (!scan_for_ssid(ssid)) {
		ESP_LOGW(TAG, "SSID %s not found in scan", ssid);
		return false;
	}
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "Connected to AP %s", ssid);
		return true;
	}
	ESP_LOGW(TAG, "Failed to get IP from AP");
	return false;
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
	snprintf(ssid, sizeof(ssid), "BINOCAN-%02X%02X%02X", mac[3], mac[4], mac[5]);
	strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid)-1);
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
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		nvs_flash_erase();
		nvs_flash_init();
	}
	esp_netif_init();
	esp_event_loop_create_default();
	s_wifi_event_group = xEventGroupCreate();

	char ssid[64] = {0};
	char password[64] = {0};
	if (read_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK) {
		ESP_LOGI(TAG, "Read credentials SSID='%s' pwd_len=%d", ssid, (int)strlen(password));
		if (try_connect_sta(ssid, password)) {
			start_mdns();
			start_webserver();
			return;
		}
	} else {
		ESP_LOGI(TAG, "No WiFi credentials in NVS");
	}
	start_ap_mode();
	start_mdns();
	start_webserver();
}