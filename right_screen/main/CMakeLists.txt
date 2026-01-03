idf_component_register(SRCS "main.cpp" 
                    INCLUDE_DIRS    "." "../../common"
                    REQUIRES nvs_flash driver esp_timer twai_daemon binocan ui lvgl_v9_port esp_app_format esp_driver_tsens app_update esp_partition            
                    )

