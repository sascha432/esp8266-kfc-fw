/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include <MicrosTimer.h>
#include "dimmer_module.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DimmerButton::DimmerButton(uint8_t pin, uint8_t channel, uint8_t button, DimmerButtons &dimmer) :
    Pin(pin, this, StateType::UP_DOWN, IOT_SWITCH_PRESSED_STATE),
    _dimmer(dimmer),
    _startTimer(0),
    _noRepeatTimer(0),
    _duration(0),
    _repeatCount(0),
    _level(0),
    _channel(channel),
    _button(button),
    _startTimerRunning(false),
    _noRepeatTimerRunning(false),
    _noRepeatCount(0)
{
}

void DimmerButton::event(StateType state, TimeType now)
{
    __LDBG_printf("EVENT channel=%u button=%u state=%s time=%u", PinMonitor::Monitor::stateType2String(state), _channel, _button, now);
    if (state == StateType::PRESSED) {

        _startTimer = now;
        _startTimerRunning = true;
        _repeatCount = 0;
        __LDBG_assert(_noRepeatCount < 3);
        __LDBG_printf("PRESSED channel=%u button=%u norepeat_count=%u store_level=%d", _channel, _button, _noRepeatCount, _level);
        if (_noRepeatCount == 0) {
            __LDBG_assert(_noRepeatTimerRunning == false);
            _noRepeatCount++;
            _level = _dimmer.getChannel(_channel);
        }
        __LDBG_printf("no_repeat_timer_running=%u no_repeat_count=%u store_level=%d", _noRepeatTimerRunning, _noRepeatCount, _level);
        return;

    }

    if (_startTimerRunning == false) {

        // ignore up event without start timer running
        __LDBG_printf("ERROR timer=%u not running", _startTimer);
        return;

    }
    _buttonReleased();
    _startTimerRunning = false;
}

void DimmerButton::loop()
{
    if (_startTimerRunning) {

        _duration = get_time_diff(_startTimer, millis());
        auto &config = _dimmer._getConfig();
        // between down and up
        if (_duration >= config.longpress_time) {
            // start with 1...
            uint16_t repeatCount = 1 + ((_duration - config.longpress_time) / config.repeat_time);
            bool repeatChanged = repeatCount != _repeatCount;
            _repeatCount = repeatCount;
            if (repeatChanged) {
                __LDBG_printf("REPEAT_CHANGED count=%u channel=%u button=%u duration=%u", _repeatCount, _channel, _button, _duration);
                _changeLevel(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.shortpress_step / (_button == 1 ? -100 : 100), config.shortpress_fadetime);
            }
        }
        return;

    }
    else if (_noRepeatTimerRunning) {
        // after up and next down
        uint16_t duration = get_time_diff(_noRepeatTimer, millis());
        if (duration > _dimmer._getConfig().shortpress_no_repeat_time) {
            __LDBG_printf("no_repeat_time=%u duration=%u channel=%u button=%u no_repeat_count=%u", _dimmer._getConfig().shortpress_no_repeat_time, duration, _channel, _button, _noRepeatCount);
            __LDBG_assert(_noRepeatCount != 0 && _noRepeatCount < 3);
            if (_noRepeatCount == 1) {
                __LDBG_printf("TURN_OFF channel=%u button=%u stored_level=%d", _channel, _button, _level);
                // turn off
                if (_dimmer.off(_channel)) {
                    _dimmer._channels[_channel].setStoredBrightness(_level);
                }
            }
            _noRepeatCount = 0;
            _noRepeatTimerRunning = false;
            __LDBG_printf("no_repeat_timer_running=%u no_repeat_count=%u", _noRepeatTimerRunning, _noRepeatCount);
        }
    }
}

void DimmerButton::_changeLevel(int32_t change, float fadeTime)
{
    int32_t curLevel = _dimmer.getChannel(_channel);
    __LDBG_printf("CHANGE_LEVEL channel=%u change=%d level=%u time=%f", _channel, change, curLevel, fadeTime);
    _setLevel(curLevel + change, curLevel, fadeTime);
}

void DimmerButton::_setLevel(int32_t newLevel, float fadeTime)
{
    __LDBG_printf("SET_LEVEL channel=%u level=%u time=%f", _channel, newLevel, fadeTime);
    _setLevel(newLevel, _dimmer.getChannel(_channel), fadeTime);
}

void DimmerButton::_setLevel(int32_t newLevel, int32_t curLevel, float fadeTime)
{
    newLevel = std::clamp_signed(
        newLevel,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _dimmer._getConfig().min_brightness / 100,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS
    );
    if (curLevel != newLevel) {
        _dimmer.setChannel(_channel, newLevel, fadeTime);
    }
}

void DimmerButton::_buttonReleased()
{
    uint32_t ms = millis();
    _duration = get_time_diff(_startTimer, ms);
    auto &config = _dimmer._getConfig();

    if (_button == 1) {
        // start timer
        _noRepeatTimer = ms;
        _noRepeatTimerRunning = true;
        if (_noRepeatCount < 2) {
            _noRepeatCount++;
        }
        __LDBG_printf("no_repeat_timer_running=%u no_repeat_count=%u", _noRepeatTimerRunning, _noRepeatCount);
    }

    if (_duration < config.shortpress_time) {

        __LDBG_printf("SHORT=%u channel=%u button=%u duration=%u state=%u", config.shortpress_time, _channel, _button, _duration, _dimmer.getChannelState(_channel));
        // short press
        if (_dimmer.getChannelState(_channel)) {
            __LDBG_printf("CHANGE channel=%u button=%u", _channel, _button);
            // channel is on
            // increase level for button 0, decrease for button 1
            _changeLevel(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.shortpress_step / (_button == 1 ? -100 : 100), config.shortpress_fadetime);
        }
        else if (_button == 0) {
            __LDBG_printf("TURN_ON channel=%u button=%u", _channel, _button);
            // button 0 turns channel on
            _dimmer.on(_channel);
        }
        return;

    }

    if (_duration < config.longpress_time) {

        int32_t level = _dimmer.getChannelState(_channel) ?
            (IOT_DIMMER_MODULE_MAX_BRIGHTNESS * (int32_t)(_button == 0 ? config.longpress_max_brightness : config.longpress_min_brightness) / 100) :
            -1;

        __LDBG_printf("LONG=%u channel=%u button=%u duration=%u level=%d", config.longpress_time, _channel, _button, _duration, level);
        // long press
        if (level != -1) {
            // channel is on
            _setLevel(level, config.longpress_fadetime);
        }
        return;

    }
}

#endif
