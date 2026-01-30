#include "odometer.h"
#include "nvs_storage.h"
#include "esp_log.h"

#define TAG "ODOM"

uint32_t odometer_get(bool *found)
{
    uint32_t value = 0;
    esp_err_t ret = nvs_get_u32_optional(NVS_STORAGE_NAMESPACE, "odometer_m", &value, found);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS: error reading odometer_m (%d)", ret);
        if (found)
            *found = false;
        return 0;
    }
    ESP_LOGD(TAG, "Odometer value found : %lu", value);
    return value;
}

esp_err_t odometer_set(uint32_t val)
{
    return nvs_update_u32_if_different(NVS_STORAGE_NAMESPACE, "odometer_m", val, NULL);
}

uint32_t trip_get(bool *found)
{
    uint32_t value = 0;
    esp_err_t ret = nvs_get_u32_optional(NVS_STORAGE_NAMESPACE, "trip_m", &value, found);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS: error reading trip_m (%d)", ret);
        if (found)
            *found = false;
        return 0;
    }
    ESP_LOGD(TAG, "Trip value found : %lu", value);
    return value;
}

esp_err_t trip_set(uint32_t val)
{
    return nvs_update_u32_if_different(NVS_STORAGE_NAMESPACE, "trip_m", val, NULL);
}
