/**
 * @file wt32_sc01.cpp
 */
#include "wt32_sc01.h"

#include <Arduino.h>
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>

namespace WT32_SC01 {

// Draw buffers: two in PSRAM (double buffering, one full screen each); PSRAM is required.
// SPI DMA on the ESP32 cannot read from PSRAM, so _flushCb() uses the blocking
// pushImage(); pushImageDMA() would need an internal RAM buffer.
static constexpr uint32_t kBufferLinesPsram = IOT_WT32_SC01_TFT_HEIGHT;

// LovyanGFX rotation of every Rotation value. The panel is portrait natively, so the hardware
// rotation 1 is the landscape orientation the display is used in by default
static constexpr uint8_t kRotationToHardware[] = { 1, 0, 3, 2 };
static constexpr bool kRotationIsPortrait[] = { false, true, false, true };

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
static bool _touchReady = false;
static bool _touchPressed = false;
static uint32_t _touchCount = 0;
static int32_t _touchX = -1;
static int32_t _touchY = -1;
static bool _inputEnabled = true;
static Rotation _rotation = Rotation::LANDSCAPE;

// allocates from PSRAM, returns nullptr if not available
static void *_allocBuffer(size_t size)
{
    return psramFound() ? ps_malloc(size) : nullptr;
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

bool touchReady()
{
    return _touchReady;
}

bool isTouched()
{
    return _touchPressed;
}

uint32_t getTouchCount()
{
    return _touchCount;
}

bool getLastTouch(int32_t &x, int32_t &y)
{
    if (_touchX < 0 || _touchY < 0) {
        return false;
    }
    x = _touchX;
    y = _touchY;
    return true;
}

void setInputEnabled(bool enabled)
{
    if (_inputEnabled != enabled) {
        _inputEnabled = enabled;
        __LDBG_printf("input %s", enabled ? "enabled" : "disabled");
    }
}

bool getInputEnabled()
{
    return _inputEnabled;
}

// Panel resolution of an orientation (the display is 480x320 in landscape)
static inline lv_coord_t _widthOf(Rotation rotation)
{
    return kRotationIsPortrait[static_cast<uint8_t>(rotation)] ? IOT_WT32_SC01_TFT_HEIGHT : IOT_WT32_SC01_TFT_WIDTH;
}

static inline lv_coord_t _heightOf(Rotation rotation)
{
    return kRotationIsPortrait[static_cast<uint8_t>(rotation)] ? IOT_WT32_SC01_TFT_WIDTH : IOT_WT32_SC01_TFT_HEIGHT;
}

Rotation getRotation()
{
    return _rotation;
}

bool isPortrait()
{
    return kRotationIsPortrait[static_cast<uint8_t>(_rotation)];
}

bool setRotation(Rotation rotation)
{
    if (!_panelReady) {
        return false;
    }
    if (rotation == _rotation) {
        return true;
    }
    _rotation = rotation;
    // the touch transform follows the panel rotation (LovyanGFX convertRawXY())
    _lcd.setRotation(kRotationToHardware[static_cast<uint8_t>(rotation)]);

    if (_disp) {
        // The draw buffers cover both orientations without a reallocation: the PSRAM buffers hold
        // kBufferLinesPsram (= the panel height) lines of the 480 px wide screen, which is the
        // same number of pixels as the 320 px wide screen needs for its full 480 lines. The
        // resolution of the display is updated in place, LVGL resizes and invalidates the screens
        _dispDrv.hor_res = _widthOf(rotation);
        _dispDrv.ver_res = _heightOf(rotation);
        lv_disp_drv_update(_disp, &_dispDrv);
        _firstFlush = true;
    }
    __LDBG_printf("rotation %u (%ux%u)", (unsigned)kRotationToHardware[static_cast<uint8_t>(rotation)], (unsigned)_widthOf(rotation), (unsigned)_heightOf(rotation));
    return true;
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
        // the state is tracked while the input is disabled as well, so that the touch that wakes
        // the display up is noticed
        if (!_touchPressed) {
            _touchPressed = true;
            _touchCount++;
            __LDBG_printf("touch pressed at %d,%d", static_cast<int>(x), static_cast<int>(y));
        }
        _touchX = x;
        _touchY = y;
    }
    else {
        if (_touchPressed) {
            __LDBG_printf("touch released at %d,%d", static_cast<int>(_touchX), static_cast<int>(_touchY));
            _touchPressed = false;
        }
    }
    // a touch that is not accepted is reported as released, LVGL and the widgets do not see it
    if (_inputEnabled && _touchPressed) {
        data->point.x = static_cast<lv_coord_t>(_touchX);
        data->point.y = static_cast<lv_coord_t>(_touchY);
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
    // landscape 480x320 (the panel is portrait natively)
    _lcd.setRotation(kRotationToHardware[static_cast<uint8_t>(_rotation)]);
    // LGFXBase::init_impl() applies LGFXBase::_brightness (default 127), the requested level wins
    _lcd.setBrightness(_backlight);

    if (!(_touchReady = _lcd.getPanel()->initTouch())) {
        __LDBG_printf("touch controller init failed (I2C address 0x%02x)", IOT_WT32_SC01_TOUCH_I2C_ADDRESS);
    }
    else {
        __LDBG_printf("touch controller ready (FT6336U, I2C address 0x%02x, I2C polling)", IOT_WT32_SC01_TOUCH_I2C_ADDRESS);
    }

    // two buffers in PSRAM (double buffering)
    _bufferPixels = kBufferLinesPsram * IOT_WT32_SC01_TFT_WIDTH;
    _buf1 = static_cast<lv_color_t *>(_allocBuffer(_bufferPixels * sizeof(lv_color_t)));
    _buf2 = static_cast<lv_color_t *>(_allocBuffer(_bufferPixels * sizeof(lv_color_t)));
    if (!_buf1 || !_buf2) {
        free(_buf1);
        free(_buf2);
        _buf1 = nullptr;
        _buf2 = nullptr;
        _error = F("no PSRAM for the draw buffers");
        __LDBG_printf("draw buffer allocation failed");
        return false;
    }
    __LDBG_printf("draw buffer %u pixels in PSRAM (double buffered)", (unsigned)_bufferPixels);

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
