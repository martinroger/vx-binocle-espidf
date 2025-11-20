idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_netif esp_event esp_wifi mdns esp_http_server app_update spiffs)

spiffs_create_partition_image(storage web FLASH_IN_PROJECT)