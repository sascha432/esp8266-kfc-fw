/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <BitsToStr.h>
#include "remote_button.h"
#include "remote_action.h"

#if DEBUG_IOT_REMOTE_CONTROL && 0
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace RemoteControl;

void Button::event(EventType eventType, uint32_t now)
{
    auto &base = *getBase();
    auto &config = base._getConfig();
    base._resetAutoSleep();

    switch (eventType) {
        case EventType::DOWN:
            base._pressed |= base._getPressedMask(_button);
            base.queueEvent(eventType, _button, 0, _getEventTimeForFirstEvent(), config.actions[_button].down);
            break;
        case EventType::UP:
            base._pressed &= ~base._getPressedMask(_button);
            base.queueEvent(eventType, _button, 0, _getEventTime(), config.actions[_button].up);
            break;
        case EventType::PRESSED:
            base.queueEvent(eventType, _button, 0, _getEventTime(), config.actions[_button].press);
            break;
        case EventType::LONG_PRESSED:
            base.queueEvent(eventType, _button, 0, _getEventTime(), config.actions[_button].long_press);
            break;
        case EventType::HOLD_REPEAT:
            base.queueEvent(eventType, _button, _repeatCount, _getEventTime(), config.actions[_button].hold);
            break;
        case EventType::HOLD_RELEASE:
            base.queueEvent(eventType, _button, 0, _getEventTime(), config.actions[_button].hold);
            break;
         case EventType::SINGLE_CLICK:
            base.queueEvent(eventType, _button, 0, _getEventTime(), config.actions[_button].single_click);
            break;
        case EventType::DOUBLE_CLICK:
            base.queueEvent(eventType, _button, 0, _getEventTime(), config.actions[_button].double_click);
            break;
        case EventType::REPEATED_CLICK: {
                switch(_repeatCount) {
                    case 1:
                        base.queueEvent(EventType::SINGLE_CLICK, _button, 1, _getEventTime(), config.actions[_button].single_click);
                        break;
                    case 2:
                        base.queueEvent(EventType::DOUBLE_CLICK, _button, 2, _getEventTime(), config.actions[_button].double_click);
                        break;
                    default:
                        base.queueEvent(eventType, _button, _repeatCount, _getEventTime(), 0);
                        // int8_t idx = config.actions[_button].getMultiClickIndex(_repeatCount);
                        // base.queueEvent(eventType, _button, _repeatCount, _getEventTime(), (idx != -1) ? config.actions[_button].multi_click[idx].action : 0);
                        break;
                }
            }
            break;
        default:
            break;
    }

#if 0
    __DBG_printf("event_type=%s (%02x) button#=%u first_time=%u time=%u pressed=%s",
        eventTypeToString(eventType),
        eventType,
        _button,
        _getEventTimeForFirstEvent(),
        _getEventTime(),
        BitsToStr<_buttonPins.size(), true>(base._pressed).c_str()
    );
#endif

}

void Button::updateConfig()
{
    auto &base = *getBase();
    auto &config = base._getConfig();
    _subscribedEvents = ButtonConfig::kDefaultEvents;
    _clickTime = config.click_time;
    _longpressTime = config.hold_time;
    _repeatTime = config.hold_repeat_time;
    // reset states
    _reset();
    // reset bit in pressed indicator
    base._pressed &= ~base._getPressedMask(_button);
}
