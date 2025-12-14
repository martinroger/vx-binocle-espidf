#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "esp_image_format.h"

extern "C" void app_main(void)
{
    // Locate factory partition
    const esp_partition_t * factoryPart = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_FACTORY,"factory");
    if(!factoryPart)
    {
        ESP_LOGE(__func__,"Factory partition could not be found, exiting.");
        return;
    }

    esp_app_desc_t factory_metadata;
    if (esp_ota_get_partition_description(factoryPart,&factory_metadata) == ESP_OK)
    {
        ESP_LOGI(__func__,"Factory partition found, adress 0x%08X, size %lu",factoryPart->address,factoryPart->size);
        ESP_LOGI(__func__,"Project: %s \tVersion: %s \tBuild date-time: %s %s",factory_metadata.project_name,factory_metadata.version, factory_metadata.date,factory_metadata.time);
    }
    else
    {
        ESP_LOGW(__func__,"Could not retrieve factory partition app metadata.");
    }

    return;


}