idf_component_register(SRCS "nvs_storage.c" "odometer.c" "sma_filter.c" "main.cpp" 
                    INCLUDE_DIRS "." "../../common"
                    REQUIRES nvs_flash driver esp_timer twai_daemon binocan)