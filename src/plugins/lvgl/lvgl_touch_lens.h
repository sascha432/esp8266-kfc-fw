/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Touch feedback of the screen manager: a soft translucent circle follows the finger while
// the panel is touched.
//
// The lens lives on the top layer of the display, above the widget tree of the active screen,
// so it survives a screen change. It is not clickable, the touches keep going to the widgets
// below it.
//
// Why not a blur: LVGL 8.4 has no blur of its own. The only way is a snapshot of the screen
// into a PSRAM buffer plus lv_canvas_blur_hor/ver on a canvas (the LVGL forum topic "Is it
// possible to implement a Blur filter in LVGL?"), which is what the debug screenshot uses
// (DEBUG_LVGL_SCREENSHOT). That means a full screen render (480x320 = 300 KB PSRAM) on every
// press and re-blur of the whole lens area on every move - a 112x112 px lens costs ~1.4
// million pixel operations per axis and frame on a 240 MHz ESP32 without a GPU. Blur is only
// usable for a static area, not for a lens that follows a finger. Every touch UI on this class
// of hardware therefore uses a translucent indicator instead (the Material touch ripple).
//
// The frosted look comes from the semi transparent fill plus the rim, which costs nothing:
// one object, no per frame computation while the finger moves.

#include <Arduino_compat.h>

#if IOT_LVGL_SUPPORT

#include <lvgl.h>

// 0 turns the touch feedback off (it is used by all screens, so it is switched here)
#ifndef LVGL_TOUCH_FEEDBACK
#    define LVGL_TOUCH_FEEDBACK 1
#endif

#if LVGL_TOUCH_FEEDBACK

class LVGLTouchLens {
public:
    // diameter of the circle (it never changes, the lens only moves)
    static constexpr lv_coord_t kSize = 64;
    static constexpr uint32_t kFadeInTime = 120;  // milliseconds
    static constexpr uint32_t kFadeOutTime = 160;  // milliseconds

    LVGLTouchLens() = default;
    ~LVGLTouchLens();

    // the panel was touched, creates the lens on the top layer on the first call
    bool press(const lv_point_t &point);
    // the touch position changed, the circle follows the finger
    void track(const lv_point_t &point);
    // the panel was released, the lens fades out and hides itself
    void release();
    // hides the lens without an animation (used when the screen changes below the finger)
    void hide();
    // removes the object from the top layer
    void destroy();

    bool isVisible() const {
        return _lens && !lv_obj_has_flag(_lens, LV_OBJ_FLAG_HIDDEN);
    }

private:
    bool _create();
    void _setPos(const lv_point_t &point);
    void _fade(lv_opa_t from, lv_opa_t to, uint32_t time, lv_anim_ready_cb_t ready);

private:
    lv_obj_t *_lens{nullptr};
    lv_coord_t _x{-1};
    lv_coord_t _y{-1};
};

#endif
#endif
