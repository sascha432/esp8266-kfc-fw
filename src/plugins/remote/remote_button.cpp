/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "remote_button.h"
#include "remote_action.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace RemoteControl;

void Button::event(EventType eventType, uint32_t now)
{
    ActionIdType actionId = -1;
    auto &config = getBase()->_getConfig();

    switch (eventType) {
        case EventType::DOWN:
            getBase()->_pressed |= getBase()->_getPressedMask(_button);
            break;
        case EventType::UP:
            getBase()->_pressed &= ~getBase()->_getPressedMask(_button);
            break;
        case EventType::SINGLE_CLICK:
            actionId = config.actions[_button].single_click;
            break;
        case EventType::LONG_PRESSED:
            actionId = config.actions[_button].double_click;
            break;
        case EventType::REPEATED_CLICK: {
                int8_t idx = config.actions[_button].getMultiClickIndex(this->getNum());
                if (idx != -1) {
                    actionId = config.actions[_button].multi_click[idx].action;
                }
            }
            break;
        default:
            break;
    }

    __DBG_printf("%s event_type=%s (%02x) button#=%u now=%u pressed=%s action_id=%d",
        name(),
        eventTypeToString(eventType),
        eventType,
        _button,
        now,
        getBase()->_getPressedButtons(),
        actionId
    );

    // //auto &config = getBase()->_getConfig();
    // switch (eventType) {
    //     case EventType::PRESSED:
    //         _pressed = true;
    //         break;

    //     case EventType::RELEASED:
    //         _pressed = false;
    //         //getBase()->_onButtonReleased(*this, );
    //         break;

    //     // case EventType::DOWN:
    //     //     if (_singleClickGroup->getRepeatCount() == 0) {
    //     //         _level = _dimmer.getChannel(_channel);
    //     //         __LDBG_printf("%s READ_LEVEL event=%s sp_count=%u level=%u", name(), eventTypeToString(eventType), _singleClickGroup->getRepeatCount(), _level);
    //     //     }
    //     //     else {
    //     //         __LDBG_printf("%s IGNORED event=%s sp_count=%u", name(), eventTypeToString(eventType), _singleClickGroup->getRepeatCount());
    //     //     }
    //     //     break;
    //     // case EventType::REPEAT:
    //     //     _changeLevelRepeat(IOT_DIMMER_MODULE_HOLD_REPEAT_TIME, _button == 1);
    //     //     break;
    //     // case EventType::CLICK:
    //     //     if (_dimmer.getChannelState(_channel)) {
    //     //         // channel is on
    //     //         _changeLevelSingle(_singleClickSteps, _button == 1);
    //     //     }
    //     //     else if (_button == 0) {
    //     //         __LDBG_printf("%s ON duration=%u", name(), _duration);
    //     //         // button 0 turns channel on
    //     //         // button 1 turns channel off, using DOWN and SINGLE_CLICK
    //     //         _dimmer.on(_channel);
    //     //     }
    //     //     else {
    //     //         __LDBG_printf("%s IGNORED event=%s state=%d", name(), eventTypeToString(eventType), _dimmer.getChannelState(_channel));
    //     //     }
    //     //     break;
    //     // case EventType::LONG_CLICK:
    //     //     _setLevel(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * (int32_t)(_button == 0 ? config.longpress_max_brightness : config.longpress_min_brightness) / 100, config.lp_fadetime);
    //     //     break;
    //     // case EventType::SINGLE_CLICK:
    //     //     if (_dimmer._channels[_channel].off(&config, _level)) {
    //     //         __LDBG_printf("%s OFF store_level=%d duration=%u", name(), _level, _duration);
    //     //         _dimmer._channels[_channel].setStoredBrightness(_level);
    //     //     }
    //     //     break;
    //     default:
    //         // __LDBG_printf("%s IGNORED event=%s", name(), eventTypeToString(eventType));
    //         break;
    // }
    // __LDBG_printf("%s event=%s", name(), eventTypeToString(eventType));
}

void Button::updateConfig()
{
    auto &config = getBase()->_getConfig();
    _subscribedEvents = ButtonConfig::_getEventTypes(config.actions[_button], false);
    _clickTime = config.shortpressTime;
    _longpressTime = config.longpressTime;
    _repeatTime = config.repeatTime;
    // reset states
    _reset();
    // reset bit in pressed indicator
    getBase()->_pressed &= ~getBase()->_getPressedMask(_button);;
}
