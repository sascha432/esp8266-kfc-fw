/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <ReadADC.h>
#include "clock.h"
#include "stl_ext/algorithm.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


extern uint32_t gpio_clear;
extern uint32_t gpio_set;
extern uint32_t pinMask;
extern uint8_t pin;

static bool init_gpio();
uint32_t gpio_clear;
uint32_t gpio_set;
uint32_t pinMask;
uint8_t pin = 12;
static bool initxxx = init_gpio();
static bool init_gpio() {
    pinMask=_BV(pin);
    if (pin == 16) {
        gpio_clear = (READ_PERI_REG(RTC_GPIO_OUT) & 0xfffffffeU);
        gpio_set = gpio_clear | 1;
    }
    else {
        gpio_set = pinMask;
        gpio_clear = pinMask;
    }
    return true;
}


#if IOT_LED_MATRIX == 0

Clock::ClockLoopOptions::ClockLoopOptions(ClockPlugin &plugin) :
    LoopOptionsBase(plugin),
    _time(plugin._time),
    _now(time(nullptr)),
    _tm({}),
    format_24h(plugin._config.time_format_24h)
{
}

struct Clock::ClockLoopOptions::tm24 &Clock::ClockLoopOptions::getLocalTime(time_t *nowPtr)
{
    if (!nowPtr) {
        nowPtr = &_now;
    }
    _tm = localtime(nowPtr);
    _tm.set_format_24h(format_24h);
    return _tm;
}


#if IOT_CLOCK_PIXEL_SYNC_ANIMATION

#error NOT UP TO DATE

bool ClockPlugin::_loopSyncingAnimation(LoopOptionsType &options)
{
    if (!_isSyncing) {
        return false;
    }
    // __LDBG_printf("isyncing=%u", _isSyncing);

    //     if (_isSyncing) {
//         if (get_time_diff(_lastUpdateTime, millis()) >= 100) {
//             _lastUpdateTime = millis();

// #if IOT_CLOCK_PIXEL_SYNC_ANIMATION
//             #error crashing

//             // show syncing animation until the time is valid
//             if (_pixelOrder) {
//                 if (++_isSyncing > IOT_CLOCK_PIXEL_ORDER_LEN) {
//                     _isSyncing = 1;
//                 }
//                 for(uint8_t i = 0; i < SevenSegmentDisplay::getNumDigits(); i++) {
//                     _display.rotate(i, _isSyncing - 1, _color, _pixelOrder.data(), _pixelOrder.size());
//                 }
//             }
//             else {
//                 if (++_isSyncing > SevenSegmentPixel_DIGITS_NUM_PIXELS) {
//                     _isSyncing = 1;
//                 }
//                 for(uint8_t i = 0; i < _display->numDigits(); i++) {
//                     _display->rotate(i, _isSyncing - 1, _color, nullptr, 0);
//
//             }
// #endif
//             _display.show();
//         }
//         return;
//     }

    // show 88:88:88 instead of the syncing animation
    uint32_t color = _color;
    _display.setParams(options.getMillis(), _getBrightness());
    _display.setDigit(0, 8, color);
    _display.setDigit(1, 8, color);
    _display.setDigit(2, 8, color);
    _display.setDigit(3, 8, color);
#        if IOT_CLOCK_NUM_DIGITS == 6
    _display.setDigit(4, 8, color);
    _display.setDigit(5, 8, color);
#        endif
    _display.setColon(0, SevenSegmentDisplay::BOTH, color);
#        if IOT_CLOCK_NUM_COLONS == 2
    _display.setColon(1, SevenSegmentDisplay::BOTH, color);
#        endif
    _display.show();
    return true;
}

#endif

#endif

void ClockPlugin::_loop()
{
    if (_debug) {
        return;
    }
    //
    LoopOptionsType options(*this);
    _display.setBrightness(_getBrightness());

    if (_animation) {
        _animation->loop(options.getMillis());
    }
    if (_blendAnimation) {
        _blendAnimation->loop(options.getMillis());
    }

    // while(true) {

    if (options.doUpdate()) {

        // start update process
        _lastUpdateTime = millis();

        IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
            if (_loopDisplayLightSensor(options)) {
                return;
            }
        )

        IF_IOT_CLOCK_PIXEL_SYNC_ANIMATION(
            if (_loopSyncingAnimation(options)) {
                // ...
            }
            else
        )
        if (options.doRedraw()) {

            IF_IOT_CLOCK(
                uint8_t displayColon = true;
                // does the animation allow blinking and is it set?
                if ((!_animation || (_animation && !_animation->isBlinkColonDisabled())) && (_config.blink_colon_speed >= kMinBlinkColonSpeed)) {
                    // set on/off depending on the update rate
                    displayColon = ((_lastUpdateTime / _config.blink_colon_speed) % 2 == 0);
                }

                auto &tm = options.getLocalTime();

                _display.setDigit(0, tm.tm_hour_format() / 10);
                _display.setDigit(1, tm.tm_hour_format() % 10);
                _display.setDigit(2, tm.tm_min / 10);
                _display.setDigit(3, tm.tm_min % 10);
                #if IOT_CLOCK_NUM_DIGITS == 6
                    _display.setDigit(4, tm.tm_sec / 10);
                    _display.setDigit(5, tm.tm_sec % 10);
                #endif

                _display.setColons(displayColon ? Clock::SevenSegment::ColonType::BOTH : Clock::SevenSegment::ColonType::NONE);
            )

            if (_blendAnimation) {
                _animation->nextFrame(options.getMillis());
                _blendAnimation->nextFrame(options.getMillis());

                if (!_blendAnimation->blend(_display, options.getMillis())) {
                    __LDBG_printf("blending done");
                    delete _blendAnimation;
                    _blendAnimation = nullptr;
                }
            }
            else if (_animation) {
                _animation->nextFrame(options.getMillis());
                _animation->copyTo(_display, options.getMillis());
            }
        }
    }

    _display.show();
    delay(5);
}
