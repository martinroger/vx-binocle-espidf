/**
 * @file display_hw.c
 * @brief Waveshare ESP32-S3-Touch-LCD-2.8C bring-up on plain esp_lcd drivers.
 */
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_io_expander_tca9554.h"

#include "display_hw.h"

#if CONFIG_ESP32S3_DATA_CACHE_LINE_64B && !CONFIG_SPIRAM_SPEED_120M && (CONFIG_DISPLAY_HW_BOUNCE_BUFFER_LINES == 0)
#warning "64 B data cache line with PSRAM below 120 MHz and no RGB bounce buffer can make the image drift; keep CONFIG_DISPLAY_HW_BOUNCE_BUFFER_LINES > 0"
#endif

static const char *TAG = "display_hw";

#define BACKLIGHT_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER    LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BACKLIGHT_DUTY_MAX      ((1U << DISPLAY_HW_BACKLIGHT_PWM_RES_BITS) - 1U)

static bool s_backlight_ready = false;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static esp_io_expander_handle_t s_expander = NULL;

/**
 * Vendor initialisation sequence copied byte for byte from the ESP32_Display_Panel board file
 * (BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_2_8_C.h). Keep it static: the ST7701 driver stores the
 * pointer and replays it in esp_lcd_panel_init(). COLMOD 0x66 is intentional (see TOO.MD).
 */
static const st7701_lcd_init_cmd_t s_st7701_init_cmds[] = {
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t []){0x08}, 1, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t []){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t []){0x10, 0x0C}, 2, 0},
    {0xC2, (uint8_t []){0x07, 0x0A}, 2, 0},
    {0xC7, (uint8_t []){0x00}, 1, 0},
    {0xCC, (uint8_t []){0x10}, 1, 0},
    {0xCD, (uint8_t []){0x08}, 1, 0},
    {0xB0, (uint8_t []){0x05, 0x12, 0x98, 0x0E, 0x0F, 0x07, 0x07, 0x09, 0x09, 0x23, 0x05, 0x52, 0x0F, 0x67, 0x2C, 0x11}, 16, 0},
    {0xB1, (uint8_t []){0x0B, 0x11, 0x97, 0x0C, 0x12, 0x06, 0x06, 0x08, 0x08, 0x22, 0x03, 0x51, 0x11, 0x66, 0x2B, 0x0F}, 16, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t []){0x5D}, 1, 0},
    {0xB1, (uint8_t []){0x3E}, 1, 0},
    {0xB2, (uint8_t []){0x81}, 1, 0},
    {0xB3, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x4E}, 1, 0},
    {0xB7, (uint8_t []){0x85}, 1, 0},
    {0xB8, (uint8_t []){0x20}, 1, 0},
    {0xC1, (uint8_t []){0x78}, 1, 0},
    {0xC2, (uint8_t []){0x78}, 1, 0},
    {0xD0, (uint8_t []){0x88}, 1, 0},
    {0xE0, (uint8_t []){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t []){0x06, 0x30, 0x08, 0x30, 0x05, 0x30, 0x07, 0x30, 0x00, 0x33, 0x33}, 11, 0},
    {0xE2, (uint8_t []){0x11, 0x11, 0x33, 0x33, 0xF4, 0x00, 0x00, 0x00, 0xF4, 0x00, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t []){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t []){0x0D, 0xF5, 0x30, 0xF0, 0x0F, 0xF7, 0x30, 0xF0, 0x09, 0xF1, 0x30, 0xF0, 0x0B, 0xF3, 0x30, 0xF0}, 16, 0},
    {0xE6, (uint8_t []){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t []){0x0C, 0xF4, 0x30, 0xF0, 0x0E, 0xF6, 0x30, 0xF0, 0x08, 0xF0, 0x30, 0xF0, 0x0A, 0xF2, 0x30, 0xF0}, 16, 0},
    {0xE9, (uint8_t []){0x36, 0x01}, 2, 0},
    {0xEB, (uint8_t []){0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40}, 7, 0},
    {0xED, (uint8_t []){0xFF, 0x10, 0xAF, 0x76, 0x54, 0x2B, 0xCF, 0xFF, 0xFF, 0xFC, 0xB2, 0x45, 0x67, 0xFA, 0x01, 0xFF}, 16, 0},
    {0xEF, (uint8_t []){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x3A, (uint8_t []){0x66}, 1, 0},
    {0x36, (uint8_t []){0x00}, 1, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x29, (uint8_t []){0x00}, 0, 0},
};

static esp_err_t backlight_init(void)
{
    /* Hold the pin low while LEDC is being configured so the panel never flashes at boot. */
    ESP_RETURN_ON_ERROR(gpio_set_direction(DISPLAY_HW_BACKLIGHT_IO, GPIO_MODE_OUTPUT), TAG, "backlight gpio");
    ESP_RETURN_ON_ERROR(gpio_set_level(DISPLAY_HW_BACKLIGHT_IO, !DISPLAY_HW_BACKLIGHT_ON_LEVEL), TAG, "backlight gpio");

    const ledc_timer_config_t timer_cfg = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = (ledc_timer_bit_t)DISPLAY_HW_BACKLIGHT_PWM_RES_BITS,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = DISPLAY_HW_BACKLIGHT_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "ledc timer");

    const ledc_channel_config_t channel_cfg = {
        .gpio_num = DISPLAY_HW_BACKLIGHT_IO,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = DISPLAY_HW_BACKLIGHT_ON_LEVEL ? 0 : BACKLIGHT_DUTY_MAX,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_cfg), TAG, "ledc channel");
    s_backlight_ready = true;
    return ESP_OK;
}

static esp_err_t i2c_bus_init(void)
{
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = DISPLAY_HW_I2C_PORT,
        .sda_io_num = DISPLAY_HW_I2C_IO_SDA,
        .scl_io_num = DISPLAY_HW_I2C_IO_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
}

static esp_err_t expander_init(void)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_new_i2c_tca9554(s_i2c_bus, DISPLAY_HW_EXPANDER_I2C_ADDRESS, &s_expander),
                        TAG, "tca9554");

    /* The driver reset left every pin as input with output register 0xFF. esp_io_expander_set_level()
     * refuses input pins, so clear the buzzer bit in the output register directly before switching
     * the pin to output; otherwise it would drive HIGH for one I2C transaction. */
    uint32_t out_reg = 0;
    ESP_RETURN_ON_ERROR(s_expander->read_output_reg(s_expander, &out_reg), TAG, "expander out reg");
    out_reg &= ~(uint32_t)DISPLAY_HW_EXPANDER_PIN_BUZZER;
    ESP_RETURN_ON_ERROR(s_expander->write_output_reg(s_expander, out_reg), TAG, "expander out reg");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, DISPLAY_HW_EXPANDER_PIN_BUZZER | DISPLAY_HW_EXPANDER_PIN_TOUCH_RST,
                                                IO_EXPANDER_OUTPUT), TAG, "expander dir");
    return ESP_OK;
}

/* Same sequence as ESP_PANEL_BOARD_TOUCH_PRE_BEGIN_FUNCTION: INT held low through reset selects 0x5D. */
static esp_err_t touch_reset(void)
{
    ESP_RETURN_ON_ERROR(gpio_set_direction(DISPLAY_HW_TOUCH_IO_INT, GPIO_MODE_OUTPUT), TAG, "touch int");
    ESP_RETURN_ON_ERROR(gpio_set_level(DISPLAY_HW_TOUCH_IO_INT, 0), TAG, "touch int");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, DISPLAY_HW_EXPANDER_PIN_TOUCH_RST, 0), TAG, "touch rst");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, DISPLAY_HW_EXPANDER_PIN_TOUCH_RST, 1), TAG, "touch rst");
    vTaskDelay(pdMS_TO_TICKS(CONFIG_DISPLAY_HW_TOUCH_RESET_WAIT_MS));
    ESP_RETURN_ON_ERROR(gpio_set_level(DISPLAY_HW_TOUCH_IO_INT, 1), TAG, "touch int");
    return gpio_reset_pin(DISPLAY_HW_TOUCH_IO_INT);
}

static esp_err_t touch_init(esp_lcd_touch_handle_t *ret_touch)
{
    ESP_RETURN_ON_ERROR(touch_reset(), TAG, "touch reset");

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_cfg.dev_addr = DISPLAY_HW_TOUCH_I2C_ADDRESS;
    tp_io_cfg.scl_speed_hz = DISPLAY_HW_I2C_CLK_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io), TAG, "touch io");

    esp_lcd_touch_io_gt911_config_t gt911_cfg = {
        .dev_addr = DISPLAY_HW_TOUCH_I2C_ADDRESS,
    };
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = DISPLAY_HW_H_RES,
        .y_max = DISPLAY_HW_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,   /* RST is on the expander, handled in touch_reset() */
#if CONFIG_DISPLAY_HW_TOUCH_USE_INTERRUPT
        .int_gpio_num = DISPLAY_HW_TOUCH_IO_INT,
#else
        .int_gpio_num = GPIO_NUM_NC,
#endif
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .driver_data = &gt911_cfg,
    };
    esp_err_t ret = esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, ret_touch);
    if (ret != ESP_OK) {
        esp_lcd_panel_io_del(tp_io);
    }
    return ret;
}

static esp_err_t lcd_init(uint8_t num_fbs, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;

    spi_line_config_t line_cfg = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_expander_pin = DISPLAY_HW_LCD_SPI_CS_EXPANDER_PIN,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = DISPLAY_HW_LCD_SPI_IO_SCK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = DISPLAY_HW_LCD_SPI_IO_SDA,
        .io_expander = s_expander,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg, 0);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_cfg, &io), TAG, "lcd 3-wire io");

    const esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = DISPLAY_HW_RGB_PCLK_HZ,
            .h_res = DISPLAY_HW_H_RES,
            .v_res = DISPLAY_HW_V_RES,
            .hsync_pulse_width = DISPLAY_HW_RGB_HPW,
            .hsync_back_porch = DISPLAY_HW_RGB_HBP,
            .hsync_front_porch = DISPLAY_HW_RGB_HFP,
            .vsync_pulse_width = DISPLAY_HW_RGB_VPW,
            .vsync_back_porch = DISPLAY_HW_RGB_VBP,
            .vsync_front_porch = DISPLAY_HW_RGB_VFP,
            .flags = {
                .pclk_active_neg = DISPLAY_HW_RGB_PCLK_ACTIVE_NEG,
            },
        },
        .data_width = 16,
        .bits_per_pixel = DISPLAY_HW_BITS_PER_PIXEL,
        .num_fbs = num_fbs,
        .bounce_buffer_size_px = DISPLAY_HW_H_RES * CONFIG_DISPLAY_HW_BOUNCE_BUFFER_LINES,
        .dma_burst_size = DISPLAY_HW_RGB_DMA_BURST_SIZE,
        .hsync_gpio_num = DISPLAY_HW_RGB_IO_HSYNC,
        .vsync_gpio_num = DISPLAY_HW_RGB_IO_VSYNC,
        .de_gpio_num = DISPLAY_HW_RGB_IO_DE,
        .pclk_gpio_num = DISPLAY_HW_RGB_IO_PCLK,
        .disp_gpio_num = DISPLAY_HW_RGB_IO_DISP,
        .data_gpio_nums = {
            DISPLAY_HW_RGB_IO_DATA0, DISPLAY_HW_RGB_IO_DATA1, DISPLAY_HW_RGB_IO_DATA2, DISPLAY_HW_RGB_IO_DATA3,
            DISPLAY_HW_RGB_IO_DATA4, DISPLAY_HW_RGB_IO_DATA5, DISPLAY_HW_RGB_IO_DATA6, DISPLAY_HW_RGB_IO_DATA7,
            DISPLAY_HW_RGB_IO_DATA8, DISPLAY_HW_RGB_IO_DATA9, DISPLAY_HW_RGB_IO_DATA10, DISPLAY_HW_RGB_IO_DATA11,
            DISPLAY_HW_RGB_IO_DATA12, DISPLAY_HW_RGB_IO_DATA13, DISPLAY_HW_RGB_IO_DATA14, DISPLAY_HW_RGB_IO_DATA15,
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };
    st7701_vendor_config_t vendor_cfg = {
        .init_cmds = s_st7701_init_cmds,
        .init_cmds_size = sizeof(s_st7701_init_cmds) / sizeof(s_st7701_init_cmds[0]),
        .rgb_config = &rgb_cfg,
        .flags = {
            .mirror_by_cmd = 1,
            .enable_io_multiplex = 0,
        },
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = DISPLAY_HW_BITS_PER_PIXEL,
        .vendor_config = &vendor_cfg,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7701(io, &panel_cfg, &panel), err, TAG, "st7701 panel");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(panel), err, TAG, "panel reset");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(panel), err, TAG, "panel init");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), err, TAG, "panel on");

    *ret_panel = panel;
    *ret_io = io;
    return ESP_OK;

err:
    if (panel) {
        esp_lcd_panel_del(panel);
    }
    if (io) {
        esp_lcd_panel_io_del(io);
    }
    return ret;
}

esp_err_t display_hw_init(uint8_t num_fbs, display_hw_handles_t *out)
{
    ESP_RETURN_ON_FALSE(out && num_fbs >= 1 && num_fbs <= 3, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    ESP_RETURN_ON_FALSE(s_i2c_bus == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialised");
    memset(out, 0, sizeof(*out));
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(backlight_init(), err, TAG, "backlight");
    ESP_GOTO_ON_ERROR(i2c_bus_init(), err, TAG, "i2c bus");
    ESP_GOTO_ON_ERROR(expander_init(), err, TAG, "io expander");

    esp_err_t touch_ret = touch_init(&out->touch);
    if (touch_ret != ESP_OK) {
        ESP_LOGE(TAG, "GT911 init failed (%s), continuing without touch", esp_err_to_name(touch_ret));
        out->touch = NULL;
    }

    ESP_GOTO_ON_ERROR(lcd_init(num_fbs, &out->panel, &out->panel_io), err, TAG, "lcd");
    out->expander = s_expander;

    ESP_LOGI(TAG, "Display up: %dx%d, %u framebuffer(s), bounce %d lines, touch %s",
             DISPLAY_HW_H_RES, DISPLAY_HW_V_RES, num_fbs, CONFIG_DISPLAY_HW_BOUNCE_BUFFER_LINES,
             out->touch ? "ok" : "missing");
    return (out->touch != NULL) ? ESP_OK : ESP_ERR_NOT_FOUND;

err:
    if (out->touch) {
        esp_lcd_touch_del(out->touch);
        out->touch = NULL;
    }
    if (s_expander) {
        esp_io_expander_del(s_expander);
        s_expander = NULL;
    }
    if (s_i2c_bus) {
        i2c_del_master_bus(s_i2c_bus);
        s_i2c_bus = NULL;
    }
    return ret;
}

esp_err_t display_hw_backlight_set(uint8_t percent)
{
    ESP_RETURN_ON_FALSE(s_backlight_ready, ESP_ERR_INVALID_STATE, TAG, "backlight not initialised");
    if (percent > 100) {
        percent = 100;
    }
    uint32_t duty = (BACKLIGHT_DUTY_MAX * percent) / 100U;
    if (!DISPLAY_HW_BACKLIGHT_ON_LEVEL) {
        duty = BACKLIGHT_DUTY_MAX - duty;
    }
    ESP_RETURN_ON_ERROR(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty), TAG, "ledc duty");
    return ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
}

esp_err_t display_hw_buzzer_set(bool on)
{
    ESP_RETURN_ON_FALSE(s_expander, ESP_ERR_INVALID_STATE, TAG, "expander not initialised");
    return esp_io_expander_set_level(s_expander, DISPLAY_HW_EXPANDER_PIN_BUZZER, on ? 1 : 0);
}
