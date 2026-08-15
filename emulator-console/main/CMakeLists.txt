idf_component_register(SRCS "console_example_main.cpp"
                       PRIV_REQUIRES fatfs driver esp_driver_mcpwm
                       INCLUDE_DIRS "." "../../common")
