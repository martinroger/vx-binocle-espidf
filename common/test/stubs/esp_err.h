// Host stub of the ESP-IDF error type, for common/test only.
#pragma once
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_TYPE_MISMATCH 0x1104
inline const char *esp_err_to_name(esp_err_t) { return "err"; }
