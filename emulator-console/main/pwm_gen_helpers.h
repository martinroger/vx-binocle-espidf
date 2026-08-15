#pragma once
#include "driver/ledc.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>

#ifdef TAG
#undef TAG
#endif
#define TAG "PWM_GEN"

#define MCPWM_CHANNEL_COOLANT 0
#define MCPWM_CHANNEL_RPM 1
#define MCPWM_CHANNEL_SPEED 2
#define MCPWM_CHANNEL_COUNT 3

typedef struct {
  mcpwm_timer_handle_t timer;
  mcpwm_oper_handle_t oper;
  mcpwm_cmpr_handle_t cmpr;
  mcpwm_gen_handle_t gen;
  uint32_t resolution_hz;
  double current_freq_hz;
  double current_duty_pct;
  uint32_t period_ticks;
  bool is_active;
  bool is_initialized;
} mcpwm_channel_ctx_t;

static mcpwm_channel_ctx_t mcpwm_channels[MCPWM_CHANNEL_COUNT] = {0};

// LEDC tracking for DAC channels (Channels 3 & 4)
static bool active_ledc_timers[8] = {false};
static uint32_t ledc_duty_resolutions_bit[8] = {0};

/* =========================================================================
 *                         MCPWM GENERATOR FUNCTIONS
 * ========================================================================= */

/// @brief Initialize an MCPWM channel with dedicated timer, operator,
/// comparator, and generator
/// @param channel_idx Channel index (0: Coolant, 1: RPM, 2: Speed)
/// @param timer_clk_hz Timer clock frequency in Hz (e.g. 1MHz for Coolant,
/// 500kHz for RPM, 200kHz for Speed)
/// @param base_freq_hz Initial frequency in Hz
/// @param output_gpio GPIO pin to output the PWM signal
/// @param duty_pct Initial duty cycle in percent (0.0 to 100.0)
/// @return ESP_OK on success, error code otherwise
esp_err_t set_mcpwm_generator(int channel_idx, uint32_t timer_clk_hz,
                              double base_freq_hz, gpio_num_t output_gpio,
                              double duty_pct) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    ESP_LOGE(TAG, "Invalid MCPWM channel index: %d", channel_idx);
    return ESP_ERR_INVALID_ARG;
  }

  if (base_freq_hz <= 0.0) {
    base_freq_hz = 100.0;
  }

  uint32_t period_ticks = (uint32_t)round((double)timer_clk_hz / base_freq_hz);
  if (period_ticks > 65535) {
    period_ticks = 65535;
  }
  if (period_ticks < 1) {
    period_ticks = 1;
  }

  mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
  ctx->resolution_hz = timer_clk_hz;
  ctx->period_ticks = period_ticks;
  ctx->current_freq_hz = (double)timer_clk_hz / (double)period_ticks;
  ctx->current_duty_pct = duty_pct;

  // 1. Create Timer
  mcpwm_timer_config_t timer_config = {
      .group_id = 0,
      .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
      .resolution_hz = timer_clk_hz,
      .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
      .period_ticks = period_ticks,
      .flags =
          {
              .update_period_on_empty = true,
              .update_period_on_sync = false,
          },
  };
  esp_err_t ret = mcpwm_new_timer(&timer_config, &ctx->timer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create MCPWM timer for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  // 2. Create Operator
  mcpwm_operator_config_t operator_config = {
      .group_id = 0,
  };
  ret = mcpwm_new_operator(&operator_config, &ctx->oper);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create MCPWM operator for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_operator_connect_timer(ctx->oper, ctx->timer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to connect timer to operator for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  // 3. Create Comparator
  mcpwm_comparator_config_t comparator_config = {
      .flags =
          {
              .update_cmp_on_tez = true,
              .update_cmp_on_tep = false,
              .update_cmp_on_sync = false,
          },
  };
  ret = mcpwm_new_comparator(ctx->oper, &comparator_config, &ctx->cmpr);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create MCPWM comparator for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  uint32_t cmp_ticks =
      (uint32_t)round((duty_pct * (double)period_ticks) / 100.0);
  if (cmp_ticks > period_ticks) {
    cmp_ticks = period_ticks;
  }
  ret = mcpwm_comparator_set_compare_value(ctx->cmpr, cmp_ticks);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set compare value for channel %d: %s", channel_idx,
             esp_err_to_name(ret));
    return ret;
  }

  // 4. Create Generator
  mcpwm_generator_config_t generator_config = {
      .gen_gpio_num = output_gpio,
  };
  ret = mcpwm_new_generator(ctx->oper, &generator_config, &ctx->gen);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create MCPWM generator for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  // 5. Configure Generator actions: HIGH on TEZ (counter=0), LOW on Compare
  // match
  ret = mcpwm_generator_set_action_on_timer_event(
      ctx->gen, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                             MCPWM_TIMER_EVENT_EMPTY,
                                             MCPWM_GEN_ACTION_HIGH));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set timer event action for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_generator_set_action_on_compare_event(
      ctx->gen, MCPWM_GEN_COMPARE_EVENT_ACTION(
                    MCPWM_TIMER_DIRECTION_UP, ctx->cmpr, MCPWM_GEN_ACTION_LOW));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set compare event action for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  // 6. Enable and Start Timer
  ret = mcpwm_timer_enable(ctx->timer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable MCPWM timer for channel %d: %s",
             channel_idx, esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_timer_start_stop(ctx->timer, MCPWM_TIMER_START_NO_STOP);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start MCPWM timer for channel %d: %s", channel_idx,
             esp_err_to_name(ret));
    return ret;
  }

  ctx->is_active = true;
  ctx->is_initialized = true;

  ESP_LOGI(TAG,
           "MCPWM channel %d initialized on GPIO %d (Clk: %lu Hz, Period: %lu "
           "ticks, Freq: %.2f Hz, Duty: %.2f%%)",
           channel_idx, (int)output_gpio, timer_clk_hz, period_ticks,
           ctx->current_freq_hz, duty_pct);

  return ESP_OK;
}

/// @brief Pause an MCPWM channel (stops counter and forces output level to LOW)
esp_err_t pause_mcpwm_channel(int channel_idx) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }
  mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
  if (!ctx->is_initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  mcpwm_generator_set_force_level(ctx->gen, 0, true);
  mcpwm_timer_start_stop(ctx->timer, MCPWM_TIMER_STOP_EMPTY);
  ctx->is_active = false;
  return ESP_OK;
}

/// @brief Resume a paused MCPWM channel
esp_err_t resume_mcpwm_channel(int channel_idx) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }
  mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
  if (!ctx->is_initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  mcpwm_generator_set_force_level(ctx->gen, -1, false);
  mcpwm_timer_start_stop(ctx->timer, MCPWM_TIMER_START_NO_STOP);
  ctx->is_active = true;
  return ESP_OK;
}

/// @brief Update duty cycle of an MCPWM channel (0.0 to 100.0%)
esp_err_t change_mcpwm_duty_cycle(int channel_idx, double duty_pct) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    ESP_LOGE(TAG, "Invalid MCPWM channel %d", channel_idx);
    return ESP_ERR_INVALID_ARG;
  }
  if (duty_pct > 100.0 || duty_pct < 0.0) {
    ESP_LOGE(TAG, "Duty cycle must be between 0 and 100, got %.2f", duty_pct);
    return ESP_ERR_INVALID_ARG;
  }
  mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
  if (!ctx->is_initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  ctx->current_duty_pct = duty_pct;
  uint32_t cmp_ticks =
      (uint32_t)round((duty_pct * (double)ctx->period_ticks) / 100.0);
  if (cmp_ticks > ctx->period_ticks) {
    cmp_ticks = ctx->period_ticks;
  }

  esp_err_t ret = mcpwm_comparator_set_compare_value(ctx->cmpr, cmp_ticks);
  if (ret != ESP_OK) {
    return ret;
  }

  if (ctx->is_active) {
    mcpwm_generator_set_force_level(ctx->gen, -1, false);
  }
  return ESP_OK;
}

/// @brief Update frequency of an MCPWM channel (in Hz)
esp_err_t change_mcpwm_frequency(int channel_idx, double freq_hz) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    ESP_LOGE(TAG, "Invalid MCPWM channel %d", channel_idx);
    return ESP_ERR_INVALID_ARG;
  }
  mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
  if (!ctx->is_initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (freq_hz < 3.0) {
    pause_mcpwm_channel(channel_idx);
    ctx->current_freq_hz = 0;
    return ESP_OK;
  }

  uint32_t period_ticks = (uint32_t)round((double)ctx->resolution_hz / freq_hz);
  if (period_ticks > 65535) {
    period_ticks = 65535;
  }
  if (period_ticks < 1) {
    period_ticks = 1;
  }

  ctx->period_ticks = period_ticks;
  ctx->current_freq_hz = (double)ctx->resolution_hz / (double)period_ticks;

  esp_err_t ret = mcpwm_timer_set_period(ctx->timer, period_ticks);
  if (ret != ESP_OK) {
    return ret;
  }

  // Scale comparator threshold to preserve existing duty cycle
  uint32_t cmp_ticks =
      (uint32_t)round((ctx->current_duty_pct * (double)period_ticks) / 100.0);
  if (cmp_ticks > period_ticks) {
    cmp_ticks = period_ticks;
  }
  mcpwm_comparator_set_compare_value(ctx->cmpr, cmp_ticks);

  if (!ctx->is_active) {
    resume_mcpwm_channel(channel_idx);
  }

  return ESP_OK;
}

bool is_mcpwm_channel_active(int channel_idx) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    return false;
  }
  return mcpwm_channels[channel_idx].is_active;
}

double get_mcpwm_actual_freq(int channel_idx) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    return 0.0;
  }
  return mcpwm_channels[channel_idx].current_freq_hz;
}

double get_mcpwm_actual_duty(int channel_idx) {
  if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
    return 0.0;
  }
  return mcpwm_channels[channel_idx].current_duty_pct;
}

/* =========================================================================
 *                         LEDC GENERATOR FUNCTIONS (DAC)
 * ========================================================================= */

/// @brief Setup function for LEDC PWM generator channels (e.g. Fuel DAC & 12V
/// LV DAC)
esp_err_t set_ledc_generator(ledc_timer_t timer_num, uint32_t base_freq_hz,
                             gpio_num_t output_gpio, ledc_channel_t channel,
                             uint8_t duty_pc,
                             ledc_clk_cfg_t clk_cfg = LEDC_USE_RC_FAST_CLK) {
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = (ledc_timer_bit_t)ledc_find_suitable_duty_resolution(
          SOC_CLK_RC_FAST_FREQ_APPROX, base_freq_hz),
      .timer_num = timer_num,
      .freq_hz = base_freq_hz,
      .clk_cfg = clk_cfg,
      .deconfigure = false};
  ledc_duty_resolutions_bit[(uint32_t)(channel)] =
      (uint32_t)(ledc_timer.duty_resolution);
  if (ledc_timer_config(&ledc_timer) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC timer");
    return ESP_FAIL;
  }

  ledc_channel_config_t ledc_channel = {
      .gpio_num = output_gpio,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = channel,
      .timer_sel = timer_num,
      .duty = (duty_pc *
               (((uint32_t)1 << (uint32_t)(ledc_timer.duty_resolution)) - 1)) /
              100,
      .hpoint = 0,
      .flags = {.output_invert = 1}};
  if (ledc_channel_config(&ledc_channel) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC channel");
    return ESP_FAIL;
  }
  active_ledc_timers[(int)channel] = true;
  ESP_LOGI(TAG, "LEDC channel %d set up on GPIO %d", channel, (int)output_gpio);
  return ESP_OK;
}

esp_err_t change_ledc_duty_cycle(ledc_channel_t channel, double duty_pc) {
  if (channel < LEDC_CHANNEL_0 || channel >= LEDC_CHANNEL_MAX) {
    ESP_LOGE(TAG, "Invalid channel %d", (int)channel);
    return ESP_ERR_INVALID_ARG;
  }
  if (duty_pc > 100.0 || duty_pc < 0.0) {
    ESP_LOGE(TAG, "Duty cycle must be between 0 and 100, got %.2f", duty_pc);
    return ESP_ERR_INVALID_ARG;
  }

  if (!active_ledc_timers[(int)channel]) {
    ledc_timer_resume(LEDC_LOW_SPEED_MODE, (ledc_timer_t)channel);
    active_ledc_timers[(int)channel] = true;
  }

  uint32_t duty =
      (uint32_t)((duty_pc * (((uint32_t)1
                              << ledc_duty_resolutions_bit[(uint32_t)channel]) -
                             1)) /
                 100.0);
  esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
  if (err != ESP_OK) {
    return err;
  }
  return ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

esp_err_t change_ledc_frequency(ledc_channel_t channel, uint32_t freq_hz) {
  ledc_timer_t timer_sel = LEDC_TIMER_3;
  if (freq_hz < 3) {
    ledc_timer_pause(LEDC_LOW_SPEED_MODE, timer_sel);
    active_ledc_timers[(int)channel] = false;
    return ESP_OK;
  } else {
    ledc_timer_resume(LEDC_LOW_SPEED_MODE, timer_sel);
    active_ledc_timers[(int)channel] = true;
    return ledc_set_freq(LEDC_LOW_SPEED_MODE, timer_sel, freq_hz);
  }
}

/* =========================================================================
 *                   UNIFIED WRAPPERS (Channels 0-2 MCPWM, 3-4 LEDC)
 * ========================================================================= */

esp_err_t change_duty_cycle(int channel, double duty_pc) {
  if (channel >= 0 && channel < MCPWM_CHANNEL_COUNT) {
    return change_mcpwm_duty_cycle(channel, duty_pc);
  } else if (channel >= MCPWM_CHANNEL_COUNT && channel < LEDC_CHANNEL_MAX) {
    return change_ledc_duty_cycle((ledc_channel_t)channel, duty_pc);
  }
  return ESP_ERR_INVALID_ARG;
}

esp_err_t change_frequency(int channel, double freq_hz) {
  if (channel >= 0 && channel < MCPWM_CHANNEL_COUNT) {
    return change_mcpwm_frequency(channel, freq_hz);
  } else if (channel >= MCPWM_CHANNEL_COUNT && channel < LEDC_CHANNEL_MAX) {
    return change_ledc_frequency((ledc_channel_t)channel, (uint32_t)freq_hz);
  }
  return ESP_ERR_INVALID_ARG;
}

bool is_channel_active(int channel) {
  if (channel >= 0 && channel < MCPWM_CHANNEL_COUNT) {
    return is_mcpwm_channel_active(channel);
  } else if (channel >= MCPWM_CHANNEL_COUNT && channel < LEDC_CHANNEL_MAX) {
    return active_ledc_timers[channel];
  }
  return false;
}

esp_err_t pause_channel(int channel) {
  if (channel >= 0 && channel < MCPWM_CHANNEL_COUNT) {
    return pause_mcpwm_channel(channel);
  } else if (channel >= MCPWM_CHANNEL_COUNT && channel < LEDC_CHANNEL_MAX) {
    ledc_timer_pause(LEDC_LOW_SPEED_MODE,
                     (ledc_timer_t)(channel < 4 ? channel : 3));
    active_ledc_timers[channel] = false;
    return ESP_OK;
  }
  return ESP_ERR_INVALID_ARG;
}

esp_err_t resume_channel(int channel) {
  if (channel >= 0 && channel < MCPWM_CHANNEL_COUNT) {
    return resume_mcpwm_channel(channel);
  } else if (channel >= MCPWM_CHANNEL_COUNT && channel < LEDC_CHANNEL_MAX) {
    ledc_timer_resume(LEDC_LOW_SPEED_MODE,
                      (ledc_timer_t)(channel < 4 ? channel : 3));
    active_ledc_timers[channel] = true;
    return ESP_OK;
  }
  return ESP_ERR_INVALID_ARG;
}

double get_channel_actual_duty(int channel) {
  if (channel >= 0 && channel < MCPWM_CHANNEL_COUNT) {
    return get_mcpwm_actual_duty(channel);
  } else if (channel >= MCPWM_CHANNEL_COUNT && channel < LEDC_CHANNEL_MAX) {
    return (100.0 *
            ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel)) /
           (((uint32_t)1 << ledc_duty_resolutions_bit[channel]) - 1);
  }
  return 0.0;
}

uint32_t get_channel_actual_freq(int channel) {
  if (channel >= 0 && channel < MCPWM_CHANNEL_COUNT) {
    return (uint32_t)round(get_mcpwm_actual_freq(channel));
  } else if (channel >= MCPWM_CHANNEL_COUNT && channel < LEDC_CHANNEL_MAX) {
    return ledc_get_freq(LEDC_LOW_SPEED_MODE,
                         (ledc_timer_t)(channel < 4 ? channel : 3));
  }
  return 0;
}