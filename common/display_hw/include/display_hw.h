/**
 * @file display_hw.h
 * @brief Hardware bring-up for the Waveshare ESP32-S3-Touch-LCD-2.8C display board.
 *
 * Owns the I2C bus, the TCA9554 IO expander, the LEDC backlight, the ST7701 RGB panel
 * (initialised over 3-wire SPI with its CS line on the expander) and the GT911 touch
 * controller. Uses plain ESP-IDF `esp_lcd` drivers only; LVGL glue lives in
 * display_hw_lvgl.h.
 *
 * Pin map and timings are taken verbatim from the ESP32_Display_Panel board definition
 * `BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_2_8_C.h` used before this component existed.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------------------------
 * Panel geometry
 * -------------------------------------------------------------------------------------------*/
#define DISPLAY_HW_H_RES                    (480)   /*!< Horizontal resolution in pixels */
#define DISPLAY_HW_V_RES                    (480)   /*!< Vertical resolution in pixels */
#define DISPLAY_HW_BITS_PER_PIXEL           (16)    /*!< RGB565 on the RGB bus */

/* ---------------------------------------------------------------------------------------------
 * RGB bus (ST7701 refresh panel)
 * -------------------------------------------------------------------------------------------*/
#define DISPLAY_HW_RGB_PCLK_HZ              (18 * 1000 * 1000)
#define DISPLAY_HW_RGB_HPW                  (8)
#define DISPLAY_HW_RGB_HBP                  (10)
#define DISPLAY_HW_RGB_HFP                  (50)
#define DISPLAY_HW_RGB_VPW                  (2)
#define DISPLAY_HW_RGB_VBP                  (18)
#define DISPLAY_HW_RGB_VFP                  (8)
#define DISPLAY_HW_RGB_PCLK_ACTIVE_NEG      (0)     /*!< 0: data latched on rising edge */
#define DISPLAY_HW_RGB_DMA_BURST_SIZE       (64)

#define DISPLAY_HW_RGB_IO_HSYNC             (GPIO_NUM_38)
#define DISPLAY_HW_RGB_IO_VSYNC             (GPIO_NUM_39)
#define DISPLAY_HW_RGB_IO_DE                (GPIO_NUM_40)
#define DISPLAY_HW_RGB_IO_PCLK              (GPIO_NUM_41)
#define DISPLAY_HW_RGB_IO_DISP              (GPIO_NUM_NC)
#define DISPLAY_HW_RGB_IO_DATA0             (GPIO_NUM_5)
#define DISPLAY_HW_RGB_IO_DATA1             (GPIO_NUM_45)
#define DISPLAY_HW_RGB_IO_DATA2             (GPIO_NUM_48)
#define DISPLAY_HW_RGB_IO_DATA3             (GPIO_NUM_47)
#define DISPLAY_HW_RGB_IO_DATA4             (GPIO_NUM_21)
#define DISPLAY_HW_RGB_IO_DATA5             (GPIO_NUM_14)
#define DISPLAY_HW_RGB_IO_DATA6             (GPIO_NUM_13)
#define DISPLAY_HW_RGB_IO_DATA7             (GPIO_NUM_12)
#define DISPLAY_HW_RGB_IO_DATA8             (GPIO_NUM_11)
#define DISPLAY_HW_RGB_IO_DATA9             (GPIO_NUM_10)
#define DISPLAY_HW_RGB_IO_DATA10            (GPIO_NUM_9)
#define DISPLAY_HW_RGB_IO_DATA11            (GPIO_NUM_46)
#define DISPLAY_HW_RGB_IO_DATA12            (GPIO_NUM_3)
#define DISPLAY_HW_RGB_IO_DATA13            (GPIO_NUM_8)
#define DISPLAY_HW_RGB_IO_DATA14            (GPIO_NUM_18)
#define DISPLAY_HW_RGB_IO_DATA15            (GPIO_NUM_17)

/* ---------------------------------------------------------------------------------------------
 * ST7701 control interface (3-wire SPI, bit-banged)
 * -------------------------------------------------------------------------------------------*/
#define DISPLAY_HW_LCD_SPI_IO_SCK           (GPIO_NUM_2)
#define DISPLAY_HW_LCD_SPI_IO_SDA           (GPIO_NUM_1)
#define DISPLAY_HW_LCD_SPI_CS_EXPANDER_PIN  (IO_EXPANDER_PIN_NUM_2)   /*!< CS is on the TCA9554 */

/* ---------------------------------------------------------------------------------------------
 * Shared I2C bus (touch + IO expander)
 * -------------------------------------------------------------------------------------------*/
#define DISPLAY_HW_I2C_PORT                 (0)
#define DISPLAY_HW_I2C_IO_SCL               (GPIO_NUM_7)
#define DISPLAY_HW_I2C_IO_SDA               (GPIO_NUM_15)
#define DISPLAY_HW_I2C_CLK_HZ               (400 * 1000)

/* ---------------------------------------------------------------------------------------------
 * IO expander (TCA9554)
 * -------------------------------------------------------------------------------------------*/
#define DISPLAY_HW_EXPANDER_I2C_ADDRESS     (0x20)
#define DISPLAY_HW_EXPANDER_PIN_TOUCH_RST   (IO_EXPANDER_PIN_NUM_1)
#define DISPLAY_HW_EXPANDER_PIN_BUZZER      (IO_EXPANDER_PIN_NUM_7)

/* ---------------------------------------------------------------------------------------------
 * Touch (GT911)
 * -------------------------------------------------------------------------------------------*/
#define DISPLAY_HW_TOUCH_I2C_ADDRESS        (0x5D)  /*!< Selected by holding INT low during reset */
#define DISPLAY_HW_TOUCH_IO_INT             (GPIO_NUM_16)

/* ---------------------------------------------------------------------------------------------
 * Backlight (LEDC PWM)
 * -------------------------------------------------------------------------------------------*/
#define DISPLAY_HW_BACKLIGHT_IO             (GPIO_NUM_6)
#define DISPLAY_HW_BACKLIGHT_ON_LEVEL       (1)
#define DISPLAY_HW_BACKLIGHT_PWM_FREQ_HZ    (5000)
#define DISPLAY_HW_BACKLIGHT_PWM_RES_BITS   (10)

/**
 * @brief Handles produced by display_hw_init().
 */
typedef struct {
    esp_lcd_panel_handle_t panel;          /*!< ST7701 RGB panel, initialised and switched on */
    esp_lcd_panel_io_handle_t panel_io;    /*!< 3-wire SPI control IO, kept alive for MADCTL mirroring */
    esp_lcd_touch_handle_t touch;          /*!< GT911 handle, NULL if touch init failed */
    esp_io_expander_handle_t expander;     /*!< TCA9554 handle */
} display_hw_handles_t;

/**
 * @brief Bring up backlight, I2C, IO expander, touch and LCD, in that order.
 *
 * The backlight is configured first with 0 % duty and stays off until
 * display_hw_backlight_set() is called. The buzzer pin is driven LOW before it is turned into
 * an output so it cannot chirp at boot.
 *
 * A touch failure is logged and reported as @c ESP_ERR_NOT_FOUND with the LCD still brought
 * up (@c out->touch is NULL), so the caller can decide whether a display without touch is
 * acceptable.
 *
 * @param[in]  num_fbs Number of RGB framebuffers to allocate in PSRAM (1..3). Use
 *                     esp_lv_adapter_get_required_frame_buffer_count() to match the tear mode.
 * @param[out] out     Filled with the created handles.
 *
 * @return
 *      - ESP_OK              everything initialised
 *      - ESP_ERR_NOT_FOUND   LCD up, touch controller failed (out->touch == NULL)
 *      - ESP_ERR_INVALID_ARG num_fbs out of range or out is NULL
 *      - ESP_ERR_INVALID_STATE already initialised
 *      - other               I2C, expander, LEDC or LCD failure; resources created so far are released
 *
 * @note Not thread-safe. Call once from app_main before any other display_hw function.
 * @note Blocks for about 350 ms (GT911 reset, ST7701 sleep-out).
 */
esp_err_t display_hw_init(uint8_t num_fbs, display_hw_handles_t *out);

/**
 * @brief Set backlight brightness.
 *
 * @param[in] percent Brightness in percent, clamped to 0..100.
 *
 * @return
 *      - ESP_OK                on success
 *      - ESP_ERR_INVALID_STATE display_hw_init() has not configured the backlight yet
 *
 * @note Thread-safe and non-blocking (LEDC register write); safe from the FreeRTOS timer task.
 */
esp_err_t display_hw_backlight_set(uint8_t percent);

/**
 * @brief Switch the buzzer on expander pin 7.
 *
 * @param[in] on true drives the pin HIGH (buzzer on), false drives it LOW.
 *
 * @return
 *      - ESP_OK                on success
 *      - ESP_ERR_INVALID_STATE expander not initialised
 *      - other                 I2C error
 *
 * @note Thread-safe through the I2C master bus lock. Performs an I2C read-modify-write and can
 *       block for up to a few hundred microseconds while a touch read holds the bus.
 */
esp_err_t display_hw_buzzer_set(bool on);

#ifdef __cplusplus
}
#endif
