/**
 * @file wt32_sc01.cpp
 */
#include "wt32_sc01.h"

#include <Arduino.h>
#include <esp32-hal-psram.h>

namespace WT32_SC01 {

// Two draw buffers in PSRAM (double buffering, 40 lines each).
// SPI DMA on the ESP32 cannot read from PSRAM, so _flushCb() uses the blocking
// pushImage(); pushImageDMA() would need an internal RAM buffer.
static constexpr uint32_t kBufferLines = 40;
static constexpr uint32_t kBufferPixels = IOT_WT32_SC01_TFT_WIDTH * kBufferLines;

static LGFX_WT32_SC01 _lcd;
static lv_disp_draw_buf_t _drawBuf;
static lv_disp_drv_t _dispDrv;
static lv_indev_drv_t _indevDrv;
static lv_disp_t *_disp = nullptr;
static lv_indev_t *_indev = nullptr;
static lv_color_t *_buf1 = nullptr;
static lv_color_t *_buf2 = nullptr;

LGFX_WT32_SC01 &lcd()
{
    return _lcd;
}

lv_disp_t *display()
{
    return _disp;
}

lv_indev_t *input()
{
    return _indev;
}

void setBacklight(uint8_t level)
{
    _lcd.setBrightness(level);
}

static void _flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;
    auto panel = static_cast<LGFX_WT32_SC01 *>(drv->user_data);
    panel->pushImage(area->x1, area->y1, width, height, reinterpret_cast<const uint16_t *>(color_p));
    lv_disp_flush_ready(drv);
}

static void _readCb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    auto panel = static_cast<LGFX_WT32_SC01 *>(drv->user_data);
    int32_t x = 0;
    int32_t y = 0;
    if (panel->getTouch(&x, &y)) {
        data->point.x = static_cast<lv_coord_t>(x);
        data->point.y = static_cast<lv_coord_t>(y);
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->continue_reading = false;
}

bool begin()
{
    if (_disp) {
        return true;
    }
    if (!_lcd.init()) {
        return false;
    }
    _lcd.setRotation(1); // landscape 480x320
    _lcd.setBrightness(255);

    if (!psramFound()) {
        return false; // the draw buffers are allocated from PSRAM
    }
    _buf1 = static_cast<lv_color_t *>(ps_malloc(kBufferPixels * sizeof(lv_color_t)));
    _buf2 = static_cast<lv_color_t *>(ps_malloc(kBufferPixels * sizeof(lv_color_t)));
    if (!_buf1 || !_buf2) {
        free(_buf1);
        free(_buf2);
        _buf1 = nullptr;
        _buf2 = nullptr;
        return false;
    }

    lv_init();
    lv_disp_draw_buf_init(&_drawBuf, _buf1, _buf2, kBufferPixels);

    lv_disp_drv_init(&_dispDrv);
    _dispDrv.hor_res = IOT_WT32_SC01_TFT_WIDTH;
    _dispDrv.ver_res = IOT_WT32_SC01_TFT_HEIGHT;
    _dispDrv.flush_cb = _flushCb;
    _dispDrv.user_data = &_lcd;
    _disp = lv_disp_drv_register(&_dispDrv);

    lv_indev_drv_init(&_indevDrv);
    _indevDrv.type = LV_INDEV_TYPE_POINTER;
    _indevDrv.read_cb = _readCb;
    _indevDrv.user_data = &_lcd;
    _indev = lv_indev_drv_register(&_indevDrv);

    return _disp != nullptr;
}

void loop()
{
    if (_disp) {
        lv_timer_handler();
    }
}

} // namespace WT32_SC01
