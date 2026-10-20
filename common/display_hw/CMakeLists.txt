idf_component_register(
    SRCS "src/display_hw.c" "src/display_hw_lvgl.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_gpio
    PRIV_REQUIRES esp_driver_i2c esp_driver_ledc
)
