/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_touch_lens.h"

#if IOT_LVGL_SUPPORT && LVGL_TOUCH_FEEDBACK

#ifndef DEBUG_LVGL
#    define DEBUG_LVGL 0
#endif

#if DEBUG_LVGL
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// the whole object is faded in/out, the fill and the rim have their own opacity on top of it
static constexpr uint32_t kLensColor = 0xffffff;
static constexpr lv_opa_t kLensFillOpa = LV_OPA_20;
static constexpr lv_opa_t kLensRimOpa = LV_OPA_40;
static constexpr lv_coord_t kLensRimWidth = 2;

static void _opaExecCb(void *var, int32_t value)
{
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(var), static_cast<lv_opa_t>(value), LV_PART_MAIN);
}

static void _fadeOutReadyCb(lv_anim_t *anim)
{
    lv_obj_add_flag(static_cast<lv_obj_t *>(anim->var), LV_OBJ_FLAG_HIDDEN);
    __LDBG_printf("touch lens hidden");
}

LVGLTouchLens::~LVGLTouchLens()
{
    destroy();
}

bool LVGLTouchLens::_create()
{
    if (_lens) {
        return true;
    }
    auto top = lv_layer_top();
    if (!top) {
        return false;
    }
    _lens = lv_obj_create(top);
    if (!_lens) {
        return false;
    }
    // the lens is below the finger - it must not swallow the touches of the widgets underneath
    lv_obj_clear_flag(_lens, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(_lens, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_lens, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_lens, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_lens, lv_color_hex(kLensColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_lens, kLensFillOpa, LV_PART_MAIN);
    lv_obj_set_style_border_color(_lens, lv_color_hex(kLensColor), LV_PART_MAIN);
    lv_obj_set_style_border_opa(_lens, kLensRimOpa, LV_PART_MAIN);
    lv_obj_set_style_border_width(_lens, kLensRimWidth, LV_PART_MAIN);
    lv_obj_set_style_outline_width(_lens, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_lens, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(_lens, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(_lens, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(_lens, kSize, kSize);
    __LDBG_printf("touch lens created (%dx%d)", static_cast<int>(kSize), static_cast<int>(kSize));
    return true;
}

void LVGLTouchLens::destroy()
{
    if (!_lens) {
        return;
    }
    lv_anim_del(_lens, _opaExecCb);
    lv_obj_del(_lens);
    _lens = nullptr;
}

bool LVGLTouchLens::press(const lv_point_t &point)
{
    if (!_create()) {
        return false;
    }
    lv_obj_clear_flag(_lens, LV_OBJ_FLAG_HIDDEN);
    // force the position to be applied again, the previous touch may have ended at the same point
    _x = -1;
    _setPos(point);
    // fade in from the current level, a press during the fade out does not flash to zero
    _fade(lv_obj_get_style_opa(_lens, LV_PART_MAIN), LV_OPA_COVER, kFadeInTime, nullptr);
    return true;
}

// The lens is a circle that follows the finger. It keeps its shape: an earlier version stretched it
// into a pill in the direction of the drag and grew it with the distance, which drew a rectangle
// that chased the pointer
void LVGLTouchLens::track(const lv_point_t &point)
{
    if (!_lens) {
        return;
    }
    _setPos(point);
}

void LVGLTouchLens::release()
{
    if (!_lens) {
        return;
    }
    _fade(lv_obj_get_style_opa(_lens, LV_PART_MAIN), LV_OPA_TRANSP, kFadeOutTime, _fadeOutReadyCb);
}

void LVGLTouchLens::hide()
{
    if (!_lens) {
        return;
    }
    lv_anim_del(_lens, _opaExecCb);
    lv_obj_add_flag(_lens, LV_OBJ_FLAG_HIDDEN);
}

void LVGLTouchLens::_setPos(const lv_point_t &point)
{
    const auto x = static_cast<lv_coord_t>(point.x - kSize / 2);
    const auto y = static_cast<lv_coord_t>(point.y - kSize / 2);
    if (x == _x && y == _y) {
        return;
    }
    _x = x;
    _y = y;
    lv_obj_set_pos(_lens, x, y);
}

void LVGLTouchLens::_fade(lv_opa_t from, lv_opa_t to, uint32_t time, lv_anim_ready_cb_t ready)
{
    lv_anim_del(_lens, _opaExecCb);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, _lens);
    lv_anim_set_exec_cb(&anim, _opaExecCb);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_time(&anim, time);
    if (ready) {
        lv_anim_set_ready_cb(&anim, ready);
    }
    lv_anim_start(&anim);
}

#endif
