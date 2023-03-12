/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <BitsToStr.h>
#include "remote.h"
#include "blink_led_timer.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

namespace RemoteControl {

    // button combo set it remoteBase.h
    // button combination used for maintenance mode (kButtonSystemComboPins)

    // hold first and last button to activate the system button combination
    //
    // holding both buttons for more than 5 seconds will reboot the device. if kept pressed during reboot, it will
    // go into the same mode again and repeat...
    //
    // once the buttons have been released, the menu is active, the LED stops flickering and starts blinking slowly.
    // the menu is active for 30 seconds after the last button press
    //
    // Button #1: disable auto sleep
    // Button #2: enable auto sleep
    // Button #3: enter deep sleep
    // Button #4: exit menu

    void Base::systemButtonComboEvent(bool state, EventType eventType, uint8_t button, uint16_t repeatCount, uint32_t eventTime)
    {
return;
        if (state) {
            Logger_notice(F("Entering system maintenance menu"));
            _systemButtonComboTime = millis();
            _systemButtonComboTimeout = _systemButtonComboTime + kSystemComboRebootTimeout;
            __LDBG_printf("PRESSED");
            _systemComboNextState(ComboButtonStateType::PRESSED);
        }
        else {
            bool acceptEvent = (_systemButtonComboState != ComboButtonStateType::PRESSED) && (millis() > _systemButtonComboTime);
            #if DEBUG_IOT_REMOTE_CONTROL
                if (acceptEvent) {
                    __LDBG_printf("event_type=%s (%ux) button#=%u pressed=%s time=%u",
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
                // case EventType::LONG_PRESSED:
                //     break;
                // case EventType::HOLD_REPEAT:
                //     break;
                // case EventType::HOLD_RELEASE:
                //     break;
                // case EventType::REPEATED_CLICK: {
                //         switch(repeatCount) {
                //             case 1: // EventType::SINGLE_CLICK
                //                 break;
                //             case 2: // EventType::DOUBLE_CLICK
                //                 break;
                //             default:
                //                 break;
                //         }
                //     }
                //     break;
                default:
                    break;
            }
            if ((_pressed & kButtonSystemComboBitMask) == 0 && _systemButtonComboState == ComboButtonStateType::PRESSED) { // wait until the last button has been released
                __LDBG_printf("released");
                _systemComboNextState(ComboButtonStateType::RELEASED);
                _systemButtonComboTime = millis() + 750; // ignore all events for one second
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
                    *pin.get() = DebouncedHardwarePin(pin->getPin(), digitalRead(pin->getPin()));
                    interrupts();
                }
            }
        }
    }

    void Base::_systemComboNextState(ComboButtonStateType state)
    {
        _systemButtonComboState = state;
        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
    }

    #define SYSTEM_MAINTENANCE_MENU "System maintenance: "

    void Base::_updateSystemComboButton()
    {
        if (_isSystemComboActive()) {
            if (millis() >= _systemButtonComboTimeout) {
                // buttons still down, restart
                if (_systemButtonComboState == ComboButtonStateType::PRESSED) {
                    Logger_notice(F(SYSTEM_MAINTENANCE_MENU "Rebooting device"));
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                    delay(750);
                    config.restartDevice();
                }
                _systemButtonComboTimeout = 0;
                _systemButtonComboState = ComboButtonStateType::NONE;
                Logger_notice(F(SYSTEM_MAINTENANCE_MENU "Menu timeout"));
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                KFCFWConfiguration::setWiFiConnectLedMode();
                return;
            }
            switch(_systemButtonComboState) {
                case ComboButtonStateType::RELEASED:
                case ComboButtonStateType::RESET_MENU_TIMEOUT:
                    // update menu timeout
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::MEDIUM);
                    _systemButtonComboTimeout = millis() + kSystemComboMenuTimeout;
                    _systemButtonComboState = ComboButtonStateType::TIMEOUT;
                    break;
                case ComboButtonStateType::CONFIRM_EXIT:
                    Logger_notice(F(SYSTEM_MAINTENANCE_MENU "Exiting menu"));
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    break;
                case ComboButtonStateType::CONFIRM_AUTO_SLEEP_OFF:
                    Logger_notice(F(SYSTEM_MAINTENANCE_MENU "Auto sleep disabled"));
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    _disableAutoSleepTimeout();
                    break;
                case ComboButtonStateType::CONFIRM_AUTO_SLEEP_ON:
                    Logger_notice(F(SYSTEM_MAINTENANCE_MENU "Auto sleep enabled"));
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    _setAutoSleepTimeout(true);
                    break;
                case ComboButtonStateType::CONFIRM_DEEP_SLEEP:
                    Logger_notice(F(SYSTEM_MAINTENANCE_MENU "Entering deep sleep"));
                    _updateSystemComboButtonLED();
                    _systemButtonComboTimeout = millis() + kSystemComboConfirmTimeout;
                    _systemButtonComboState = ComboButtonStateType::EXIT_MENU_TIMEOUT;
                    _Scheduler.add(Event::milliseconds(750), false, [](Event::CallbackTimerPtr) {
                        RemoteControlPlugin::getInstance().enterDeepSleep();
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
                if (!BUILTIN_LED_GET(BlinkLEDTimer::BlinkType::FLICKER)) {
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                }
                break;
            case ComboButtonStateType::RELEASED:
            case ComboButtonStateType::RESET_MENU_TIMEOUT:
            case ComboButtonStateType::TIMEOUT:
                if (!BUILTIN_LED_GET(BlinkLEDTimer::BlinkType::MEDIUM)) {
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::MEDIUM);
                }
                break;
            case ComboButtonStateType::EXIT_MENU_TIMEOUT:
            case ComboButtonStateType::NONE:
                break;
        }
    }

}
