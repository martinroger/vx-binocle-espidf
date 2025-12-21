#include "styles.h"
#include "images.h"
#include "fonts.h"

#include "ui.h"
#include "screens.h"

//
// Style: screen_dark_setting
//

void init_style_screen_dark_setting_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(0xfffafafa));
    lv_style_set_bg_color(style, lv_color_hex(0xff15171a));
    lv_style_set_line_color(style, lv_color_hex(0xfffafafa));
};

lv_style_t *get_style_screen_dark_setting_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_screen_dark_setting_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_screen_dark_setting(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_screen_dark_setting_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_screen_dark_setting(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_screen_dark_setting_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: scale_white_parts
//

void init_style_scale_white_parts_INDICATOR_DEFAULT(lv_style_t *style) {
    lv_style_set_line_color(style, lv_color_hex(0xfffafafa));
};

lv_style_t *get_style_scale_white_parts_INDICATOR_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_scale_white_parts_INDICATOR_DEFAULT(style);
    }
    return style;
};

void init_style_scale_white_parts_ITEMS_DEFAULT(lv_style_t *style) {
    lv_style_set_line_color(style, lv_color_hex(0xfffafafa));
};

lv_style_t *get_style_scale_white_parts_ITEMS_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_scale_white_parts_ITEMS_DEFAULT(style);
    }
    return style;
};

void add_style_scale_white_parts(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_scale_white_parts_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(obj, get_style_scale_white_parts_ITEMS_DEFAULT(), LV_PART_ITEMS | LV_STATE_DEFAULT);
};

void remove_style_scale_white_parts(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_scale_white_parts_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_remove_style(obj, get_style_scale_white_parts_ITEMS_DEFAULT(), LV_PART_ITEMS | LV_STATE_DEFAULT);
};

//
// Style: arc_white_parts
//

void init_style_arc_white_parts_INDICATOR_DEFAULT(lv_style_t *style) {
    lv_style_set_arc_color(style, lv_color_hex(0xfffafafa));
};

lv_style_t *get_style_arc_white_parts_INDICATOR_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_malloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_arc_white_parts_INDICATOR_DEFAULT(style);
    }
    return style;
};

void add_style_arc_white_parts(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_arc_white_parts_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
};

void remove_style_arc_white_parts(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_arc_white_parts_INDICATOR_DEFAULT(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
};

//
//
//

void add_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*AddStyleFunc)(lv_obj_t *obj);
    static const AddStyleFunc add_style_funcs[] = {
        add_style_screen_dark_setting,
        add_style_scale_white_parts,
        add_style_arc_white_parts,
    };
    add_style_funcs[styleIndex](obj);
}

void remove_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*RemoveStyleFunc)(lv_obj_t *obj);
    static const RemoveStyleFunc remove_style_funcs[] = {
        remove_style_screen_dark_setting,
        remove_style_scale_white_parts,
        remove_style_arc_white_parts,
    };
    remove_style_funcs[styleIndex](obj);
}

