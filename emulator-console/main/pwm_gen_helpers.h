#pragma once
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
	int group_id;
	mcpwm_timer_handle_t timer;
	mcpwm_oper_handle_t oper;
	mcpwm_cmpr_handle_t cmpr;
	mcpwm_gen_handle_t gen;
	uint32_t resolution_hz;
	double current_freq_hz;
	double current_duty_pct;
	uint32_t period_ticks;
	uint32_t cmp_ticks;
	bool is_active;
	bool is_initialized;
} mcpwm_channel_ctx_t;

static mcpwm_channel_ctx_t mcpwm_channels[MCPWM_CHANNEL_COUNT] = {0};

/* =========================================================================
 *                         MCPWM GENERATOR FUNCTIONS
 * ========================================================================= */

/// @brief Initialize an MCPWM channel with dedicated timer, operator, comparator, and generator
/// @param channel_idx Channel index (0: Coolant, 1: RPM, 2: Speed)
/// @param group_id MCPWM group ID (0 or 1)
/// @param timer_clk_hz Timer clock frequency in Hz (e.g. 500kHz for Coolant/RPM in Group 0, 200kHz for Speed in Group
/// 1)
/// @param base_freq_hz Initial frequency in Hz
/// @param output_gpio GPIO pin to output the PWM signal
/// @param duty_pct Initial duty cycle in percent (0.0 to 100.0)
/// @return ESP_OK on success, error code otherwise
esp_err_t set_mcpwm_generator(int channel_idx, int group_id, uint32_t timer_clk_hz, double base_freq_hz,
							  gpio_num_t output_gpio, double duty_pct) {
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

	uint32_t cmp_ticks = (uint32_t)round((duty_pct * (double)period_ticks) / 100.0);
	if (cmp_ticks > period_ticks) {
		cmp_ticks = period_ticks;
	}

	mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
	ctx->group_id = group_id;
	ctx->resolution_hz = timer_clk_hz;
	ctx->period_ticks = period_ticks;
	ctx->cmp_ticks = cmp_ticks;
	ctx->current_freq_hz = (double)timer_clk_hz / (double)period_ticks;
	ctx->current_duty_pct = (100.0 * (double)cmp_ticks) / (double)period_ticks;

	// 1. Create Timer
	mcpwm_timer_config_t timer_config = {
		.group_id = group_id,
		.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
		.resolution_hz = timer_clk_hz,
		.count_mode = MCPWM_TIMER_COUNT_MODE_UP,
		.period_ticks = period_ticks,
		.flags =
			{
				.update_period_on_empty = false,
				.update_period_on_sync = false,
			},
	};
	esp_err_t ret = mcpwm_new_timer(&timer_config, &ctx->timer);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create MCPWM timer for channel %d (group %d): %s", channel_idx, group_id,
				 esp_err_to_name(ret));
		return ret;
	}

	// 2. Create Operator
	mcpwm_operator_config_t operator_config = {
		.group_id = group_id,
	};
	ret = mcpwm_new_operator(&operator_config, &ctx->oper);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create MCPWM operator for channel %d (group %d): %s", channel_idx, group_id,
				 esp_err_to_name(ret));
		return ret;
	}

	ret = mcpwm_operator_connect_timer(ctx->oper, ctx->timer);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to connect timer to operator for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	// 3. Create Comparator
	mcpwm_comparator_config_t comparator_config = {
		.flags =
			{
				.update_cmp_on_tez = false,
				.update_cmp_on_tep = false,
				.update_cmp_on_sync = false,
			},
	};
	ret = mcpwm_new_comparator(ctx->oper, &comparator_config, &ctx->cmpr);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create MCPWM comparator for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	ret = mcpwm_comparator_set_compare_value(ctx->cmpr, cmp_ticks);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set compare value for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	// 4. Create Generator
	mcpwm_generator_config_t generator_config = {
		.gen_gpio_num = output_gpio,
		.flags =
			{
				.invert_pwm = true,
			},
	};
	ret = mcpwm_new_generator(ctx->oper, &generator_config, &ctx->gen);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create MCPWM generator for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	// 5. Configure Generator actions: HIGH on TEZ (counter=0), LOW on Compare match
	ret = mcpwm_generator_set_action_on_timer_event(
		ctx->gen,
		MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set timer event action for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	ret = mcpwm_generator_set_action_on_compare_event(
		ctx->gen, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, ctx->cmpr, MCPWM_GEN_ACTION_LOW));
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set compare event action for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	// 6. Enable and Start Timer
	ret = mcpwm_timer_enable(ctx->timer);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to enable MCPWM timer for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	ret = mcpwm_timer_start_stop(ctx->timer, MCPWM_TIMER_START_NO_STOP);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start MCPWM timer for channel %d: %s", channel_idx, esp_err_to_name(ret));
		return ret;
	}

	ctx->is_active = true;
	ctx->is_initialized = true;

	ESP_LOGI(TAG,
			 "MCPWM channel %d initialized on GPIO %d [Group %d] (Clk: %lu Hz, Period: %lu ticks, Freq: %.4f Hz, Duty: "
			 "%.4f%%)",
			 channel_idx, (int)output_gpio, group_id, timer_clk_hz, period_ticks, ctx->current_freq_hz,
			 ctx->current_duty_pct);

	return ESP_OK;
}

/// @brief Pause an MCPWM channel (forces output level to LOW)
esp_err_t pause_mcpwm_channel(int channel_idx) {
	if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
		return ESP_ERR_INVALID_ARG;
	}
	mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
	if (!ctx->is_initialized) {
		return ESP_ERR_INVALID_STATE;
	}

	mcpwm_generator_set_force_level(ctx->gen, 0, true);
	ctx->is_active = false;
	return ESP_OK;
}

/// @brief Resume a paused MCPWM channel (releases forced level back to active PWM)
esp_err_t resume_mcpwm_channel(int channel_idx) {
	if (channel_idx < 0 || channel_idx >= MCPWM_CHANNEL_COUNT) {
		return ESP_ERR_INVALID_ARG;
	}
	mcpwm_channel_ctx_t *ctx = &mcpwm_channels[channel_idx];
	if (!ctx->is_initialized) {
		return ESP_ERR_INVALID_STATE;
	}

	mcpwm_generator_set_force_level(ctx->gen, -1, true);
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

	uint32_t cmp_ticks = (uint32_t)round((duty_pct * (double)ctx->period_ticks) / 100.0);
	if (cmp_ticks > ctx->period_ticks) {
		cmp_ticks = ctx->period_ticks;
	}

	ctx->cmp_ticks = cmp_ticks;
	ctx->current_duty_pct = (100.0 * (double)cmp_ticks) / (double)ctx->period_ticks;

	esp_err_t ret = mcpwm_comparator_set_compare_value(ctx->cmpr, cmp_ticks);
	if (ret != ESP_OK) {
		return ret;
	}

	if (ctx->is_active) {
		mcpwm_generator_set_force_level(ctx->gen, -1, true);
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
		ctx->current_freq_hz = 0.0;
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

	// Scale comparator threshold to preserve existing duty percentage
	uint32_t cmp_ticks = (uint32_t)round((ctx->current_duty_pct * (double)period_ticks) / 100.0);
	if (cmp_ticks > period_ticks) {
		cmp_ticks = period_ticks;
	}
	ctx->cmp_ticks = cmp_ticks;
	ctx->current_duty_pct = (100.0 * (double)cmp_ticks) / (double)period_ticks;

	ret = mcpwm_comparator_set_compare_value(ctx->cmpr, cmp_ticks);
	if (ret != ESP_OK) {
		return ret;
	}

	if (!ctx->is_active) {
		resume_mcpwm_channel(channel_idx);
	} else {
		mcpwm_generator_set_force_level(ctx->gen, -1, true);
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
 *                         CONVENIENCE WRAPPERS
 * ========================================================================= */

esp_err_t change_duty_cycle(int channel, double duty_pc) { return change_mcpwm_duty_cycle(channel, duty_pc); }

esp_err_t change_frequency(int channel, double freq_hz) { return change_mcpwm_frequency(channel, freq_hz); }

bool is_channel_active(int channel) { return is_mcpwm_channel_active(channel); }

esp_err_t pause_channel(int channel) { return pause_mcpwm_channel(channel); }

esp_err_t resume_channel(int channel) { return resume_mcpwm_channel(channel); }

double get_channel_actual_duty(int channel) { return get_mcpwm_actual_duty(channel); }

double get_channel_actual_freq(int channel) { return get_mcpwm_actual_freq(channel); }