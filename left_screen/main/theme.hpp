#pragma once
#include "global_vars.hpp"
#include "lvgl_v9_port.h"
#include "backlight_fader.hpp"
#include <ui.h>
#include <styles.h>

#pragma region COMMON

#pragma endregion

#pragma region SPECIFIC

inline void switch_theme(bool darkMode = headlightsOn, bool getMutex = false, uint16_t fade_duration_ms = 250)
{
	if (getMutex)
	{
		if (!(lvgl_port_lock(-1)))
		{
			ESP_LOGE(__func__, "Could not get port mutex.");
			return;
		}
	}
	if (darkMode)
	{
		add_style_screen_dark_setting(objects.main_tabview);

#ifdef CONFIG_RIGHT_SIDE_DISPLAY
		add_style_scale_white_parts(objects.speed_scale);
		add_style_arc_white_parts(objects.speed_arc);
		lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, true);
#elifdef CONFIG_LEFT_SIDE_DISPLAY
		add_style_scale_white_parts(objects.rpm_scale);
		add_style_arc_white_parts(objects.rpm_arc);
#endif

		display_board_st.lightMode = false;
		set_backlight_brightness_smooth(display_board_st.darkBrightness, fade_duration_ms);
	}
	else
	{
		remove_style_screen_dark_setting(objects.main_tabview);

#ifdef CONFIG_RIGHT_SIDE_DISPLAY
		remove_style_scale_white_parts(objects.speed_scale);
		remove_style_arc_white_parts(objects.speed_arc);
		lv_obj_set_state(objects.theme_switch, LV_STATE_CHECKED, false);
#elifdef CONFIG_LEFT_SIDE_DISPLAY
		remove_style_scale_white_parts(objects.rpm_scale);
		remove_style_arc_white_parts(objects.rpm_arc);
#endif

		display_board_st.lightMode = true;
		set_backlight_brightness_smooth(display_board_st.lightBrightness, fade_duration_ms);
	}
	if (getMutex)
		lvgl_port_unlock();
}

#pragma endregion