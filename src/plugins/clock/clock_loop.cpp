/**
 * Author: sascha_lammers@gmx.de
 */

#if !IOT_LED_MATRIX

#error TODO merge with led_matrix_loop

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <ReadADC.h>
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_CLOCK_USE_DITHERING == 0
#define _dithering()
#endif

Clock::ClockLoopOptions::ClockLoopOptions(ClockPlugin &plugin) :
    LoopOptionsBase(plguin),
    _updateRate(plugin.getUpdateRate()),
    _forceUpdate(plugin._forceUpdate),
    _isFading(plugin._isFading),
    _time(plugin._time),
    _millis(millis()),
    _millisSinceLastUpdate(get_time_diff(plugin._lastUpdateTime, _millis)),
    _now(time(nullptr)),
    _tm({})
{
}

struct Clock::ClockLoopOptions::tm24 &Clock::ClockLoopOptions::getLocalTime(time_t *nowPtr)
{
    if (!nowPtr) {
        nowPtr = &_now;
    }
    _tm = localtime(nowPtr);
    _tm.set_format_24h(_plugin._config.time_format_24h);
    return _tm;
}


#if IOT_CLOCK_PIXEL_SYNC_ANIMATION

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
//                 }
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

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
bool ClockPlugin::_loopDisplayLightSensor(LoopOptionsType &options)
{
    if (_displaySensor == DisplaySensorType::OFF) {
        return false;
    }
    _forceUpdate = false;
    if (_displaySensor == DisplaySensorType::SHOW) {
        _displaySensor = DisplaySensorType::BUSY;

        // request to read the ADC 10 times every 25ms ~ 250ms
        ADCManager::getInstance().requestAverage(10, 25000, [this](const ADCManager::ADCResult &result) {

            if (
                result.isInvalid() ||           // adc queue has been aborted
                _displaySensor != DisplaySensorType::BUSY        // disabled or something went wrong
            ) {
                __LDBG_printf("ADC callback: display sensor value=%u result=%u", _displaySensor, result.isValid());
                return;
            }
            _displaySensor = DisplaySensorType::SHOW; // set back from BUSY to SHOW

            auto str = PrintString(F("% " __STRINGIFY(IOT_CLOCK_NUM_DIGITS) "u"), result.getValue()); // left padded with spaces
            // replace space with #
            String_replace(str, ' ', '#');
            // disable any animation, set color to green and brightness to 50%
            SevenSegmentDisplay::AnimationCallback emptyCallback;
            auto &callback = _display.getCallback();
            if (callback) {
                std::swap(callback, emptyCallback);
            }
            auto brightness = std::exchange(_display._params.brightness, SevenSegmentDisplay::kMaxBrightness / 2);
            _display.print(str, Color(0, 0xff, 0));
            _display.show();
            _display._params.brightness = brightness;
            if (emptyCallback) {
                std::swap(callback, emptyCallback);
            }
        });
    }
    return true;
}
#endif

void ClockPlugin::_loop()
{
    LoopOptionsType options(*this);

#    if IOT_CLOCK_BUTTON_PIN!=-1
    // check buttons
    if (_loopUpdateButtons(options)) {
        _dithering();
        return;
    }
#    endif
    if (!options.doUpdate()) {
        _dithering();
        return;
    }

    // start update process
    __DBGTM(MicrosTimer mt; _timerDiff.push_back(options.getTimeSinceLastUpdate()));
    _lastUpdateTime = millis();

#    if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    if (_loopDisplayLightSensor(options)) {
        _dithering();
        return;
    }
#    endif
#    if IOT_CLOCK_PIXEL_SYNC_ANIMATION
    if (_loopSyncingAnimation(options)) {
        _dithering();
        return;
    }
#    endif

    if (_animation) {
        __DBGTM(mt.start());
        _animation->loop(options.getNow());
        __DBGTM(_anmationTime.push_back(mt.getTime()));
        if (_animation->finished()) {
            __LDBG_printf("animation loop has finished");
            setAnimation(AnimationType::NEXT);
        }
    }

    if (options.doRedraw()) {

        uint8_t displayColon = true;
        // does the animation allow blinking and is it set?
        if ((!_animation || _animation->doBlinkColon()) && (_config.blink_colon_speed >= kMinBlinkColonSpeed)) {
            // set on/off depending on the update rate
            displayColon = ((_lastUpdateTime / _config.blink_colon_speed) % 2 == 0);
        }

        uint32_t color = _color;
        auto &tm = options.getLocalTime();

        if (_isFading && _fadeTimer.reached()) {
            __LDBG_printf("fading=done brightness=%u", _targetBrightness);
            _isFading = false;
            _schedulePublishState = true;
        }

        __DBGTM(mt.start());
        _display.setParams(options.getMillis(), _getBrightness());

        _display.setDigit(0, tm.tm_hour_format() / 10, color);
        _display.setDigit(1, tm.tm_hour_format() % 10, color);
        _display.setDigit(2, tm.tm_min / 10, color);
        _display.setDigit(3, tm.tm_min % 10, color);
#    if IOT_CLOCK_NUM_DIGITS == 6
        _display.setDigit(4, tm.tm_sec / 10, color);
        _display.setDigit(5, tm.tm_sec % 10, color);
#    endif

        if (!displayColon) {
            color = 0;
        }
        _display.setColon(0, SevenSegmentDisplay::BOTH, color);
#    if IOT_CLOCK_NUM_COLONS == 2
        _display.setColon(1, SevenSegmentDisplay::BOTH, color);
#    endif
        __DBGTM(_animationRenderTime.push_back(mt.getTime()));

        __DBGTM(mt.start());
        _display.show();
        __DBGTM(_displayTime.push_back(mt.getTime()));
    }
    else {
        _dithering();
    }
}

#endif
