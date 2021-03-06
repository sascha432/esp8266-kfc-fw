/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock_button.h"
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Clock;

#if IOT_CLOCK_BUTTON_PIN != -1

void Button::event(EventType eventType, uint32_t now)
{
    __LDBG_printf("event_type=%s (%02x) repeat=%u button#=%u now=%u", eventTypeToString(eventType), eventType, _repeatCount, _button, now);
    getBase().buttonCallback(_button, eventType, _repeatCount);
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
        _rotaryActionTimer.remove();
    }
    switch(action) {
        case 0:
            // _digitalWrite(132, HIGH); // green LED left side
            // _digitalWrite(128, HIGH); // blue LED
            // _digitalWrite(129, HIGH); // red LED
            // _digitalWrite(130, HIGH); // green LED right side
            _PCF8574.PORT |= 0b10111;
            // _PCF8574.PORT = static_cast<uint8_t>(_PCF8574.PORT) & ~0b10111;
            break;
        case 1:
            // _digitalWrite(128, LOW);
            // _digitalWrite(129, LOW);
            _PCF8574.PORT &= ~0b11;
            // _PCF8574.PORT = static_cast<uint8_t>(_PCF8574.PORT) & ~0b11;
            break;
    }
}


#endif

void ClockPlugin::buttonCallback(uint8_t button, EventType eventType, uint16_t repeatCount)
{
    if (button == 0) {
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
                if (_config.enabled) {
                    _setState(false);
                }
                else {
                    _setState(true);
                }
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
#if IOT_CLOCK_TOUCH_PIN != -1
    else if (button == 1) {
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
#endif
}


#if IOT_CLOCK_BUTTON_PIN!=-1

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

