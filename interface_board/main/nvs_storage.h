#pragma once
#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Default namespace used for storage
#define NVS_STORAGE_NAMESPACE "storage"

// Initialize NVS flash. Handles erase-if-needed case.
esp_err_t nvs_init_flash(void);

// Try to read a u32 from NVS. If the key is not found, *found is set to false and ESP_OK is returned.
esp_err_t nvs_get_u32_optional(const char *ns, const char *key, uint32_t *out_val, bool *found);

// Write a u32 to NVS (and commit)
esp_err_t nvs_set_u32_value(const char *ns, const char *key, uint32_t val);

// Update a u32 in NVS only if different. If updated_out != NULL it will be set to true when a write+commit happened.
esp_err_t nvs_update_u32_if_different(const char *ns, const char *key, uint32_t new_val, bool *updated_out);

#ifdef __cplusplus
}
#endif

#endif // NVS_STORAGE_H
