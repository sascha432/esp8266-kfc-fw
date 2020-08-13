/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <LoopFunctions.h>
#include <EventTimer.h>
#include <ReadADC.h>

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

ClockPlugin::LoopOptionsType::LoopOptionsType(ClockPlugin &plugin) :
    _plugin(plugin),
    _millis(millis()),
    _millisSinceLastUpdate(get_time_diff(_plugin._lastUpdateTime, _millis)),
    _now(time(nullptr)),
    _tm({})
{
}

uint32_t ClockPlugin::LoopOptionsType::getTimeSinceLastUpdate() const
{
    return _millisSinceLastUpdate;
}

uint32_t ClockPlugin::LoopOptionsType::getMillis() const
{
    return _millis;
}

struct ClockPlugin::LoopOptionsType::tm24 &ClockPlugin::LoopOptionsType::getLocalTime(time_t *nowPtr)
{
    if (!nowPtr) {
        nowPtr = &_now;
    }
    _tm = localtime(nowPtr);
    _tm.set_format_24h(_plugin._config.time_format_24h);
    return _tm;
}

time_t ClockPlugin::LoopOptionsType::getNow() const {
    return _now;
}

bool ClockPlugin::LoopOptionsType::doUpdate() const
{
    return (_millisSinceLastUpdate >= _plugin._updateRate);
}

bool ClockPlugin::LoopOptionsType::hasTimeChanged() const
{
    return _now != _plugin._time;
}

bool ClockPlugin::LoopOptionsType::doRedraw()
{
    if (_plugin._forceUpdate || doUpdate() || hasTimeChanged()) {
        _plugin._forceUpdate = false;
        _plugin._time = _now;
        return true;
    }
    return false;
}




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
    _display.setMillis(options.getMillis());
    _display.setDigit(0, 8, color);
    _display.setDigit(1, 8, color);
    _display.setDigit(2, 8, color);
    _display.setDigit(3, 8, color);
#if IOT_CLOCK_NUM_DIGITS == 6
    _display.setDigit(4, 8, color);
    _display.setDigit(5, 8, color);
#endif
    _display.setColon(0, SevenSegmentDisplay::BOTH, color);
#if IOT_CLOCK_NUM_COLONS == 2
    _display.setColon(1, SevenSegmentDisplay::BOTH, color);
#endif
    _display.show();
    return true;
}

bool ClockPlugin::_loopDisplayLightSensor(LoopOptionsType &options)
{
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    if (_displaySensorValue == 0) {
        return false;
    }

    _forceUpdate = false;
    if (_displaySensorValue == 1) {
        _displaySensorValue++; // set to 2 and make sure it is not called again before finished

        // request to read the ADC 10 times every 25ms ~ 250ms
        ADCManager::getInstance().requestAverage(10, 25, [this](const ADCManager::ADCResult &result) {

            if (
                result.isInvalid() ||           // adc queue has been aborted
                _displaySensorValue != 2        // disabled or something went wrong
            ) {
                __LDBG_printf("ADC callback: display sensor value=%u result=%u", _displaySensorValue, result.isValid());
                return;
            }
            _displaySensorValue = 1;

            auto str = PrintString(F("% " __STRINGIFY(IOT_CLOCK_NUM_DIGITS) "u"), result.getValue()); // left padded with spaces
            // replace space with #
            String_replace(str, ' ', '#');
            // disable any animation, set color to green and brightness to 50%
            SevenSegmentDisplay::AnimationCallback emptyCallback;
            auto &callback = _display.getCallback();
            if (callback) {
                std::swap(callback, emptyCallback);
            }
            _display.setBrightness(SevenSegmentDisplay::kMaxBrightness / 2);
            _display.print(str, Color(0, 0xff, 0));
            _display.show();
            _display.setBrightness(_brightness);
            if (emptyCallback) {
                std::swap(callback, emptyCallback);
            }
        });
    }
    return true;
#else
    return false;
#endif
}

bool ClockPlugin::_loopUpdateButtons(LoopOptionsType &options)
{
    return false;
}

void ClockPlugin::_loop()
{
    LoopOptionsType options(*this);

    // check buttons
    if (_loopUpdateButtons(options)) {
        return;
    }
    if (!options.doUpdate()) {
        return;
    }

    // start update process
    __DBGTM(
        MicrosTimer mt;
        _timerDiff.push_back(options.getTimeSinceLastUpdate())
    );
    _lastUpdateTime = millis();

    if (_loopDisplayLightSensor(options)) {
        return;
    }
    else if (_loopSyncingAnimation(options)) {
        return;
    }

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

#if 0
        tm.tm_hour = 13;
        tm.tm_min = 58;
        displayColon = false;
#endif

        __DBGTM(mt.start());
        _display.setMillis(options.getMillis());
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
        _display.setColon(0, SevenSegmentDisplay::BOTH, color);
        #if IOT_CLOCK_NUM_COLONS == 2
            _display.setColon(1, SevenSegmentDisplay::BOTH, color);
        #endif
        __DBGTM(_animationRenderTime.push_back(mt.getTime()));

        __DBGTM(mt.start());
        _display.show();
        __DBGTM(_displayTime.push_back(mt.getTime()));
    }
}
