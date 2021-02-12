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

void Button::event(EventType eventType, uint32_t now)
{
    __LDBG_printf("event_type=%s (%02x) repeat=%u button#=%u now=%u", eventTypeToString(eventType), eventType, _repeatCount, _button, now);
    getBase().buttonCallback(_button, eventType, _repeatCount);
}

void ClockPlugin::buttonCallback(uint8_t button, EventType eventType, uint16_t repeatCount)
{
    switch (eventType) {
        case EventType::PRESSED:
            if (!_config.enabled) {
                _setState(true);
            }
            break;
        case EventType::LONG_PRESSED:
            if (_config.enabled) {
                _setState(false);
            }
            else {
                _setState(true);
            }
            break;
        case EventType::HELD:
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
                pinMonitor.detach(this);

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