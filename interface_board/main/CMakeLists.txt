idf_component_register(SRCS "main.cpp" 
                    INCLUDE_DIRS "." "../../common"
                    REQUIRES esp_driver_gpio nvs_flash nvs_storage odometer esp_timer twai_daemon binocan esp_app_format esp_driver_tsens app_update esp_partition sma_filter esp_driver_mcpwm esp_driver_gptimer driver)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=cpp")