#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: screen_dark_setting
lv_style_t *get_style_screen_dark_setting_MAIN_DEFAULT();
void add_style_screen_dark_setting(lv_obj_t *obj);
void remove_style_screen_dark_setting(lv_obj_t *obj);

// Style: scale_white_parts
lv_style_t *get_style_scale_white_parts_INDICATOR_DEFAULT();
lv_style_t *get_style_scale_white_parts_ITEMS_DEFAULT();
void add_style_scale_white_parts(lv_obj_t *obj);
void remove_style_scale_white_parts(lv_obj_t *obj);

// Style: arc_white_parts
lv_style_t *get_style_arc_white_parts_INDICATOR_DEFAULT();
void add_style_arc_white_parts(lv_obj_t *obj);
void remove_style_arc_white_parts(lv_obj_t *obj);



#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/