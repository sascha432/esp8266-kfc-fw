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
    LoopOptionsType options(*this);
    _display.setBrightness(_getBrightness());
    _display.show();

    if (_animation) {
        _animation->loop(options.getMillis());
    }
    if (_blendAnimation) {
        _blendAnimation->loop(options.getMillis());
    }

    while(true) {

        if (!options.doUpdate()) {
            break;
        }

        // start update process
        _lastUpdateTime = millis();

        IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
            if (_loopDisplayLightSensor(options)) {
                break;
            }
        )

        IF_IOT_CLOCK_PIXEL_SYNC_ANIMATION(
            if (_loopSyncingAnimation(options)) {
                break;
            }
        )

        if (options.doRedraw()) {

            IF_IOT_CLOCK(

                uint8_t displayColon = true;
                // does the animation allow blinking and is it set?
                if ((!_animation || (_animation && !_animation->isBlinkColonDisabled())) && (_config.blink_colon_speed >= kMinBlinkColonSpeed)) {
                    // set on/off depending on the update rate
                    displayColon = ((_lastUpdateTime / _config.blink_colon_speed) % 2 == 0);
                }

                uint32_t color = _getColor();
                auto &tm = options.getLocalTime();

                _display.setDigit(0, tm.tm_hour_format() / 10, color);
                _display.setDigit(1, tm.tm_hour_format() % 10, color);
                _display.setDigit(2, tm.tm_min / 10, color);
                _display.setDigit(3, tm.tm_min % 10, color);
                #if IOT_CLOCK_NUM_DIGITS == 6
                    _display.setDigit(4, tm.tm_sec / 10, color);
                    _display.setDigit(5, tm.tm_sec % 10, color);
                #endif

                if (!displayColon) {
                    color = 0;
                }

                _display.setColon(0, Clock::SevenSegment::ColonType::TOP, color);
                _display.setColon(0, Clock::SevenSegment::ColonType::BOTTOM, color);
                #if IOT_CLOCK_NUM_COLONS == 2
                    _display.setColon(1, Clock::SevenSegment::ColonType::TOP, color);
                    _display.setColon(1, Clock::SevenSegment::ColonType::BOTTOM, color);
                #endif

            )

            if (_blendAnimation) {

                _animation->nextFrame(options.getMillis());
                _blendAnimation->nextFrame(options.getMillis());

                if (_blendAnimation->blend(_display, options.getMillis())) {
                    // display mixed state
                    _display.showIntrLocked();
                }
                else {
                    // blending done, delete _blendAnimation
                    __LDBG_printf("blending done");
                    delete _blendAnimation;
                    _blendAnimation = nullptr;
                    _display.showIntrLocked();
                }

            }
            else if (_animation) {

#define BENCH 0
                // render single frame
#if BENCH
                // ets_intr_lock();
                uint32_t _start = micros();
#endif
                _animation->nextFrame(options.getMillis());
#if BENCH
                uint32_t dur2 = micros() - _start;
#endif

                _animation->copyTo(_display, options.getMillis());
#if BENCH
                uint32_t dur3 = micros() - _start;
#endif
                _display.show();
#if BENCH
                uint32_t dur4 = micros() - _start;
// ets_intr_unlock();
                ::printf("%u %u %u\n", dur2, dur3, dur4);
#endif

            }
            else {
                // no animation object available
                // display plain color
                _display.fill(_getColor());
                _display.showIntrLocked();
            }

            return;
        }

        break;
    }

    if (options.doRefresh()) {
        _display.showIntrLocked();
    }
    else {
        // run display.show() every millisecond to update dithering
        // mandatory frames are displayed with interrupts locked
        _display.delay(1);
    }

    // __LDBG_printf("refresh fading_brightness=%u color=%s fps=%u", _fadingBrightness, getColor().toString().c_str(), FastLED.getFPS());
}
