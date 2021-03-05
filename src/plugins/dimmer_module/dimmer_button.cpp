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

#if DEBUG_IOT_DIMMER_MODULE && DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Dimmer;


Button::Button(uint8_t pin, uint8_t channel, uint8_t button, Buttons &dimmer, SingleClickGroupPtr singleClickGroup) :
    PushButton(pin, &dimmer, std::move(ButtonConfig(dimmer._getConfig())), singleClickGroup, dimmer._getConfig().pin_inverted(channel, button) ? ActiveStateType::INVERTED : ActiveStateType::NON_INVERTED),
    _dimmer(dimmer),
    _level(0),
    _channel(channel),
    _button(button)
{
    if (dimmer._getConfig().longpress_time == 0) {
        _subscribedEvents = EnumHelper::Bitset::removeBits(_subscribedEvents, EventType::LONG_PRESSED);
    }
    if (_button == 1) {
        _subscribedEvents = EnumHelper::Bitset::addBits(_subscribedEvents, EventType::SINGLE_CLICK);
    }
#if DEBUG_PIN_MONITOR
    setName(PrintString(F("%s:%u"), _button == 0 ? PSTR("BUTTON-UP") : PSTR("BUTTON-DOWN"), _channel));
#endif
}

void Button::event(EventType eventType, uint32_t now)
{
    __DBG_IF(
        auto oldLevel = _dimmer.getChannel(_channel);
    );
    auto &config = _dimmer._getConfig();
    auto repeatCount = _singleClickGroup->getRepeatCount();
    switch (eventType) {
        case EventType::DOWN:
            // store level first
            if (_singleClickGroup->getRepeatCount() == 0) {
                _level = _dimmer.getChannel(_channel);
            }
            if (_dimmer.getChannelState(_channel)) {
                _changeLevelSingle(_singleClickSteps, _button == 1, config.lp_fadetime); //125 * _singleClickSteps / 1000.0);
            }
            else if (_button == 0) {
                _dimmer.on(_channel);
            }
            break;
        case EventType::HOLD_REPEAT:
            _changeLevelRepeat(IOT_DIMMER_MODULE_HOLD_REPEAT_TIME, _button == 1);
            break;
        case EventType::SINGLE_CLICK:
            // button 1 only
            if (_dimmer._channels[_channel].off(&config, _level)) {
                __LDBG_printf("%s OFF store_level=%d duration=%u", name(), _level, _duration);
            }
            break;
        case EventType::LONG_CLICK: {
                int32_t level = _button == 0 ? config.longpress_max_brightness : config.longpress_min_brightness;
                if (level != 0) {
                    level = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * level / 100;
                    if (!_dimmer.getChannelState(_channel)) {
                        _dimmer.on(_channel);
                    }
                    _setLevel(level, config.lp_fadetime);
                }
            }
            break;
        default:
            __LDBG_printf("%s IGNORED event=%s", name(), eventTypeToString(eventType));
            break;
    }
    __DBG_IF(
        __DBG_printf("type=%s (%02x) repeat=%u btn=%u dur=%u level=%u new=%u", eventTypeToString(eventType), eventType, repeatCount, _button, _duration, oldLevel, _dimmer.getChannel(_channel));
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
    __LDBG_printf("fadetime=%f/%f step=%u%% level=%d", _dimmer._config.lp_fadetime, _dimmer._config.lp_fadetime * 1.048, steps, invert ? -levelChange : levelChange);
    _changeLevel(invert ? -levelChange : levelChange, _dimmer._config.lp_fadetime * -1.048);
}

void Button::_changeLevelRepeat(uint16_t repeatTime, bool invert)
{
    float fadeTime = _dimmer._config.lp_fadetime;
    int16_t levelChange = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * ((repeatTime / 1000.0) / fadeTime);
    __LDBG_printf("fadetime=%f/%f repeat=%u level=%d", fadeTime, fadeTime * 1.024, repeatTime, invert ? -levelChange : levelChange);
    _changeLevel(invert ? -levelChange : levelChange, fadeTime * 1.024);
}

void Button::_setLevel(int32_t newLevel, float fadeTime)
{
    __LDBG_printf("%s SET level=%d state=%d time=%f", name(), newLevel, _dimmer.getChannelState(_channel), fadeTime);
    if (_dimmer.getChannelState(_channel)) {
        _setLevel(newLevel, _dimmer.getChannel(_channel), fadeTime);
    }
}

void Button::_setLevel(int32_t newLevel, int16_t curLevel, float fadeTime)
{
    auto &config = _dimmer._getConfig();
    newLevel = std::clamp<int32_t>(
        newLevel,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.min_brightness / 100,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS * config.max_brightness / 100
    );
    if (curLevel != newLevel) {
        _dimmer.setChannel(_channel, newLevel, -fadeTime);
    }
}

#endif
