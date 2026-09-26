/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_test_screen.h"

#if IOT_LVGL_SUPPORT

#include <lvgl.h>
#include <Arduino.h>
#include <esp32-hal-psram.h>
#include "devices/wt32_sc01/wt32_sc01.h"

static constexpr lv_coord_t kWidth = IOT_WT32_SC01_TFT_WIDTH;
static constexpr lv_coord_t kHeight = IOT_WT32_SC01_TFT_HEIGHT;

static uint32_t _pressCount = 0;
static int32_t _lastX = -1;
static int32_t _lastY = -1;

static lv_obj_t *_pressLabel = nullptr;
static lv_obj_t *_pointLabel = nullptr;

// 1 pixel checkerboard. The panel only resolves single pixels if a fine black/white
// grid is visible - an even gray field means the image is soft, scaled or band limited.
// It is also the reference for tuning the SPI clock (speckle/noise shows up first here).
static constexpr lv_coord_t kCheckerHeight = 22;
static lv_color_t *_checkerPixels = nullptr;
static lv_img_dsc_t _checkerImg;

static void _addCheckerboard(lv_obj_t *parent, lv_coord_t y)
{
    if (!_checkerPixels) {
        _checkerPixels = static_cast<lv_color_t *>(ps_malloc(kWidth * kCheckerHeight * sizeof(lv_color_t)));
        if (!_checkerPixels) {
            return;
        }
        for (lv_coord_t py = 0; py < kCheckerHeight; py++) {
            for (lv_coord_t px = 0; px < kWidth; px++) {
                _checkerPixels[py * kWidth + px] = ((px + py) & 1) ? lv_color_white() : lv_color_black();
            }
        }
        _checkerImg = {};
        _checkerImg.header.cf = LV_IMG_CF_TRUE_COLOR;
        _checkerImg.header.w = kWidth;
        _checkerImg.header.h = kCheckerHeight;
        _checkerImg.data_size = static_cast<uint32_t>(sizeof(lv_color_t)) * kWidth * kCheckerHeight;
        _checkerImg.data = reinterpret_cast<const uint8_t *>(_checkerPixels);
    }

    auto img = lv_img_create(parent);
    lv_img_set_src(img, &_checkerImg);
    lv_obj_set_pos(img, 0, y);
}

static void _updateLabels()
{
    if (_pressLabel) {
        lv_label_set_text_fmt(_pressLabel, "Presses: %u", static_cast<unsigned>(_pressCount));
    }
    if (_pointLabel) {
        if (_pressCount) {
            lv_label_set_text_fmt(_pointLabel, "Last point: %d, %d", static_cast<int>(_lastX), static_cast<int>(_lastY));
        }
        else {
            lv_label_set_text(_pointLabel, "Touch the blue area");
        }
    }
}

static void _touchEventCb(lv_event_t *event)
{
    auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        _pressCount++;
    }
    auto indev = lv_indev_get_act();
    if (indev) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        _lastX = point.x;
        _lastY = point.y;
    }
    _updateLabels();
}

static lv_obj_t *_addColorBar(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, uint32_t color)
{
    auto obj = lv_obj_create(parent);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_pos(obj, x, y);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    return obj;
}

void LVGLTestScreen::create()
{
    auto disp = WT32_SC01::display();
    if (!disp) {
        return;
    }

    _pressCount = 0;
    _lastX = -1;
    _lastY = -1;

    auto screen = lv_disp_get_scr_act(disp);
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    auto title = lv_label_create(screen);
    lv_label_set_text(title, "WT32-SC01 touch display test");
    lv_obj_set_style_text_color(title, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    auto info = lv_label_create(screen);
    lv_label_set_text_fmt(info, "LVGL %d.%d, %dx%d, ST7796S + FT6336U", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, static_cast<int>(kWidth), static_cast<int>(kHeight));
    lv_obj_set_style_text_color(info, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 44);

    // color bars to verify the panel and the color order
    static const uint32_t colors[] = {
        0x000000, 0xff0000, 0x00ff00, 0x0000ff, 0x00ffff, 0xff00ff, 0xffff00, 0xffffff
    };
    static constexpr size_t numColors = sizeof(colors) / sizeof(colors[0]);
    for (size_t i = 0; i < numColors; i++) {
        _addColorBar(screen, static_cast<lv_coord_t>(i * (kWidth / numColors)), 76, static_cast<lv_coord_t>(kWidth / numColors), 94, colors[i]);
    }

    _addCheckerboard(screen, 172);

    // touch area
    auto pad = lv_obj_create(screen);
    lv_obj_set_size(pad, kWidth - 40, 110);
    lv_obj_align(pad, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(pad, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(pad, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0x2080ff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(pad, _touchEventCb, LV_EVENT_ALL, nullptr);

    _pressLabel = lv_label_create(pad);
    lv_label_set_text(_pressLabel, "Presses: 0");
    lv_obj_set_style_text_color(_pressLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(_pressLabel, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(_pressLabel, LV_ALIGN_TOP_LEFT, 8, 8);

    _pointLabel = lv_label_create(pad);
    lv_label_set_text(_pointLabel, "Touch the blue area");
    lv_obj_set_style_text_color(_pointLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(_pointLabel, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(_pointLabel, LV_ALIGN_TOP_LEFT, 8, 40);

    lv_refr_now(disp);
}

void LVGLTestScreen::clear()
{
    auto disp = WT32_SC01::display();
    if (!disp) {
        return;
    }

    _pressLabel = nullptr;
    _pointLabel = nullptr;

    auto screen = lv_disp_get_scr_act(disp);
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_refr_now(disp);
}

uint32_t LVGLTestScreen::getPressCount()
{
    return _pressCount;
}

bool LVGLTestScreen::getLastPoint(int32_t &x, int32_t &y)
{
    if (_pressCount == 0) {
        return false;
    }
    x = _lastX;
    y = _lastY;
    return true;
}

#endif
