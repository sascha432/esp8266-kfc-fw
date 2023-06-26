/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock_button.h"
#include "clock.h"
#if HAVE_IOEXPANDER
#include <IOExpander.h>
#endif

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Clock;

#if PIN_MONITOR

void Button::event(EventType eventType, uint32_t now)
{
    __LDBG_printf("event_type=%s (%02x) repeat=%u button#=%u now=%u", eventTypeToString(eventType), eventType, _repeatCount, _button, now);
    getBase().buttonCallback(ButtonType(_button), eventType, _repeatCount);
}

#if IOT_CLOCK_HAVE_ROTARY_ENCODER

void Clock::RotaryEncoder::event(EventType eventType, uint32_t now)
{
    __LDBG_printf("event_type=%u now=%u decrease=%u", eventType, now, eventType == EventType::COUNTER_CLOCK_WISE);
    ClockPlugin::getInstance().rotaryCallback(eventType == EventType::COUNTER_CLOCK_WISE, now);
}

void ClockPlugin::rotaryCallback(bool decrease, uint32_t now)
{
    auto diff = get_time_diff(_lastRotaryUpdate, now) / 1000;
    if (_rotaryAction == 0) {
        _lastRotaryUpdate = now;
        if (diff > 500) {
            _rotaryAcceleration = kRotaryAccelerationDivider;
        }
        else {
            _rotaryAcceleration = std::min<uint8_t>(kRotaryMaxAcceleration, _rotaryAcceleration + 1);
        }

        // __LDBG_printf("diff=%u decrease=%u acceleration=%u", diff, decrease, _rotaryAcceleration);

        if (_isEnabled) {
            auto brightness = _targetBrightness;
            if (decrease) {
                _setBrightness(std::max<int16_t>(1, brightness - (_rotaryAcceleration / kRotaryAccelerationDivider)));
            }
            else {
                _setBrightness(std::min<int16_t>(255, brightness + (_rotaryAcceleration / kRotaryAccelerationDivider)));
            }
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
    }
    else if (_rotaryAction == 1) {
        if (diff > 1000) { // limit input frequency to once per second
            uint8_t animation = (_config.animation + (decrease ? 1 : 4)) % 5; // limit values to 0-5
            setAnimation(static_cast<AnimationType>(animation), 750);
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
        setRotaryAction(_rotaryAction); // reset timer
    }
}

void ClockPlugin::setRotaryAction(uint8_t action)
{
    __LDBG_printf("set rotary action=%u", action);
    _rotaryAction = action;
    if (action != 0) {
        _Timer(_rotaryActionTimer).add(Event::milliseconds(5000), false, [this](Event::CallbackTimerPtr) {
            setRotaryAction(0);
        });
    }
    else {
        _Timer(_rotaryActionTimer).remove();
    }
    auto &_PCF8574 = IOExpander::config._device;
    switch(action) {
        case 0:
            // digitalWrite(132, HIGH); // green LED left side
            // digitalWrite(128, HIGH); // blue LED
            // digitalWrite(129, HIGH); // red LED
            // digitalWrite(130, HIGH); // green LED right side
            _PCF8574.PORT |= 0b10111;
            break;
        case 1:
            // digitalWrite(128, LOW);
            // digitalWrite(129, LOW);
            _PCF8574.PORT &= ~0b11;
            break;
    }
}


#endif

void ClockPlugin::buttonCallback(ButtonType button, EventType eventType, uint16_t repeatCount)
{
    LoopFunctions::callOnce([this, button, eventType, repeatCount]() {
        _buttonCallback(button, eventType, repeatCount);
    });
}

static constexpr int _getBrightnessChange(float pct)
{
    return ((pct * 255) / 100) + 1;
}

static constexpr int kBrightnessChangeClick = _getBrightnessChange(2);
static constexpr int kBrightnessChangeLongPress = _getBrightnessChange(10);
static constexpr int kBrightnessChangeHold = _getBrightnessChange(1);

void ClockPlugin::_buttonCallback(ButtonType button, EventType eventType, uint16_t repeatCount)
{
    __LDBG_printf("button=%u event_type=%u repeat=%u", button, eventType, repeatCount);
    switch(button) {
        #if IOT_CLOCK_BUTTON_PIN != -1
            case ButtonType::MAIN: {
                switch (eventType) {
                    case EventType::PRESSED:
                        if (!_config.enabled) {
                            _setState(true);
                        }
                        IF_IOT_CLOCK_HAVE_ROTARY_ENCODER(
                            else {
                                setRotaryAction((_rotaryAction + 1) % 2); // toggle rotary encoder action
                            }
                        )
                        break;
                    case EventType::LONG_PRESSED:
                        _setState(!_config.enabled);
                        break;
                    case EventType::HOLD:
                        // start flashing red after 5 seconds and reboot 2 seconds later
                        // if the button is pressed for 8.2 seconds a hard reset is performed
                        if (repeatCount * 250 + 1500 > 5000) {
                            // disable animation blending and start flashing red
                            delete _animation;
                            _animation = nullptr;
                            _setAnimation(new Clock::FlashingAnimation(*this, Color(255, 0, 0), 250));
                            _targetBrightness = 255 / 4; // 25%
                            _fadeTimer.disable();
                            _forceUpdate = true;
                            _isFading = false;
                            _Timer(_timer).remove();

                            // disable buttons
                            pinMonitor.end();
                            //pinMonitor.detach(this);

                            _Scheduler.add(2000, false, [this](Event::CallbackTimerPtr timer) {
                                __LDBG_printf("restarting device");
                                config.restartDevice();
                            });
                        }
                        break;
                    //  case EventType::SINGLE_CLICK:
                    //     if (!_config.enabled) {
                    //         _setState(false);
                    //     }
                    //     else {
                    //         _setState(true);
                    //     }
                    //     // base.queueEvent(eventType, _button, _getEventTime(), config.actions[_button].single_click);
                    //     break;
                    // case EventType::DOUBLE_CLICK:
                    //     // queueEvent(eventType, _button, _getEventTime(), config.actions[_button].double_click);
                    //     break;
                    default:
                        break;
                }
            }
            break;
        #endif

        #if IOT_CLOCK_TOUCH_PIN != -1
            case ButtonType::TOUCH: {
                switch (eventType) {
                    case EventType::PRESSED:
                    case EventType::SINGLE_CLICK:
                        if (!_config.enabled) {
                            _setState(true);
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        #endif

        #if IOT_LED_MATRIX_TOGGLE_PIN != -1
            case ButtonType::TOGGLE_ON_OFF: {
                switch (eventType) {
                    #if IOT_LED_MATRIX_TOGGLE_PIN_LONG_PRESS_TYPE == 0
                        case EventType::LONG_PRESSED:
                    #endif
                    case EventType::PRESSED:
                    case EventType::SINGLE_CLICK: {
                        __LDBG_printf("toggle on off click turn %s", !_config.enabled ? PSTR("on") : PSTR("off"));
                        _setState(!_config.enabled);
                    }
                    break;
                    #if IOT_LED_MATRIX_TOGGLE_PIN_LONG_PRESS_TYPE == 1
                        case EventType::HOLD:
                            if (repeatCount != 1) { // if holding the button too long, LONG_PRESSED is omitted. simulate LONG_PRESSED for the first repeat
                                break;
                            }
                            // fallthrough
                        case EventType::LONG_PRESSED: {
                            if (!_config.enabled) {
                                __LDBG_printf("toggle long press turn on (was off)");
                                _setState(true);
                            }
                            else {
                                __LDBG_printf("toggle long press next animation");
                                nextAnimation();
                            }
                        }
                        break;
                    #endif
                    default:
                        break;
                }
            }
            break;
        #endif

        #if IOT_LED_MATRIX_NEXT_ANIMATION_PIN != -1
            case ButtonType::NEXT_ANIMATION: {
                switch (eventType) {
                    case EventType::LONG_PRESSED:
                        __LDBG_printf("first animation");
                        setAnimation(AnimationType::SOLID);
                        break;
                    case EventType::PRESSED:
                    case EventType::SINGLE_CLICK:
                        __LDBG_printf("next animation");
                        nextAnimation();
                        break;
                    default:
                        break;
                }
            }
            break;
        #endif

        #if IOT_LED_MATRIX_INCREASE_BRIGHTNESS_PIN != -1
            case ButtonType::INCREASE_BRIGHTNESS: {
                switch (eventType) {
                    case EventType::PRESSED:
                    case EventType::SINGLE_CLICK:
                        setBrightness(std::min(255, _targetBrightness + kBrightnessChangeClick));
                        break;
                    case EventType::LONG_PRESSED:
                        setBrightness(std::min(255, _targetBrightness + kBrightnessChangeLongPress));
                        break;
                    case EventType::HOLD:
                        setBrightness(std::min(255, _targetBrightness + kBrightnessChangeHold));
                        break;
                    default:
                        break;
                }
            }
            break;
        #endif

        #if IOT_LED_MATRIX_DECREASE_BRIGHTNESS_PIN != -1
            case ButtonType::DECREASE_BRIGHTNESS: {
                switch (eventType) {
                    case EventType::PRESSED:
                    case EventType::SINGLE_CLICK:
                        setBrightness(std::max(1, _targetBrightness - kBrightnessChangeClick));
                        break;
                    case EventType::LONG_PRESSED:
                        setBrightness(std::max(1, _targetBrightness - kBrightnessChangeLongPress));
                        break;
                    case EventType::HOLD:
                        setBrightness(std::max(1, _targetBrightness - kBrightnessChangeHold));
                        break;
                    default:
                        break;
                }
            }
            break;
        #endif

        default:
            break;
    }
}

#if IOT_CLOCK_BUTTON_PIN != -1

// void ClockPlugin::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
// {
//     __LDBG_printf("duration=%u repeat=%u", duration, repeatCount);
//     if (repeatCount == 1) {
//         plugin.readConfig();
//         plugin._buttonCounter = 0;
//     }
//     if (repeatCount == 12) {    // start flashing after 2 seconds, hard reset occurs ~2.5s
//         plugin._autoBrightness = kAutoBrightnessOff;
//         plugin._display.setBrightness(SevenSegmentDisplay::kMaxBrightness);
//         plugin._setAnimation(new Clock::FlashingAnimation, *this, Color(255, 0, 0), 150));

//         _Scheduler.add(2000, false, [](Event::CallbackTimerPtr timer) {   // call restart if no reset occured
//             __LDBG_printf("restarting device\n"));
//             config.restartDevice();
//         });
//     }
// }

// void ClockPlugin::onButtonReleased(Button& btn, uint16_t duration)
// {
//     __LDBG_printf("duration=%u", duration);
//     if (duration < 800) {
//         plugin._onButtonReleased(duration);
//     }
// }

// void ClockPlugin::_onButtonReleased(uint16_t duration)
// {
//     __LDBG_printf("press=%u", _buttonCounter % 4);
//     if (_resetAlarm()) {
//         return;
//     }
//     switch(_buttonCounter % 4) {
//         case 0:
//             setAnimation(AnimationType::RAINBOW);
//             break;
//         case 1:
//             setAnimation(AnimationType::FLASHING);
//             break;
//         case 2:
//             setAnimation(AnimationType::FADING);
//             break;
//         case 3:
//         default:
//             setAnimation(AnimationType::NONE);
//             break;
//     }
//     _buttonCounter++;
// }

#endif

#endif

