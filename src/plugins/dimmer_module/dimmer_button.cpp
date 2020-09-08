/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include <MicrosTimer.h>
#include "dimmer_button.h"
#include "dimmer_buttons.h"
#include "dimmer_module.h"
// #include <EnumHelper.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


DimmerButton::DimmerButton(uint8_t pin, uint8_t channel, uint8_t button, DimmerButtons &dimmer) :
    PushButton(pin, &dimmer, std::move(DimmerButtonConfig(dimmer._getConfig())), IOT_SWITCH_PRESSED_STATE),
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

void DimmerButton::event(EventType eventType, TimeType now)
{
    auto &config = _dimmer._getConfig();
    switch (eventType) {
        case EventType::DOWN:
            if (_clickRepeatCount == 0) {
                _level = _dimmer.getChannel(_channel);
                __LDBG_printf("%s READ_LEVEL event=%s no_repeat_count=%u level=%u", name(), eventTypeToString(eventType), _clickRepeatCount, _level);
            }
            else {
                __LDBG_printf("%s IGNORED event=%s no_repeat_count=%u", name(), eventTypeToString(eventType), _clickRepeatCount);
            }
            break;
        case EventType::REPEAT:
            _changeLevel(config.shortpress_step);
            break;
        case EventType::CLICK:
            if (_dimmer.getChannelState(_channel)) {
                // channel is on
                _changeLevel(config.shortpress_step);
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
            _setLevel(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * (int32_t)(_button == 0 ? config.longpress_max_brightness : config.longpress_min_brightness) / 100, config.longpress_fadetime);
            break;
        case EventType::SINGLE_CLICK:
            if (_dimmer.off(_channel)) {
                __LDBG_printf("%s OFF store_level=%d duration=%u", name(), _level, _duration);
                _dimmer._channels[_channel].setStoredBrightness(_level);
            }
            else {
                __LDBG_printf("%s OFF level=0 duration=%u", name(), _duration);
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
    __LDBG_printf("%s CHANGE level=%d -> %d state=%d time=%f repeat_time=%u", name(), curLevel, curLevel + change, _dimmer.getChannelState(_channel), fadeTime, _repeatTime);
    _setLevel(curLevel + change, curLevel, fadeTime);
}

void DimmerButton::_changeLevel(uint16_t stepSize)
{
    // for repeated smooth level changes calculate the fadetime from repeat time adding 10% or max. 10ms on top
    int16_t levelChange = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * stepSize / 100;
    uint32_t time = _repeatTime + std::max(10U, _repeatTime / 10U);
    float fadeTime = _dimmer._getAbsoluteFadeTime(time, levelChange);
    // increase level for button 0, decrease for button 1
    if (_button == 1) {
        levelChange = -levelChange;
    }
    __LDBG_printf("fadetime=%f repeat=%u used_time=%u step=%u%% level=%d", fadeTime, _repeatTime, time, stepSize, levelChange);
    _changeLevel(levelChange, fadeTime);
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
    newLevel = std::clamp_signed(
        newLevel,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _dimmer._getConfig().min_brightness / 100,
        IOT_DIMMER_MODULE_MAX_BRIGHTNESS
    );
    if (curLevel != newLevel) {
        _dimmer.setChannel(_channel, newLevel, fadeTime);
    }
}

#endif
