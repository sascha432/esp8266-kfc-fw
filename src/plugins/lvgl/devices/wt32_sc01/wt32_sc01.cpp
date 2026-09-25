/**
 * @file wt32_sc01.cpp
 */
#include "wt32_sc01.h"

#include <Arduino.h>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>

namespace WT32_SC01 {

// Preferred draw buffers: two in PSRAM (double buffering, 40 lines each). If PSRAM is
// not available or exhausted, a single smaller buffer in internal RAM is used.
// SPI DMA on the ESP32 cannot read from PSRAM, so _flushCb() uses the blocking
// pushImage(); pushImageDMA() would need an internal RAM buffer.
static constexpr uint32_t kBufferLinesPsram = 40;
static constexpr uint32_t kBufferLinesInternal = 16;

static LGFX_WT32_SC01 _lcd;
static lv_disp_draw_buf_t _drawBuf;
static lv_disp_drv_t _dispDrv;
static lv_indev_drv_t _indevDrv;
static lv_disp_t *_disp = nullptr;
static lv_indev_t *_indev = nullptr;
static lv_color_t *_buf1 = nullptr;
static lv_color_t *_buf2 = nullptr;
static uint32_t _bufferPixels = 0;
static const __FlashStringHelper *_error = nullptr;
static uint8_t _backlight = 255;    // requested level, applied after the panel init
static bool _panelReady = false;
static bool _firstFlush = true;
static bool _touchPressed = false;
static int32_t _touchX = -1;
static int32_t _touchY = -1;

// allocates from PSRAM or internal RAM, returns nullptr if not available
static void *_allocBuffer(size_t size, bool psram)
{
    if (psram) {
        return psramFound() ? ps_malloc(size) : nullptr;
    }
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

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

const __FlashStringHelper *lastError()
{
    return _error;
}

void setBacklight(uint8_t level)
{
    _backlight = level;
    if (_panelReady) {
        _lcd.setBrightness(level);
    }
}

uint8_t getBacklight()
{
    return _backlight;
}

static void _flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;
    if (_firstFlush) {
        _firstFlush = false;
        __LDBG_printf("first flush %ux%u at %d,%d", (unsigned)width, (unsigned)height, (int)area->x1, (int)area->y1);
    }
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
        if (!_touchPressed || x != _touchX || y != _touchY) {
            __LDBG_printf("touch %d,%d%s", (int)x, (int)y, _touchPressed ? "" : " (pressed)");
            _touchPressed = true;
            _touchX = x;
            _touchY = y;
        }
    }
    else {
        data->state = LV_INDEV_STATE_RELEASED;
        if (_touchPressed) {
            __LDBG_printf("touch released");
            _touchPressed = false;
            _touchX = -1;
            _touchY = -1;
        }
    }
    data->continue_reading = false;
}

bool begin()
{
    if (_disp) {
        return true;
    }
    _error = nullptr;

    // PSRAM is optional, the draw buffers fall back to internal RAM
    __LDBG_printf("psram=%u size=%u free=%u internal=%u", (unsigned)psramFound(), (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());

    // NOTE: Panel_Device::init() returns true unconditionally in LovyanGFX 1.2.30, this
    // only fails if there is no panel/bus at all
    if (!_lcd.init()) {
        _error = F("panel initialization failed");
        __LDBG_printf("panel initialization failed");
        return false;
    }
    _panelReady = true;
    _lcd.setRotation(1); // landscape 480x320
    // LGFXBase::init_impl() applies LGFXBase::_brightness (default 127), the requested level wins
    _lcd.setBrightness(_backlight);

    if (!_lcd.getPanel()->initTouch()) {
        __LDBG_printf("touch controller init failed (I2C address 0x%02x)", IOT_WT32_SC01_TOUCH_I2C_ADDRESS);
    }

    // two buffers in PSRAM (double buffering), or a single smaller one in internal RAM
    _bufferPixels = kBufferLinesPsram * IOT_WT32_SC01_TFT_WIDTH;
    _buf1 = static_cast<lv_color_t *>(_allocBuffer(_bufferPixels * sizeof(lv_color_t), true));
    _buf2 = static_cast<lv_color_t *>(_allocBuffer(_bufferPixels * sizeof(lv_color_t), true));
    if (!_buf1 || !_buf2) {
        free(_buf1);
        free(_buf2);
        _buf1 = nullptr;
        _buf2 = nullptr;
        _bufferPixels = kBufferLinesInternal * IOT_WT32_SC01_TFT_WIDTH;
        _buf1 = static_cast<lv_color_t *>(_allocBuffer(_bufferPixels * sizeof(lv_color_t), false));
        if (!_buf1) {
            _error = F("no memory for the draw buffer (PSRAM and internal RAM)");
            __LDBG_printf("draw buffer allocation failed");
            return false;
        }
    }
    __LDBG_printf("draw buffer %u pixels%s", (unsigned)_bufferPixels, _buf2 ? " in PSRAM (double buffered)" : " in internal RAM");

    lv_init();
    lv_disp_draw_buf_init(&_drawBuf, _buf1, _buf2, _bufferPixels);

    lv_disp_drv_init(&_dispDrv);
    _dispDrv.hor_res = IOT_WT32_SC01_TFT_WIDTH;
    _dispDrv.ver_res = IOT_WT32_SC01_TFT_HEIGHT;
    _dispDrv.draw_buf = &_drawBuf; // required, the buffer is not set by lv_disp_drv_init()
    _dispDrv.flush_cb = _flushCb;
    _dispDrv.user_data = &_lcd;
    _disp = lv_disp_drv_register(&_dispDrv);
    if (!_disp) {
        _error = F("registering the LVGL display failed");
        return false;
    }
    _firstFlush = true;

    lv_indev_drv_init(&_indevDrv);
    _indevDrv.type = LV_INDEV_TYPE_POINTER;
    _indevDrv.read_cb = _readCb;
    _indevDrv.user_data = &_lcd;
    _indev = lv_indev_drv_register(&_indevDrv);

    return true;
}

void loop()
{
    if (_disp) {
        lv_timer_handler();
    }
}

} // namespace WT32_SC01
