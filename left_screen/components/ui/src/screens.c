#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "styles.h"
#include "ui.h"


#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

void create_screen_main_scr() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main_scr = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_SNAPPABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    {
        lv_obj_t *parent_obj = obj;
        {
            // main_tabview
            lv_obj_t *obj = lv_tabview_create(parent_obj);
            objects.main_tabview = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
            lv_tabview_set_tab_bar_position(obj, LV_DIR_TOP);
            lv_tabview_set_tab_bar_size(obj, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ONE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_CHECKED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // dials_tab
                    lv_obj_t *obj = lv_tabview_add_tab(parent_obj, "dials");
                    objects.dials_tab = obj;
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // rpmScale
                            lv_obj_t *obj = lv_scale_create(parent_obj);
                            objects.rpm_scale = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 480, 480);
                            lv_scale_set_mode(obj, LV_SCALE_MODE_ROUND_INNER);
                            lv_scale_set_range(obj, 0, 8000);
                            lv_scale_set_total_tick_count(obj, 33);
                            lv_scale_set_major_tick_every(obj, 4);
                            lv_scale_set_label_show(obj, true);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_length(obj, 20, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_length(obj, 10, LV_PART_ITEMS | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_width(obj, 3, LV_PART_ITEMS | LV_STATE_DEFAULT);
                        }
                        {
                            // rpm_arc
                            lv_obj_t *obj = lv_arc_create(parent_obj);
                            objects.rpm_arc = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 478, 478);
                            lv_arc_set_range(obj, 0, 8000);
                            lv_arc_set_value(obj, 500);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_rounded(obj, false, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_color(obj, lv_color_hex(0xff00b734), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_opa(obj, 180, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_width(obj, 12, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffff3a3a), LV_PART_INDICATOR | LV_STATE_FOCUSED);
                            lv_obj_set_style_arc_opa(obj, 255, LV_PART_INDICATOR | LV_STATE_FOCUSED);
                            lv_obj_set_style_arc_color(obj, lv_color_hex(0xffffab00), LV_PART_INDICATOR | LV_STATE_CHECKED);
                            lv_obj_set_style_arc_opa(obj, 255, LV_PART_INDICATOR | LV_STATE_CHECKED);
                            lv_obj_set_style_pad_top(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_left(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_right(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffff3a3a), LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_opa(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
                        }
                        {
                            // rpm
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.rpm = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_128px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_FOCUSED);
                            lv_label_set_text(obj, "0000");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 159, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_16px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "RPM");
                        }
                        {
                            // fuelLevel
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.fuel_level = obj;
                            lv_obj_set_pos(obj, 27, 193);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_FOCUSED);
                            lv_label_set_text(obj, "100");
                        }
                        {
                            // coolant
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.coolant = obj;
                            lv_obj_set_pos(obj, 27, 213);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_FOCUSED);
                            lv_label_set_text(obj, "123");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, -28, 193);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_AUTO, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "FUEL");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, -28, 213);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_AUTO, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "COOL");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 52, 193);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_14px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "%");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 52, 213);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_14px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "C");
                        }
                        {
                            // lowFuel_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.low_fuel_tt = obj;
                            lv_obj_set_pos(obj, -30, -100);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_low_fuel);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // overTemperature_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.over_temperature_tt = obj;
                            lv_obj_set_pos(obj, 30, -100);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_over_temperature);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // lowCoolant_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.low_coolant_tt = obj;
                            lv_obj_set_pos(obj, -30, 70);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_low_coolant);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // battery_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.battery_tt = obj;
                            lv_obj_set_pos(obj, 30, 70);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_battery);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // lowOil_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.low_oil_tt = obj;
                            lv_obj_set_pos(obj, -90, 70);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_low_oil);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // mil_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.mil_tt = obj;
                            lv_obj_set_pos(obj, 90, 70);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_mil);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // CAN_state
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.can_state = obj;
                            lv_obj_set_pos(obj, 0, -60);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "CAN");
                        }
                        {
                            // Internal_state
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.internal_state = obj;
                            lv_obj_set_pos(obj, 0, -138);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "DEGRADED");
                        }
                        {
                            // ITF_state
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.itf_state = obj;
                            lv_obj_set_pos(obj, 73, -60);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "ITF");
                        }
                        {
                            // interlock_state
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.interlock_state = obj;
                            lv_obj_set_pos(obj, -73, -60);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "ITLK");
                        }
                    }
                }
                {
                    // settings tab
                    lv_obj_t *obj = lv_tabview_add_tab(parent_obj, "settings");
                    objects.settings_tab = obj;
                    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // override alarm sw
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.override_alarm_sw = obj;
                            lv_obj_set_pos(obj, -127, -173);
                            lv_obj_set_size(obj, 50, 25);
                            lv_obj_add_event_cb(obj, action_set_rpm_alarm_override, LV_EVENT_VALUE_CHANGED, (void *)0);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 28, -173);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Override RPM alarm");
                        }
                        {
                            // rpm_alarm_spinbox
                            lv_obj_t *obj = lv_spinbox_create(parent_obj);
                            objects.rpm_alarm_spinbox = obj;
                            lv_obj_set_pos(obj, -45, -90);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_spinbox_set_digit_format(obj, 5, 0);
                            lv_spinbox_set_range(obj, 0, 10000);
                            lv_spinbox_set_rollover(obj, false);
                            lv_spinbox_set_step(obj, 1);
                            lv_spinbox_set_value(obj, 0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_60px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_MAIN | LV_STATE_DISABLED);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_SELECTED | LV_STATE_DISABLED);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_CURSOR | LV_STATE_DISABLED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff626262), LV_PART_CURSOR | LV_STATE_DISABLED);
                        }
                        {
                            // inc_rpm_alarm_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.inc_rpm_alarm_btn = obj;
                            lv_obj_set_pos(obj, 85, -90);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_add_event_cb(obj, action_inc_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "+");
                                }
                            }
                        }
                        {
                            // dec_rpm_alarm_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.dec_rpm_alarm_btn = obj;
                            lv_obj_set_pos(obj, -175, -90);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_add_event_cb(obj, action_dec_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "-");
                                }
                            }
                        }
                        {
                            // save_rpm_alarm_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.save_rpm_alarm_btn = obj;
                            lv_obj_set_pos(obj, 165, -90);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, 50);
                            lv_obj_add_event_cb(obj, action_save_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "Save");
                                }
                            }
                        }
                        {
                            // blink_alarm_sw
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.blink_alarm_sw = obj;
                            lv_obj_set_pos(obj, -127, -143);
                            lv_obj_set_size(obj, 50, 25);
                            lv_obj_add_event_cb(obj, action_blink_rpm_alarm_toggled, LV_EVENT_VALUE_CHANGED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 7, -143);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Blink RPM alarm");
                        }
                        {
                            // shift_ind_sw
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.shift_ind_sw = obj;
                            lv_obj_set_pos(obj, -127, -25);
                            lv_obj_set_size(obj, 50, 25);
                            lv_obj_add_event_cb(obj, action_shift_indicator_toggled, LV_EVENT_VALUE_CHANGED, (void *)0);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 35, -25);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Use SHIFT indicator");
                        }
                        {
                            // shift_mid_spinbox
                            lv_obj_t *obj = lv_spinbox_create(parent_obj);
                            objects.shift_mid_spinbox = obj;
                            lv_obj_set_pos(obj, 0, 29);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_spinbox_set_digit_format(obj, 5, 0);
                            lv_spinbox_set_range(obj, 1500, 10000);
                            lv_spinbox_set_rollover(obj, false);
                            lv_spinbox_set_step(obj, 1);
                            lv_spinbox_set_value(obj, 0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_60px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_MAIN | LV_STATE_DISABLED);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_SELECTED | LV_STATE_DISABLED);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_CURSOR | LV_STATE_DISABLED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff626262), LV_PART_CURSOR | LV_STATE_DISABLED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffab00), LV_PART_CURSOR | LV_STATE_DEFAULT);
                        }
                        {
                            // inc_shift_mid_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.inc_shift_mid_btn = obj;
                            lv_obj_set_pos(obj, 130, 29);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_add_event_cb(obj, action_inc_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "+");
                                }
                            }
                        }
                        {
                            // dec_shift_mid_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.dec_shift_mid_btn = obj;
                            lv_obj_set_pos(obj, -130, 29);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_add_event_cb(obj, action_dec_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffab00), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "-");
                                }
                            }
                        }
                        {
                            // shift_top_spinbox
                            lv_obj_t *obj = lv_spinbox_create(parent_obj);
                            objects.shift_top_spinbox = obj;
                            lv_obj_set_pos(obj, 0, 100);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_spinbox_set_digit_format(obj, 5, 0);
                            lv_spinbox_set_range(obj, 0, 10000);
                            lv_spinbox_set_rollover(obj, false);
                            lv_spinbox_set_step(obj, 1);
                            lv_spinbox_set_value(obj, 0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_60px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_MAIN | LV_STATE_DISABLED);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_SELECTED | LV_STATE_DISABLED);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffa1a1a1), LV_PART_CURSOR | LV_STATE_DISABLED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff626262), LV_PART_CURSOR | LV_STATE_DISABLED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffff3a3a), LV_PART_CURSOR | LV_STATE_DEFAULT);
                        }
                        {
                            // inc_shift_top_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.inc_shift_top_btn = obj;
                            lv_obj_set_pos(obj, 130, 100);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_add_event_cb(obj, action_inc_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "+");
                                }
                            }
                        }
                        {
                            // dec_shift_top_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.dec_shift_top_btn = obj;
                            lv_obj_set_pos(obj, -130, 100);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_add_event_cb(obj, action_dec_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "-");
                                }
                            }
                        }
                        {
                            // save_shift_ind_btn
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.save_shift_ind_btn = obj;
                            lv_obj_set_pos(obj, -7, 172);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, 50);
                            lv_obj_add_event_cb(obj, action_save_rpm_spinbox, LV_EVENT_CLICKED, (void *)0);
                            lv_obj_add_state(obj, LV_STATE_DISABLED);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "Save");
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 421);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(30));
                            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    // debug tab
                    lv_obj_t *obj = lv_tabview_add_tab(parent_obj, "debug");
                    objects.debug_tab = obj;
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // voltage_lvl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.voltage_lvl = obj;
                            lv_obj_set_pos(obj, 0, -94);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_16px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "99.9V");
                        }
                        {
                            // odometer
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.odometer = obj;
                            lv_obj_set_pos(obj, 0, -130);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "000000");
                        }
                        {
                            // trip
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.trip = obj;
                            lv_obj_set_pos(obj, 0, -110);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_24px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "000.0");
                        }
                        {
                            // airbag_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.airbag_tt = obj;
                            lv_obj_set_pos(obj, 60, 31);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_airbag_srs);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // abs_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.abs_tt = obj;
                            lv_obj_set_pos(obj, 60, -29);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_abs);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // brakes_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.brakes_tt = obj;
                            lv_obj_set_pos(obj, 0, 31);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_brakes);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // parkingbrake_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.parkingbrake_tt = obj;
                            lv_obj_set_pos(obj, -60, 31);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_parking_brake);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xffff3a3a), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // hiBeam_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.hi_beam_tt = obj;
                            lv_obj_set_pos(obj, 0, -29);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_hi_beams);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xff3251ff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // indicators_tt
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.indicators_tt = obj;
                            lv_obj_set_pos(obj, -60, -29);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_turn_indicators);
                            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
                            lv_obj_set_style_image_recolor(obj, lv_color_hex(0xff00b734), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_image_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // version info
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.version_info = obj;
                            lv_obj_set_pos(obj, 0, -186);
                            lv_obj_set_size(obj, 260, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_14px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "v0.0.0-g1234567-dirty");
                        }
                        {
                            // project info
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.project_info = obj;
                            lv_obj_set_pos(obj, 0, -168);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_14px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "______-screen");
                        }
                        {
                            // current_partition
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.current_partition = obj;
                            lv_obj_set_pos(obj, 0, -152);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_14px, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "ota_x");
                        }
                        {
                            // reboot_factory
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.reboot_factory = obj;
                            lv_obj_set_pos(obj, 0, 177);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_add_event_cb(obj, action_reboot_factory, LV_EVENT_RELEASED, (void *)0);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, &ui_font_white_rabbit_14px, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "Factory boot");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    tick_screen_main_scr();
}

void tick_screen_main_scr() {
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main_scr,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main_scr();
}
