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


DimmerButton::DimmerButton(uint8_t pin, uint8_t channel, uint8_t button, DimmerButtons &dimmer, SingleClickGroupPtr singleClickGroup) :
    PushButton(pin, &dimmer, std::move(DimmerButtonConfig(dimmer._getConfig())), singleClickGroup, dimmer._getConfig().pin_inverted(channel, button) ? ActiveStateType::INVERTED : ActiveStateType::NON_INVERTED),
    _dimmer(dimmer),
    _level(0),
    _channel(channel),
    _button(button)
{
    if (_button == 1) {
        _subscribedEvents = EnumHelper::Bitset::addBits(_subscribedEvents, EventType::DOWN, EventType::SINGLE_CLICK);
    }
#if DEBUG_PIN_MONITOR
    setName(PrintString(F("%s:%u"), _button == 0 ? PSTR("BUTTON-UP") : PSTR("BUTTON-DOWN"), _channel));
#endif
}

void DimmerButton::event(EventType eventType, uint32_t now)
{
    auto &config = _dimmer._getConfig();
    switch (eventType) {
        case EventType::DOWN:
            if (_singleClickGroup->getRepeatCount() == 0) {
                _level = _dimmer.getChannel(_channel);
                __LDBG_printf("%s READ_LEVEL event=%s sp_count=%u level=%u", name(), eventTypeToString(eventType), _singleClickGroup->getRepeatCount(), _level);
            }
            else {
                __LDBG_printf("%s IGNORED event=%s sp_count=%u", name(), eventTypeToString(eventType), _singleClickGroup->getRepeatCount());
            }
            break;
        case EventType::REPEATED_CLICK:
            _changeLevelRepeat(IOT_DIMMER_MODULE_HOLD_REPEAT_TIME, _button == 1);
            break;
        case EventType::CLICK:
            if (_dimmer.getChannelState(_channel)) {
                // channel is on
                _changeLevelSingle(_singleClickSteps, _button == 1);
            }
            else if (_button == 0) {
                __LDBG_printf("%s ON duration=%u", name(), _duration);
                // button 0 turns channel on
                // button 1 turns channel off, using DOWN and SINGLE_CLICK
                _dimmer.on(_channel);
            }
            else {
                __LDBG_printf("%s IGNORED event=%s state=%d", name(), eventTypeToString(eventType), _dimmer.getChannelState(_channel));
            }
            break;
        case EventType::LONG_CLICK:
            _setLevel(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * (int32_t)(_button == 0 ? config.longpress_max_brightness : config.longpress_min_brightness) / 100, config.lp_fadetime);
            break;
        case EventType::SINGLE_CLICK:
            if (_dimmer._channels[_channel].off(&config, _level)) {
                __LDBG_printf("%s OFF store_level=%d duration=%u", name(), _level, _duration);
                _dimmer._channels[_channel].setStoredBrightness(_level);
            }
            break;
        default:
            __LDBG_printf("%s IGNORED event=%s", name(), eventTypeToString(eventType));
            break;
    }
}

void DimmerButton::_changeLevel(int32_t change, float fadeTime)
{
    int32_t curLevel = _dimmer.getChannel(_channel);
    _setLevel(curLevel + change, curLevel, fadeTime);
}

void DimmerButton::_changeLevelSingle(uint16_t steps, bool invert)
{
    int16_t levelChange = (IOT_DIMMER_MODULE_MAX_BRIGHTNESS / steps);
    __LDBG_printf("fadetime=%f/%f step=%u%% level=%d", _dimmer._config.lp_fadetime, _dimmer._config.lp_fadetime * 1.048, steps, invert ? -levelChange : levelChange);
    _changeLevel(invert ? -levelChange : levelChange, _dimmer._config.lp_fadetime * 1.048);
}

void DimmerButton::_changeLevelRepeat(uint16_t repeatTime, bool invert)
{
    float fadeTime = _dimmer._config.lp_fadetime;
    int16_t levelChange = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * ((repeatTime / 1000.0) / fadeTime);
    __LDBG_printf("fadetime=%f/%f repeat=%u level=%d", fadeTime, fadeTime * 1.024, repeatTime, invert ? -levelChange : levelChange);
    _changeLevel(invert ? -levelChange : levelChange, fadeTime * 1.024);
}

void DimmerButton::_setLevel(int32_t newLevel, float fadeTime)
{
    __LDBG_printf("%s SET level=%d state=%d time=%f", name(), newLevel, _dimmer.getChannelState(_channel), fadeTime);
    if (_dimmer.getChannelState(_channel)) {
        _setLevel(newLevel, _dimmer.getChannel(_channel), fadeTime);
    }
}

void DimmerButton::_setLevel(int32_t newLevel, int16_t curLevel, float fadeTime)
{
    newLevel = std::clamp<int32_t>(
        newLevel,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _dimmer._getConfig().min_brightness / 100,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS
    );
    if (curLevel != newLevel) {
        _dimmer.setChannel(_channel, newLevel, fadeTime);
    }
}

#endif
