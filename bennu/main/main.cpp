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
    // Get the payload data start, end and size, and report it
    extern const unsigned char payload_bin_start[] asm("_binary_payload_bin_start");
    extern const unsigned char payload_bin_end[] asm("_binary_payload_bin_end");
    const ssize_t payload_bin_size = (payload_bin_end - payload_bin_start);
    ESP_LOGI(__func__, "Payload size : %u bytes", payload_bin_size);

    // Get the storage data start, end and size, and report it
    extern const unsigned char storage_bin_start[] asm("_binary_storage_bin_start");
    extern const unsigned char storage_bin_end[] asm("_binary_storage_bin_end");
    const ssize_t storage_bin_size = (storage_bin_end - storage_bin_start);
    ESP_LOGI(__func__, "Storage size : %u bytes", storage_bin_size);

    // Locate factory partition
    const esp_partition_t *factoryPart = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
    if (!factoryPart)
    {
        ESP_LOGE(__func__, "Factory partition could not be found, exiting.");
        return;
    }
    // Check this is not the current partition
    const esp_partition_t *currentPart = esp_ota_get_running_partition();
    if (currentPart->address == factoryPart->address)
    {
        ESP_LOGE(__func__, "Current running partition is destination partition, abort !");
        return;
    }

    // Locate storage partition
    const esp_partition_t *storagePart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "storage");
    if (!storagePart)
    {
        ESP_LOGE(__func__, "Storage partition could not be found, exiting.");
        return;
    }

    // Get metadata of factory if it exists
    esp_app_desc_t factory_metadata;
    if (esp_ota_get_partition_description(factoryPart, &factory_metadata) == ESP_OK)
    {
        ESP_LOGI(__func__, "Factory partition found, adress 0x%08X, size %lu", factoryPart->address, factoryPart->size);
        ESP_LOGI(__func__, "Project: %s \tVersion: %s \tBuild date-time: %s %s",
                 factory_metadata.project_name, factory_metadata.version, factory_metadata.date, factory_metadata.time);
    }
    else
    {
        ESP_LOGW(__func__, "Could not retrieve factory partition app metadata.");
    }

    // Get metadata of payload if it exists
    uint32_t *scan = (uint32_t *)payload_bin_start; // Start scanning from edge of payload
    uint32_t scan_end = 1024;                       // Scan the first 1024x4 bytes
    const esp_app_desc_t *img_descriptor = NULL;    // Pointer to start of image descriptor
    esp_app_desc_t image_metadata;                  // Final holding structure of image metadata
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
    if (!img_descriptor)
    {
        ESP_LOGW(__func__, "Could not find valid image metadata in payload, proceed at own risks");
    }
    else
    {
        mempcpy(&image_metadata, img_descriptor, sizeof(esp_app_desc_t));
        ESP_LOGI(__func__, "Image metadata found.\nProject: %s \tVersion: %s \tBuild date-time: %s %s",
                 image_metadata.project_name, image_metadata.version, image_metadata.date, image_metadata.time);
    }

    // Proceed with deployment to factory partition
    // First erase
    esp_err_t deploy_err;
    deploy_err = esp_partition_erase_range(factoryPart, 0, factoryPart->size);
    if (deploy_err != ESP_OK)
    {
        ESP_LOGE(__func__, "Could not erase the factory partition : %s", esp_err_to_name(deploy_err));
        return;
    }
    ESP_LOGI(__func__, "Commencing write to factory partition...");
    deploy_err = esp_partition_write(factoryPart, 0, (void *)payload_bin_start, payload_bin_size);
    if (deploy_err != ESP_OK)
    {
        ESP_LOGE(__func__, "Could not write to the factory partition : %s", esp_err_to_name(deploy_err));
        return;
    }
    ESP_LOGI(__func__, "Write operation finished, marking current partition as invalid");
    deploy_err = esp_ota_mark_app_invalid_rollback();
    if (deploy_err != ESP_OK)
    {
        ESP_LOGW(__func__, "Invalidation failed : %s", deploy_err);
    }
    ESP_LOGI(__func__, "Setting factory as boot partition");
    deploy_err = esp_ota_set_boot_partition(factoryPart);
    if (deploy_err != ESP_OK)
    {
        ESP_LOGE(__func__, "Could not set factory as the boot partition. Not rebooting.");
        return;
    }

    // Then do the same to the storage partition
    if (storage_bin_size > 0)
    {
        deploy_err = esp_partition_erase_range(storagePart, 0, storagePart->size);
        if (deploy_err != ESP_OK)
        {
            ESP_LOGE(__func__, "Could not erase the storage partition : %s", esp_err_to_name(deploy_err));
            return;
        }
        ESP_LOGI(__func__, "Commencing write to storage partition...");
        deploy_err = esp_partition_write(storagePart, 0, (void *)storage_bin_start, storage_bin_size);
        if (deploy_err != ESP_OK)
        {
            ESP_LOGE(__func__, "Could not write to the storage partition : %s", esp_err_to_name(deploy_err));
        }
        else
            ESP_LOGI(__func__, "Storage partition written OK");
    }
    else
        ESP_LOGW(__func__, "Storage bin contains no data, skipping rewrite...");

    ESP_LOGI(__func__, "Boot partition set to factory, reboot in 5...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(__func__, "...4...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(__func__, "   ...3...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(__func__, "      ...2...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(__func__, "         ...1...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_restart();
}