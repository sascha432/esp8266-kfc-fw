/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include <MicrosTimer.h>
#include "dimmer_button.h"
#include "dimmer_buttons.h"
#include "dimmer_module.h"
#include <stl_ext/algorithm.h>
// #include <EnumHelper.h>

#if DEBUG_IOT_DIMMER_MODULE //&& DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Dimmer;
using namespace PinMonitor;

Button::Button(uint8_t pin, uint8_t channel, uint8_t button, Buttons &dimmer, SingleClickGroupPtr singleClickGroup) :
    PushButton(pin, &dimmer, std::move(ButtonConfig(dimmer._getConfig())), singleClickGroup, dimmer._getConfig()._base.pin_inverted(channel, button) ? ActiveStateType::INVERTED : ActiveStateType::NON_INVERTED),
    _dimmer(dimmer),
    _level(0),
    _channel(channel),
    _button(button),
    _repeat(0)
{
    if (_longPressTime == 0) {
        _longPressTime = std::max<uint16_t>(_repeatTime, _clickTime + 50);
        _repeatTime = _longPressTime;
        _subscribedEvents = EnumHelper::Bitset::removeBits(_subscribedEvents, EventType::LONG_PRESSED);
    }
    // if (_button == 1) {
    //     _subscribedEvents = EnumHelper::Bitset::addBits(_subscribedEvents, EventType::SINGLE_CLICK);
    // }
    _subscribedEvents = EnumHelper::Bitset::addBits(_subscribedEvents, EventType::REPEATED_CLICK);
// #if DEBUG_PIN_MONITOR
//     setName(PrintString(F("%s:%u"), _button == 0 ? PSTR("BUTTON-UP") : PSTR("BUTTON-DOWN"), _channel));
// #endif
}

void Button::event(EventType eventType, uint32_t now)
{
    __DBG_IF(
        auto oldLevel = _dimmer.getChannel(_channel);
    );
    auto &config = _dimmer._getConfig();
    auto groupRepeatCount = _singleClickGroup->getRepeatCount();
    switch (eventType) {
        case EventType::DOWN:
            // store level first
            if (groupRepeatCount == 0) {
                _level = _dimmer.getChannel(_channel);
            }
            if (_dimmer.getChannelState(_channel)) {
                // int16_t levelChange = (IOT_DIMMER_MODULE_MAX_BRIGHTNESS / _singleClickSteps);
                // _level += _button == 1 ? -levelChange : levelChange;
                // calculate time for a single click to get a smooth transition to hold repeat
                // _changeLevelSingle(_singleClickSteps, _button == 1, (_longPressTime * config.lp_fadetime / 950.0) / (config.lp_fadetime / _singleClickSteps));
                if (config._base.longpress_time == 0) {
                    _changeLevelRepeat(_repeatTime, _button == 1);
                }
                else {
                    _changeLevelSingle(_singleClickSteps, _button == 1, config._base.lp_fadetime);
                }
            }
            else if (_button == 0) {
                _dimmer.on(_channel);
            }
            break;
        case EventType::UP:
            if (_repeat & _BV(_button)) {
                _repeat &= ~_BV(_button);
                _freezeLevel();
            }
            break;
        case EventType::HOLD_REPEAT:
            _changeLevelRepeat(_repeatTime, _button == 1);
            _repeat |= _BV(_button);
            break;

        // case EventType::SINGLE_CLICK:
        case EventType::REPEATED_CLICK:
            if (_button == 1 && _repeatCount == 1 && groupRepeatCount == 1) {
                // button 1 only
                if (_dimmer._channels[_channel].off(&config, _level)) {
                    __LDBG_printf("#%u OFF store_level=%d duration=%u", _button, _level, _duration);
                }
            }
            else {
                if (config._base.longpress_time == 0 && (_repeatCount > 1 || groupRepeatCount > 1)) {
                    _freezeLevel();
                }
            }
            break;
        case EventType::LONG_CLICK: {
                int32_t level = _button == 0 ? config._base.longpress_max_brightness : config._base.longpress_min_brightness;
                if (level != 0) {
                    level = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * level / 100;
                    if (!_dimmer.getChannelState(_channel)) {
                        _dimmer.on(_channel);
                    }
                    _setLevel(level, config._base.lp_fadetime);
                }
            }
            break;
        default:
            __LDBG_printf("#%u IGNORED event=%s", _button, eventTypeToString(eventType));
            break;
    }
    __DBG_IF(
        __DBG_printf("pin=%u type=%s (%02x) repeat=%u/0x%01x grprep=%u btn=%u dur=%u grpdur=%u level=%u new=%u", _pin, eventTypeToString(eventType), eventType, _repeatCount, _repeat, groupRepeatCount, _button, _duration, _singleClickGroup->getDuration(), oldLevel, _dimmer.getChannel(_channel));
    );
}

void Button::_changeLevel(int32_t change, float fadeTime)
{
    int32_t curLevel = _dimmer.getChannel(_channel);
    _setLevel(curLevel + change, curLevel, fadeTime);
}

void Button::_changeLevelSingle(uint16_t steps, bool invert, float time)
{
    int16_t levelChange = (IOT_DIMMER_MODULE_MAX_BRIGHTNESS / steps);
    __LDBG_printf("fadetime=%f/%f step=%u%% level=%d", time, time * 1.1, steps, invert ? -levelChange : levelChange);
    _changeLevel(invert ? -levelChange : levelChange, time * 1.1);
}

void Button::_changeLevelRepeat(uint16_t repeatTime, bool invert)
{
    float fadeTime = _dimmer._config._base.lp_fadetime;
    int16_t levelChange = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * ((repeatTime / 1000.0) / fadeTime);
    __LDBG_printf("fadetime=%f/%f repeat=%u level=%d", fadeTime, fadeTime * 1.1, repeatTime, invert ? -levelChange : levelChange);
    // to avoid choppy dimming, the time is 10% longer than the actual level change takes
    // once the button is released the fading will be stopped at the current level
    _changeLevel(invert ? -levelChange : levelChange, fadeTime * 1.1);
}

void Button::_setLevel(int32_t newLevel, float fadeTime)
{
    __LDBG_printf("#%u SET level=%d state=%d time=%f", _button, newLevel, _dimmer.getChannelState(_channel), fadeTime);
    if (_dimmer.getChannelState(_channel)) {
        _setLevel(newLevel, _dimmer.getChannel(_channel), fadeTime);
    }
}

void Button::_setLevel(int32_t newLevel, int16_t curLevel, float fadeTime)
{
    auto &config = _dimmer._getConfig();
    newLevel = std::clamp<int32_t>(
        newLevel,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config._base.min_brightness / 100,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config._base.max_brightness / 100
    );
    if (curLevel != newLevel) {
        __LDBG_printf("#%u setChannel channel=%d new_level=%d time=%f", _button, _channel, newLevel, -fadeTime);
        _dimmer.setChannel(_channel, newLevel, -fadeTime); // negative values for time will be passed directly to the dimmer
    }
}

void Button::_freezeLevel()
{
    _dimmer.stopFading(_channel);
}

#endif
