#pragma once
#include "global_vars.hpp"
#include "lvgl_v9_port.h"
#include <ui.h>
#include <styles.h>

#pragma region COMMON

#pragma endregion

#pragma region SPECIFIC

inline void switch_theme(bool darkMode = headlightsOn, bool getMutex = false)
{
if(getMutex)
{
	if(!(lvgl_port_lock(-1)))
	{
		ESP_LOGE(__func__,"Could not get port mutex.");
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
		display_board_st.backLight->setBrightness(display_board_st.darkBrightness);
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
		display_board_st.backLight->setBrightness(display_board_st.lightBrightness);
	}
	if(getMutex)
		lvgl_port_unlock();
}

#pragma endregion