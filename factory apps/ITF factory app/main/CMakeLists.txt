idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "." "../../../interface_board/main" "../../../common"
                    REQUIRES nvs_flash esp_netif esp_event esp_wifi mdns esp_http_server app_update spiffs binocan twai_daemon esp_driver_twai driver esp_timer esp_driver_mcpwm esp_driver_gptimer)

spiffs_create_partition_image(storage web FLASH_IN_PROJECT)