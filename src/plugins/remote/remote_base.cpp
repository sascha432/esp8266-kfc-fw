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

#if DEBUG_IOT_REMOTE_CONTROL && 0
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

    //
    // press first and last button to activate the system button combination
    //
    // holding both buttons for more than 5 seconds restarts the system. if kept pressed during reboot, it will
    // go into the same mode again and repeat...
    //
    // once the buttons have been released, the menu is active, the LED stops flickering and starts blinking slowly.
    // the menu is active for 10 seconds after the last button press
    //
    // 1st button short press: enable auto sleep and exit menu - confirmed by 1x blink
    // 1st button long press: disable auto sleep for remote access and exit menu - confirmed by 2x blink
    // 4th button click: exit menu - confirmed by flickerung


    void Base::systemButtonComboEvent(bool state, EventType eventType, uint8_t button, uint16_t repeatCount, uint32_t eventTime)
    {
        if (state) {
            _systemButtonComboTime = millis();
            _systemButtonComboTimeout = _systemButtonComboTime + kSystemComboRebootTimeout;
            __DBG_printf("PRESSED");
            _systemComboNextState(ComboButtonStateType::PRESSED);
        }
        else {
            bool acceptEvent = (_systemButtonComboState != ComboButtonStateType::PRESSED) && (millis() > _systemButtonComboTime);
#if 1
            if (acceptEvent) {
                __DBG_printf("event_type=%s (%ux) button#=%u pressed=%s time=%u",
                    PinMonitor::PushButton::eventTypeToString(eventType),
                    repeatCount,
                    button,
                    BitsToStr<kButtonPins.size(), true>(_pressed).c_str(),
                    eventTime
                );

            }
#endif
            switch(eventType) {
                case EventType::DOWN:
                    if (acceptEvent && _systemButtonComboState == ComboButtonStateType::TIMEOUT) {
                        _systemComboNextState(ComboButtonStateType::RESET_MENU_TIMEOUT);
                    }
                    _pressed |= _getPressedMask(button);
                    break;
                case EventType::UP:
                    _pressed &= ~_getPressedMask(button);
                    break;
                case EventType::PRESSED:
                    if (acceptEvent) {
                        if (button == 0) {
                            _systemComboNextState(ComboButtonStateType::CONFIRM_AUTO_SLEEP_OFF);
                        }
                        else  if (button == 1) {
                            _systemComboNextState(ComboButtonStateType::CONFIRM_AUTO_SLEEP_ON);
                        }
                        else  if (button == 2) {
                            _systemComboNextState(ComboButtonStateType::CONFIRM_DEEP_SLEEP);
                        }
                        else  if (button == 3) {
                            _systemComboNextState(ComboButtonStateType::CONFIRM_EXIT);
                        }
                    }
                    break;
                case EventType::LONG_PRESSED:
                    break;
                case EventType::HOLD_REPEAT:
                    break;
                case EventType::HOLD_RELEASE:
                    break;
                // case EventType::SINGLE_CLICK:
                //     break;
                // case EventType::DOUBLE_CLICK:
                //     break;
                case EventType::REPEATED_CLICK: {
                        switch(repeatCount) {
                            case 1: // EventType::SINGLE_CLICK
                                break;
                            case 2: // EventType::DOUBLE_CLICK
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                default:
                    break;
            }
            if ((_pressed & kButtonSystemComboBitMask) == 0 && _systemButtonComboState == ComboButtonStateType::PRESSED) { // wait until the last button has been released
                __DBG_printf("released");
                _systemComboNextState(ComboButtonStateType::RELEASED);
                _systemButtonComboTime = millis() + 750; // ignore all events
                // reset state and debouncer
                // _resetButtonState();
                // _pressed = 0;
                // delay(10);
            }
        }
    }

    void Base::_resetButtonState()
    {
        for(const auto &pin: pinMonitor.getPins()) {
            if (std::find(kButtonPins.begin(), kButtonPins.end(), pin->getPin()) != kButtonPins.end()) {
                auto debounce = pin->getDebounce();
                if (debounce) {
                    noInterrupts();
                    *pin.get() = DebouncedHardwarePin(pin->getPin());
                    interrupts();
                }
            }
        }
    }

    void Base::_systemComboNextState(ComboButtonStateType state)
    {
        _systemButtonComboState = state;
        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
    }

    void Base::_updateSystemComboButton()
    {
        if (_isSystemComboActive()) {
            if (millis() >= _systemButtonComboTimeout) {
                // buttons still down, restart
                if (_systemButtonComboState == ComboButtonStateType::PRESSED) {
                    __DBG_printf_E("rebooting system");
                    BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                    delay(750);
                    config.restartDevice();
                }
                _systemButtonComboTimeout = 0;
                _systemButtonComboState = ComboButtonStateType::NONE;
                __DBG_printf("timeout, ending menu");
                BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                KFCFWConfiguration::setWiFiConnectLedMode();
                return;
            }
            switch(_systemButtonComboState) {
                case ComboButtonStateType::RELEASED:
                case ComboButtonStateType::RESET_MENU_TIMEOUT:
                    // update menu timeout
                    __DBG_printf("menu timeout=%.2fs", kSystemComboMenuTimeout / 1000.0);
                    BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::MEDIUM);
                    _systemButtonComboTimeout = millis() + kSystemComboMenuTimeout;
                    _systemButtonComboState = ComboButtonStateType::TIMEOUT;
                    break;
                case ComboButtonStateType::CONFIRM_EXIT:
                    __DBG_printf("confirming exit");
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    break;
                case ComboButtonStateType::CONFIRM_AUTO_SLEEP_OFF:
                    __DBG_printf("confirming auto sleep off exit");
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    _disableAutoSleepTimeout();
                    break;
                case ComboButtonStateType::CONFIRM_AUTO_SLEEP_ON:
                    __DBG_printf("confirming auto sleep on exit");
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    _setAutoSleepTimeout(true);
                    break;
                case ComboButtonStateType::CONFIRM_DEEP_SLEEP:
                    __DBG_printf("confirming deep sleep on exit");
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    _Scheduler.add(Event::milliseconds(750), false, [](Event::CallbackTimerPtr) {
                        RemoteControlPlugin::getInstance()._enterDeepSleep();
                    });
                    break;
                case ComboButtonStateType::PRESSED:
                case ComboButtonStateType::TIMEOUT:
                case ComboButtonStateType::EXIT_MENU_TIMEOUT:
                    // wait for action
                    break;
                case ComboButtonStateType::NONE:
                    break;
            }
        }
    }

    void Base::_updateSystemComboButtonLED()
    {
        switch(_systemButtonComboState) {
            case ComboButtonStateType::PRESSED:
            case ComboButtonStateType::CONFIRM_EXIT:
            case ComboButtonStateType::CONFIRM_AUTO_SLEEP_OFF:
            case ComboButtonStateType::CONFIRM_DEEP_SLEEP:
            case ComboButtonStateType::CONFIRM_AUTO_SLEEP_ON:
                if (!BUILDIN_LED_GET(BlinkLEDTimer::BlinkType::FLICKER)) {
                    BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                }
                break;
            case ComboButtonStateType::RELEASED:
                // BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                // break;
            case ComboButtonStateType::RESET_MENU_TIMEOUT:
            case ComboButtonStateType::TIMEOUT:
                if (!BUILDIN_LED_GET(BlinkLEDTimer::BlinkType::MEDIUM)) {
                    BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::MEDIUM);
                }
                break;
                // if (!BUILDIN_LED_GET(BlinkLEDTimer::BlinkType::FAST)) { // fast blinks twice until kSystemComboConfirmTimeout is reached
                //     BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FAST);
                // }
                // break;
                // if (!BUILDIN_LED_GET(BlinkLEDTimer::BlinkType::MEDIUM)) { // medium blinks once until kSystemComboConfirmTimeout is reached
                //     BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::MEDIUM);
                // }
                break;
            case ComboButtonStateType::EXIT_MENU_TIMEOUT:
            case ComboButtonStateType::NONE:
                break;
        }
    }

}
