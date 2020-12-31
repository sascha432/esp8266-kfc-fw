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


// String Button::actionString(EventType eventType, uint8_t buttonNum, uint16_t repeatCount)
// //const __FlashStringHelper *actionString(EventType eventType, uint8_t buttonNum, uint16_t repeatCount)
// {
//     // switch(eventType) {
//     //     case EventType::REPEATED_CLICK:
//     //         if (repeatCount != 1) {
//     //             break;
//     //         }
//     //     case EventType::SINGLE_CLICK:
//     //     case EventType::PRESSED:
//     //         switch(buttonNum) {
//     //             case 0:
//     //                 return F("on-press");
//     //             case 1:
//     //                 return F("up-press");
//     //             case 2:
//     //                 return F("down-press");
//     //             case 3:
//     //                 return F("off-press");
//     //         }
//     //         break;
//     //     case EventType::DOWN:
//     //         switch(buttonNum) {
//     //             case 0:
//     //                 return F("on-down");
//     //             case 1:
//     //                 return F("up-down");
//     //             case 2:
//     //                 return F("down-down");
//     //             case 3:
//     //                 return F("off-down");
//     //         }
//     //         break;
//     //     case EventType::UP:
//     //         switch(buttonNum) {
//     //             case 0:
//     //                 return F("on-release");
//     //             case 1:
//     //                 return F("up-release");
//     //             case 2:
//     //                 return F("down-release");
//     //             case 3:
//     //                 return F("off-release");
//     //         }
//     //         break;
//     //     case EventType::LONG_PRESSED:
//     //         switch(buttonNum) {
//     //             case 0:
//     //                 return F("on-long-press");
//     //             case 1:
//     //                 return F("up-long-press");
//     //             case 2:
//     //                 return F("down-long-press");
//     //             case 3:
//     //                 return F("off-long-press");
//     //         }
//     //         break;
//     //     case EventType::HELD:
//     //         switch(buttonNum) {
//     //             case 0:
//     //                 return F("on-hold");
//     //             case 1:
//     //                 return F("up-hold");
//     //             case 2:
//     //                 return F("down-hold");
//     //             case 3:
//     //                 return F("off-hold");
//     //         }
//     //         break;
//     //     default:
//     //         break;
//     // }
//     // return nullptr;
//     return emptyString;
// }

void Button::event(EventType eventType, uint32_t now)
{
    auto &base = *getBase();
    auto &config = base._getConfig();
    base._resetAutoSleep();

    switch (eventType) {
        case EventType::DOWN:
            base._pressed |= base._getPressedMask(_button);
            base.queueEvent(eventType, _button, _getEventTimeForFirstEvent(), config.actions[_button].down);
            break;
        case EventType::UP:
            base._pressed &= ~base._getPressedMask(_button);
            base.queueEvent(eventType, _button, _getEventTime(), config.actions[_button].up);
            break;
        case EventType::PRESSED:
            base.queueEvent(eventType, _button, _getEventTime(), config.actions[_button].press);
            break;
        case EventType::LONG_PRESSED:
            base.queueEvent(eventType, _button, _getEventTime(), config.actions[_button].long_press);
            break;
        case EventType::HELD:
            base.queueEvent(eventType, _button, _repeatCount, _getEventTime(), config.actions[_button].hold);
            break;
#if 1
         case EventType::SINGLE_CLICK:
            base.queueEvent(eventType, _button, _getEventTime(), config.actions[_button].single_click);
            break;
        case EventType::DOUBLE_CLICK:
            base.queueEvent(eventType, _button, _getEventTime(), config.actions[_button].double_click);
            break;
#endif
#if 0
        case EventType::REPEATED_CLICK: {
                switch(_repeatCount) {
                    case 1:
                        base.queueEvent(eventType, _button, _repeatCount, _getEventTime(), config.actions[_button].single_click);
                        break;
                    case 2:
                        base.queueEvent(eventType, _button, _repeatCount, _getEventTime(), config.actions[_button].double_click);
                        break;
                    default:
                        int8_t idx = config.actions[_button].getMultiClickIndex(_repeatCount);
                        base.queueEvent(eventType, _button, _repeatCount, _getEventTime(), (idx != -1) ? config.actions[_button].multi_click[idx].action : 0);
                        break;
                }
            }
            break;
#endif
        default:
            break;
    }

#if 1
    __LDBG_printf("%s event_type=%s (%02x) button#=%u now=%u pressed=%s",
        name(),
        eventTypeToString(eventType),
        eventType,
        _button,
        now,
        base._getPressedButtons()
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
