#pragma once
#include "esphome.h"
#include "driver/mcpwm_prelude.h"
#include <cmath>
#include <cstdint>

namespace esphome {
namespace vehicle_emulator {

static const char *TAG_MCPWM = "VEHICLE_MCPWM";

#define MCPWM_CHAN_COOLANT 0
#define MCPWM_CHAN_RPM     1
#define MCPWM_CHAN_SPEED   2
#define MCPWM_CHAN_COUNT   3

struct McpwmChannelContext {
    int group_id{0};
    mcpwm_timer_handle_t timer{nullptr};
    mcpwm_oper_handle_t oper{nullptr};
    mcpwm_cmpr_handle_t cmpr{nullptr};
    mcpwm_gen_handle_t gen{nullptr};
    uint32_t resolution_hz{0};
    double current_freq_hz{0.0};
    double current_duty_pct{0.0};
    uint32_t period_ticks{0};
    uint32_t cmp_ticks{0};
    bool is_active{false};
    bool is_initialized{false};
};

static McpwmChannelContext mcpwm_channels[MCPWM_CHAN_COUNT];

class McpwmManager {
public:
    static bool init_channel(int chan_idx, int group_id, uint32_t timer_clk_hz, double base_freq_hz,
                             int gpio_num, double duty_pct) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return false;
        if (base_freq_hz <= 0.0) base_freq_hz = 100.0;

        uint32_t period_ticks = static_cast<uint32_t>(std::round(static_cast<double>(timer_clk_hz) / base_freq_hz));
        if (period_ticks > 65535) period_ticks = 65535;
        if (period_ticks < 1) period_ticks = 1;

        uint32_t cmp_ticks = static_cast<uint32_t>(std::round((duty_pct * static_cast<double>(period_ticks)) / 100.0));
        if (cmp_ticks > period_ticks) cmp_ticks = period_ticks;

        McpwmChannelContext &ctx = mcpwm_channels[chan_idx];
        ctx.group_id = group_id;
        ctx.resolution_hz = timer_clk_hz;
        ctx.period_ticks = period_ticks;
        ctx.cmp_ticks = cmp_ticks;
        ctx.current_freq_hz = static_cast<double>(timer_clk_hz) / static_cast<double>(period_ticks);
        ctx.current_duty_pct = (100.0 * static_cast<double>(cmp_ticks)) / static_cast<double>(period_ticks);

        // 1. Timer Config (Immediate shadow updates)
        mcpwm_timer_config_t timer_config = {
            .group_id = group_id,
            .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
            .resolution_hz = timer_clk_hz,
            .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
            .period_ticks = period_ticks,
            .flags = {
                .update_period_on_empty = false,
                .update_period_on_sync = false,
            },
        };
        esp_err_t ret = mcpwm_new_timer(&timer_config, &ctx.timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_MCPWM, "Failed to create timer for chan %d: %s", chan_idx, esp_err_to_name(ret));
            return false;
        }

        // 2. Operator Config
        mcpwm_operator_config_t oper_config = {
            .group_id = group_id,
            .flags = {},
        };
        ret = mcpwm_new_operator(&oper_config, &ctx.oper);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_MCPWM, "Failed to create operator for chan %d: %s", chan_idx, esp_err_to_name(ret));
            return false;
        }

        ret = mcpwm_operator_connect_timer(ctx.oper, ctx.timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_MCPWM, "Failed to connect timer to oper for chan %d: %s", chan_idx, esp_err_to_name(ret));
            return false;
        }

        // 3. Comparator Config
        mcpwm_comparator_config_t cmpr_config = {
            .flags = {
                .update_cmp_on_tez = false,
                .update_cmp_on_tep = false,
                .update_cmp_on_sync = false,
            },
        };
        ret = mcpwm_new_comparator(ctx.oper, &cmpr_config, &ctx.cmpr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_MCPWM, "Failed to create comparator for chan %d: %s", chan_idx, esp_err_to_name(ret));
            return false;
        }

        ret = mcpwm_comparator_set_compare_value(ctx.cmpr, cmp_ticks);
        if (ret != ESP_OK) return false;

        // 4. Generator Config with Inverted Polarity
        mcpwm_generator_config_t gen_config = {
            .gen_gpio_num = gpio_num,
            .flags = {
                .invert_pwm = true,
            },
        };
        ret = mcpwm_new_generator(ctx.oper, &gen_config, &ctx.gen);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_MCPWM, "Failed to create generator for chan %d: %s", chan_idx, esp_err_to_name(ret));
            return false;
        }

        // Actions: HIGH on empty (counter=0), LOW on compare match
        mcpwm_generator_set_action_on_timer_event(
            ctx.gen,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
        mcpwm_generator_set_action_on_compare_event(
            ctx.gen,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, ctx.cmpr, MCPWM_GEN_ACTION_LOW));

        // 5. Enable and Start Timer
        mcpwm_timer_enable(ctx.timer);
        mcpwm_timer_start_stop(ctx.timer, MCPWM_TIMER_START_NO_STOP);

        ctx.is_active = true;
        ctx.is_initialized = true;

        ESP_LOGI(TAG_MCPWM, "MCPWM Chan %d initialized on GPIO %d (Group %d, Freq: %.2f Hz, Duty: %.2f%%)",
                 chan_idx, gpio_num, group_id, ctx.current_freq_hz, ctx.current_duty_pct);
        return true;
    }

    static void pause_channel(int chan_idx) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return;
        McpwmChannelContext &ctx = mcpwm_channels[chan_idx];
        if (!ctx.is_initialized) return;
        mcpwm_generator_set_force_level(ctx.gen, 0, true); // Force LOW
        ctx.is_active = false;
    }

    static void resume_channel(int chan_idx) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return;
        McpwmChannelContext &ctx = mcpwm_channels[chan_idx];
        if (!ctx.is_initialized) return;
        mcpwm_generator_set_force_level(ctx.gen, -1, true); // Normal PWM
        ctx.is_active = true;
    }

    static bool set_frequency(int chan_idx, double freq_hz) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return false;
        McpwmChannelContext &ctx = mcpwm_channels[chan_idx];
        if (!ctx.is_initialized) return false;

        if (freq_hz < 3.0) {
            pause_channel(chan_idx);
            ctx.current_freq_hz = 0.0;
            return true;
        }

        uint32_t period_ticks = static_cast<uint32_t>(std::round(static_cast<double>(ctx.resolution_hz) / freq_hz));
        if (period_ticks > 65535) period_ticks = 65535;
        if (period_ticks < 1) period_ticks = 1;

        ctx.period_ticks = period_ticks;
        ctx.current_freq_hz = static_cast<double>(ctx.resolution_hz) / static_cast<double>(period_ticks);

        mcpwm_timer_set_period(ctx.timer, period_ticks);

        uint32_t cmp_ticks = static_cast<uint32_t>(std::round((ctx.current_duty_pct * static_cast<double>(period_ticks)) / 100.0));
        if (cmp_ticks > period_ticks) cmp_ticks = period_ticks;
        ctx.cmp_ticks = cmp_ticks;
        ctx.current_duty_pct = (100.0 * static_cast<double>(cmp_ticks)) / static_cast<double>(period_ticks);

        mcpwm_comparator_set_compare_value(ctx.cmpr, cmp_ticks);

        if (!ctx.is_active) {
            resume_channel(chan_idx);
        } else {
            mcpwm_generator_set_force_level(ctx.gen, -1, true);
        }
        return true;
    }

    static bool set_duty(int chan_idx, double duty_pct) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return false;
        if (duty_pct > 100.0) duty_pct = 100.0;
        if (duty_pct < 0.0) duty_pct = 0.0;

        McpwmChannelContext &ctx = mcpwm_channels[chan_idx];
        if (!ctx.is_initialized) return false;

        uint32_t cmp_ticks = static_cast<uint32_t>(std::round((duty_pct * static_cast<double>(ctx.period_ticks)) / 100.0));
        if (cmp_ticks > ctx.period_ticks) cmp_ticks = ctx.period_ticks;

        ctx.cmp_ticks = cmp_ticks;
        ctx.current_duty_pct = (100.0 * static_cast<double>(cmp_ticks)) / static_cast<double>(ctx.period_ticks);

        mcpwm_comparator_set_compare_value(ctx.cmpr, cmp_ticks);
        if (ctx.is_active) {
            mcpwm_generator_set_force_level(ctx.gen, -1, true);
        }
        return true;
    }

    static double get_actual_freq(int chan_idx) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return 0.0;
        return mcpwm_channels[chan_idx].current_freq_hz;
    }

    static double get_actual_duty(int chan_idx) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return 0.0;
        return mcpwm_channels[chan_idx].current_duty_pct;
    }

    static bool is_active(int chan_idx) {
        if (chan_idx < 0 || chan_idx >= MCPWM_CHAN_COUNT) return false;
        return mcpwm_channels[chan_idx].is_active;
    }

    // Vehicle specific hardware initialization
    static void init_all(int coolant_pin, int rpm_pin, int speed_pin,
                         double coolant_freq_hz = 100.0, double speed_duty_pct = 33.0, double rpm_duty_pct = 50.0) {
        // Group 0: Coolant (500kHz clock, base coolant_freq_hz, ~35.7% duty for 90 degC)
        init_channel(MCPWM_CHAN_COOLANT, 0, 500000, coolant_freq_hz, coolant_pin, 35.7);

        // Group 0: RPM (500kHz clock, initially paused)
        init_channel(MCPWM_CHAN_RPM, 0, 500000, 100.0, rpm_pin, rpm_duty_pct);
        pause_channel(MCPWM_CHAN_RPM);

        // Group 1: Speed (200kHz clock, initially paused)
        init_channel(MCPWM_CHAN_SPEED, 1, 200000, 100.0, speed_pin, speed_duty_pct);
        pause_channel(MCPWM_CHAN_SPEED);
    }
};

} // namespace vehicle_emulator
} // namespace esphome

