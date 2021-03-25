/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <BitsToStr.h>
#include "remote_base.h"
#include "remote_event_queue.h"
#include "remote.h"
#include "blink_led_timer.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

namespace RemoteControl {

    static String buttonActionString(Button::EventType eventType, uint8_t buttonNum, uint16_t repeatCount)
    {
        auto eventNum = getEventTypeInt(eventType);
        if (eventNum != static_cast<uint16_t>(EventNameType::ANY)) {
            auto eventName = Plugins::RemoteControl::getEventName(eventNum);
            if (eventName && *eventName) {
                auto name = Plugins::RemoteControl::getName(buttonNum);
                if (name && *name) {
                    String str = eventName;
                    str.replace(F("{repeat}"), String(repeatCount));
                    str.replace(F("{button_name}"), name);
                    return str;
                }
            }
        }
        __DBG_printf("eventNum=%u buttonNum=%u event name or name missing", eventNum, buttonNum);

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
        return String();
    }

    String Base::createJsonEvent(const String &action, EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, bool reset, const char *combo) const
    {
        if (action.length()) {
            PrintString json(F("{\"device\":\""));
            json.print(System::Device::getName());
            json.print(F("\",\"action\":\""));
            json.print(action);
            json.print(F("\",\"event\":\""));
            json.print(FPSTR(Button::eventTypeToString(type)));
            json.printf_P(PSTR("\",\"button\":%u"), buttonNum);
            if (eventCount) {
                json.printf_P(PSTR(",\"repeat\":%u"), eventCount);
            }
            // if (reset) {
            //     json.print(F(",\"reset\":true"));

            // }
            // if (combo) {
            //     json.printf_P(PSTR(",\"combo\":\"%s\""), combo);
            // }
            json.printf_P(PSTR(",\"ts\":%u}"), eventTime);
            return json;
        }
        return String();
    }

    void Base::queueEvent(EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, uint16_t actionId)
    {
        auto eventNum = buttonEventToEventType(type);
        __LDBG_printf("queueEvent type=%u button=%u event=%s enabled=%s udp=%s mqtt=%s mask=%s", type, buttonNum, Button::eventTypeToString(type),
            BitsToStr<Plugins::RemoteControl::Events::kBitCount, false>(_getConfig().enabled.event_bits).c_str(),
            BitsToStr<Plugins::RemoteControl::Events::kBitCount, false>(_getConfig().actions[buttonNum].udp.event_bits).c_str(),
            BitsToStr<Plugins::RemoteControl::Events::kBitCount, false>(_getConfig().actions[buttonNum].mqtt.event_bits).c_str(),
            BitsToStr<Plugins::RemoteControl::Events::kBitCount, false>(getEventBitmask(type)).c_str()
        );

        if (_getConfig().enabled[eventNum]) { // global enable flag

            auto button = _getConfig().actions[buttonNum];
            if (_getConfig().udp_enable && button.udp[eventNum]) { //(button.udp.event_bits & getEventBitmask(type))) {
                auto json = createJsonEvent(
                    buttonActionString(type, buttonNum, eventCount),
                    type,
                    buttonNum,
                    eventCount,
                    eventTime,
                    (type == EventType::RELEASED || type == EventType::HOLD_RELEASE),
                    BitsToStr<kButtonPins.size(), true>(_pressed).c_str()
                );
                __LDBG_assert_printf(json.length() != 0, "json payload empty, type %u, eventNum %u", type, eventNum);

                __LDBG_printf("ActionUDP json=%s", json.c_str());
                auto action = new ActionUDP(0, std::move(json), Plugins::RemoteControl::getUdpHost(), IPAddress(), _getConfig().udp_port);
                _queue.emplace_back(type, buttonNum, Queue::Event::LockType::NONE, action);
            }
            if (_getConfig().mqtt_enable && button.mqtt[eventNum]) { // (button.mqtt.event_bits & getEventBitmask(type))) {

                auto action = new ActionMQTT(0, buttonActionString(type, buttonNum, eventCount));
                __LDBG_printf("ActionMQTT payload=%s", action->getPayload().c_str());
                _queue.emplace_back(type, buttonNum, Queue::Event::LockType::NONE, action);

            }
        }

        if (_queue.size() && !_queueTimer) {
            __LDBG_printf("arming queue timer");
            _queueTimer.add(25, true, [this](Event::CallbackTimerPtr timer) {
                timerCallback(timer);
            }, Event::PriorityType::HIGHEST);
        }

    }

    void Base::timerCallback(Event::CallbackTimerPtr timer)
    {
        if (!config.getWiFiUp()) {
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

#if DEBUG_IOT_REMOTE_CONTROL
        auto count = _queue.size();
#endif
        noInterrupts();
        _queue.erase(std::remove_if(_queue.begin(), _queue.end(), [](const Queue::Event &event) {
            return event.canDelete();
        }), _queue.end());
        interrupts();
#if DEBUG_IOT_REMOTE_CONTROL
        if (count != _queue.size()) {
            __LDBG_printf("%u items removed from queue", count - _queue.size());
        }
#endif

        if (_queue.empty()) {
            __LDBG_printf("disarming queue timer");
            timer->disarm();
        }
    }

    void Base::event(BaseEventType type, StateType state)
    {
        switch (type) {
        case BaseEventType::CHARGE_DETECTION: {
            bool newState = state & StateType::IS_HIGH;
#if 0
            __LDBG_printf("event new_state=%u pin_state=%u pin=%u", newState, digitalRead(IOT_REMOTE_CONTROL_CHARGING_PIN), IOT_REMOTE_CONTROL_CHARGING_PIN);
            delay(5); // check pin after 5ms, simple debouncing for prototype board that is using a switch to toggle the charging state
            if (digitalRead(IOT_REMOTE_CONTROL_CHARGING_PIN) != newState) {
                __LDBG_printf("charge detection pin bounce");
                break;
            }
#endif
            __LDBG_printf("is_charging=%u new_charging_state=%u has_changed=%u pin_state=%u", _isCharging, newState, (newState != _isCharging), digitalRead(IOT_REMOTE_CONTROL_CHARGING_PIN));
            if (newState != _isCharging) {
                _isCharging = newState;
                RemoteControlPlugin::getInstance()._chargingStateChanged(_isCharging);
                if (!_isCharging) {
                    if (millis() > __autoSleepTimeout) { // stay awake for 30 seconds after disconnecting the charger
                        _resetAutoSleep();
                        _setAutoSleepTimeout(millis() + 30000, ~0U);
                    }
                }
            }
        } break;
        case BaseEventType::NONE:
            break;
        }
    }

}
