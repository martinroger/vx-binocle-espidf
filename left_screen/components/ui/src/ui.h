#ifndef EEZ_LVGL_UI_GUI_H
#define EEZ_LVGL_UI_GUI_H

#include <lvgl.h>
#include "screens.h"

#ifndef SCREEN_ID_MAIN
#define SCREEN_ID_MAIN 1
#endif


#ifdef __cplusplus
extern "C" {
#endif

void ui_init();
void ui_tick();

void animateTargetArc(lv_obj_t* targetArc, int32_t targetValue);
void animateTargetArcWithDuration(lv_obj_t* targetArc, int32_t targetValue, uint32_t duration);

void loadScreen(enum ScreensEnum screenId,bool animate);



#ifdef __cplusplus
}
#endif

#endif // EEZ_LVGL_UI_GUI_H