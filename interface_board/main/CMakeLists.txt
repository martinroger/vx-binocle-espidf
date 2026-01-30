idf_component_register(SRCS "main.cpp" 
                    INCLUDE_DIRS "." "../../common"
                    REQUIRES nvs_flash nvs_storage odometer driver esp_timer twai_daemon binocan esp_app_format esp_driver_tsens app_update esp_partition sma_filter)