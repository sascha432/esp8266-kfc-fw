/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "remote_base.h"
#include "remote_event_queue.h"
#include "remote.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

namespace RemoteControl {

    // 0 {button_name}-down
    // 1 {button_name}-up
    // 2 {button_name}-press
    // 3 {button_name}-long-press
    // 4 {button_name}-single-click
    // 5 {button_name}-double-click
    // 6 {button_name}-multi-{repeat}-click
    // 7 {button_name}-hold
    // 8 {button_name}-hold-release

    static uint8_t buttonEventToEventNum(Button::EventType type)
    {
        using EventNameType = Plugins::RemoteControl::RemoteControl::EventNameType;
        switch(type) {
            case Button::EventType::DOWN:
                return static_cast<uint8_t>(EventNameType::BUTTON_DOWN);
            case Button::EventType::UP:
                return static_cast<uint8_t>(EventNameType::BUTTON_UP);
            case Button::EventType::PRESSED:
                return static_cast<uint8_t>(EventNameType::BUTTON_PRESS);
            case Button::EventType::LONG_PRESSED:
                return static_cast<uint8_t>(EventNameType::BUTTON_LONG_PRESS);
            case Button::EventType::SINGLE_CLICK:
                return static_cast<uint8_t>(EventNameType::BUTTON_SINGLE_CLICK);
            case Button::EventType::DOUBLE_CLICK:
                return static_cast<uint8_t>(EventNameType::BUTTON_DOUBLE_CLICK);
            case Button::EventType::REPEATED_CLICK:
                return static_cast<uint8_t>(EventNameType::BUTTON_MULTI_CLICK);
            case Button::EventType::HOLD_REPEAT:
                return static_cast<uint8_t>(EventNameType::BUTTON_HOLD_REPEAT);
            case Button::EventType::HOLD_RELEASE:
                return static_cast<uint8_t>(EventNameType::BUTTON_HOLD_RELEASE);
            default:
                break;
        }
        return 0;
    }

    static String buttonActionString(Button::EventType eventType, uint8_t buttonNum, uint16_t repeatCount)
    {
        PrintString str;

        auto eventNum = buttonEventToEventNum(eventType);
        if (eventNum) {
            auto eventName = Plugins::RemoteControl::getEventName(eventNum);
            if (eventName) {
                auto name = Plugins::RemoteControl::getName(buttonNum);
                if (name) {
                    str = eventName;
                    str.replace(F("{repeat}"), String(repeatCount));
                    str.replace(F("{button_name}"), name);
                }
            }
        }

        // str.printf_P(PSTR("%u-%u-%u-"));
        // auto name = Plugins::RemoteControl::getName(buttonNum);
        // if (name) {
        //     str += name;
        // } else {
        //     str += F("NULL");
        // }
        // str += '-';
        // auto eventName = Plugins::RemoteControl::getEventName(buttonNum);
        // if (eventName) {
        //     str += eventName;
        // } else {
        //     str += F("NULL");
        // }
        return str;
    }


    void Base::queueEvent(Button::EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, uint16_t actionId)
    {
        auto eventNum = buttonEventToEventNum(type);
        __DBG_printf("queueEvent type=%u button=%u event=%u enabled=%u bits=%u", type, buttonNum, eventNum, _getConfig().enabled[eventNum], _getConfig().actions[buttonNum].udp.event_bits);

        // if (_getConfig().events[eventNum].enabled) { // global enable flag

            // _getConfig().actions[buttonNum].getUdpBits();

            // auto udpAction = _getConfig().actions[buttonNum].hasUdpAction();

        // }

        if (_getConfig().enabled[eventNum]) { // global enable flag

            auto button = _getConfig().actions[buttonNum];
            if (_getConfig().udp_enable && button.hasUdpAction()) {
                auto action = buttonActionString(type, buttonNum, eventCount);
                if (action.length()) {
                    PrintString json(F("{\"device\":\"%s\",\"action\":\"%s\",\"event\":\"%s\",\"button\":%u,\"repeat\":%u,\"ts\":%u}"), System::Device::getName(), action.c_str(), Button::eventTypeToString(type), buttonNum, eventCount, eventTime);
#if DEBUG_IOT_REMOTE_CONTROL
                    String jsonCopy = json;
#endif
                    auto action = new ActionUDP(0, Payload::Json(std::move(json)), Plugins::RemoteControl::getUdpHost(), IPAddress(), _getConfig().udp_port);
                    __LDBG_printf("json=%s udpAction", jsonCopy.c_str());
                    _queue.emplace_back(type, buttonNum, Queue::Event::LockType::NONE, action);
                }
            }
            if (_getConfig().mqtt_enable && button.hasMQTTAction()) {
                PrintString str;
                PGM_P eventStr = nullptr;
                if (type == Button::EventType::DOWN && button.mqtt.event_down) {
                    eventStr = PSTR("down");
                }
                if (type == Button::EventType::UP && button.mqtt.event_up) {
                    eventStr = PSTR("up");
                }
                if (type == Button::EventType::PRESSED && button.mqtt.event_press) {
                    eventStr = PSTR("short_press");
                }
                if (type == Button::EventType::SINGLE_CLICK && button.mqtt.event_single_click) {
                    eventStr = PSTR("single_click");
                }
                if (type == Button::EventType::DOUBLE_CLICK && button.mqtt.event_double_click) {
                    eventStr = PSTR("double_click");
                }
                if (type == Button::EventType::REPEATED_CLICK && button.mqtt.event_multi_click) {
                    str = PrintString(F("repeated_%u_click"), eventCount);
                    eventStr = str.c_str();
                }
                if (type == Button::EventType::LONG_PRESSED && button.mqtt.event_long_press) {
                    eventStr = PSTR("long_press");
                }
                if (type == Button::EventType::HOLD_REPEAT && button.mqtt.event_hold) {
                    eventStr = PSTR("hold_repeat");
                }
                if (type == Button::EventType::HOLD_RELEASE && button.mqtt.event_hold_released) {
                    eventStr = PSTR("hold_release");
                }
                if (eventStr) {
                    auto action = new ActionMQTT(0, PrintString(F("button_%u_%s"), buttonNum + 1, eventStr));
                    __LDBG_printf("payload=%s mqttAction", action->getPayload().c_str());
                    _queue.emplace_back(type, buttonNum, Queue::Event::LockType::NONE, action);
                }

            }

        }


//         if (_getConfig().events[eventNum].enabled) { // global enable flag

//         if (!skip) {
//             auto action = Button::actionString(type, buttonNum, eventCount);
//             if (action) {
//                 PrintString json(F("{\"device\":\"%s\",\"action\":\"%s\",\"event\":\"%s\",\"button\":%u,\"repeat\":%u,\"ts\":%u}"), System::Device::getName(), action, Button::eventTypeToString(type), buttonNum, eventCount, eventTime);
// #if DEBUG_IOT_REMOTE_CONTROL
//                 String jsonCopy = json;
// #endif
//                 auto action = new ActionUDP(0, Payload::Json(std::move(json)), Plugins::RemoteControl::getUdpHost(), IPAddress(), _getConfig().udp_port);
//                 __LDBG_printf("json=%s", jsonCopy.c_str());
//                 _queue.emplace_back(type, buttonNum, Queue::Event::LockType::NONE, action);
//             }
//         }

        // TODO add actions to target

        // auto event = Queue::Event(type, buttonNum, eventCount, eventTime, _pressed, target);

        // __LDBG_printf();

        if (_queue.size() && !_queueTimer) {
            _queueTimer.add(25, true, [this](Event::CallbackTimerPtr timer) {
                timerCallback(timer);
            });
        }

    }

    void Base::timerCallback(Event::CallbackTimerPtr timer)
    {
        if (!WiFi.isConnected()) {
            // __LDBG_printf("WiFi not connected, delaying queue");
            _resetAutoSleep();
            return;
        }
        if (_lock.isAnyLocked(nullptr)) {
            _resetAutoSleep();
            return;
        }
        for(auto &item: _queue) {
            if (item.canSend()) {
                if (!_lock.isLocked(item.getButtonNum(), &item.getAction())) {
                    item.send(_lock);
                    _resetAutoSleep();
                    if (_lock.isAnyLocked(nullptr)) {
                        return;
                    }
                }
            }
        }

#if DEBUG_IOT_REMOTE_CONTROL && 0
        auto count = _queue.size();
#endif
        noInterrupts();
        _queue.erase(std::remove_if(_queue.begin(), _queue.end(), [](const Queue::Event &event) {
            return event.canDelete();
        }), _queue.end());
        interrupts();
#if DEBUG_IOT_REMOTE_CONTROL && 0
        if (count != _queue.size()) {
            __DBG_printf("%u items removed from queue", count - _queue.size());
        }
#endif

        if (_queue.empty()) {
            timer->disarm();
        }
    }

}
