/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include "../include/templates.h"
#include <plugins.h>
#include "dimmer_plugin.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void DimmerButtons::_beginButtons()
{
    static_assert(IOT_DIMMER_MODULE_CHANNELS <= 8, "max. channels exceeded");

    for(uint8_t i = 0; i < _channels.size() * 2; i++) {
        auto pinNum = _config.pins[i];
        __LDBG_printf("channel=%u btn=%u pin=%u lp_time=%u rep_time=%u", i / 2, i % 2, pinNum, _config.longpress_time, _config.repeat_time);
        if (pinNum) {
            pinMonitor.attach<DimmerButton>(pinNum, i / 2, i % 2, *this);
            pinMode(pinNum, IOT_DIMMER_MODULE_PINMODE);
        }
    }
}

void DimmerButtons::_endButtons()
{
    pinMonitor.detach(static_cast<void *>(this));
}


// void DimmerButtonsImpl::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
// {
//     __LDBG_printf("onButtonHeld %p duration %u repeat %u", &btn, duration, repeatCount);

//     uint8_t pressed, channel;
//     bool buttonUp;
//     if (dimmer_plugin._findButton(btn, pressed, channel, buttonUp)) {
//         dimmer_plugin._buttonRepeat(channel, buttonUp, repeatCount);
//     }
// }

// void DimmerButtonsImpl::onButtonReleased(Button& btn, uint16_t duration)
// {
//     __LDBG_printf("onButtonReleased %p duration %u", &btn, duration);

//     uint8_t pressed, channel;
//     bool buttonUp;
//     auto &config = dimmer_plugin._getConfig();

//     if (dimmer_plugin._findButton(btn, pressed, channel, buttonUp)) {
//         if (duration < config.shortpress_time) {   // short press
//             dimmer_plugin._buttonShortPress(channel, buttonUp);
//         }
//         else if (duration < config.longpress_time) { // long press
//             dimmer_plugin._buttonLongPress(channel, buttonUp);
//         }
//         else { // held

//         }
//     }
//     if (!pressed) { // no button pressed anymore, remove main loop functions
//         dimmer_plugin._removeLoopFunction();
//     }
// }


// void DimmerButtonsImpl::_buttonShortPress(uint8_t channel, bool up)
// {
//     __LDBG_printf("channel=%d dir=%s repeat=%u", channel, up ? PSTR("up") : PSTR("down"), _turnOffTimerRepeat[channel]);
//     if (getChannelState(channel)) {
//         // single short press, start timer
//         // _turnOffTimerRepeat[channel] will be 0 for a single button down press

//         if (_turnOffTimer[channel]) {
//             _turnOffTimerRepeat[channel]++;
//             __LDBG_printf("rearm repeat=%u no_rep_time=%u", _turnOffTimerRepeat[channel], _config.shortpress_no_repeat_time);
//             _turnOffTimer[channel]->rearm(_config.shortpress_no_repeat_time, false);     // rearm timer after last keypress
//         }
//         else {
//             _turnOffLevel[channel] = getChannel(channel);
//             _turnOffTimerRepeat[channel] = 0;
//             __LDBG_printf("add repeat=%u no_rep_time=%u", _turnOffTimerRepeat[channel], _config.shortpress_no_repeat_time);
//             _Timer(_turnOffTimer[channel]).add(_config.shortpress_no_repeat_time, false, [this, channel](Event::CallbackTimerPtr timer) {
//                 // update buttons
//                 _loop();
//                 __LDBG_printf("off_ch=%u timer_exp=%u repeat=%d", channel, _config.shortpress_no_repeat_time, _turnOffTimerRepeat[channel]);
//                 if (_turnOffTimerRepeat[channel] == 0) { // single button down press detected, turn off
//                     if (off(channel)) {
//                         _channels[channel].setStoredBrightness(_turnOffLevel[channel]); // restore level to when the button was pressed
//                     }
//                 }
//             }, Event::PriorityType::TIMER);
//         }
//         if (up) {
//             _buttonRepeat(channel, true, 0);        // short press up = fire repeat event
//             _turnOffTimerRepeat[channel]++;
//         } else {
//             _buttonRepeat(channel, false, 0);       // short press down = fire repeat event
//         }
//     }
//     else {
//         if (up) {                                   // short press up while off = turn on
//             on(channel);
//         }
//         else {                                      // short press down while off = ignore
//         }
//     }
// }

// void DimmerButtonsImpl::_buttonLongPress(uint8_t channel, bool up)
// {
//     __LDBG_printf("channel=%d dir=%s", channel, up ? PSTR("up") : PSTR("down"));
//     //if (getChannelState(channel))
//     {
//         if (up) {
//             // long press up = increase to max brightness
//             setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.longpress_max_brightness / 100, _config.longpress_fadetime);
//         }
//         else {
//             // long press down = decrease to min brightness
//             setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.longpress_min_brightness / 100, _config.longpress_fadetime);
//         }
//     }
// }

// void DimmerButtonsImpl::_buttonRepeat(uint8_t channel, bool up, uint16_t repeatCount)
// {
//     auto level = getChannel(channel);
//     int16_t change = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.shortpress_step / 100;
//     if (!up) {
//         change = -change;
//     }
//     int16_t newLevel = max(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100, min(IOT_DIMMER_MODULE_MAX_BRIGHTNESS, level + change));
//     if (level != newLevel) {
//         __LDBG_printf("channel=%d dir=%s repeat=%d change=%d new_level=%d", channel, up ? PSTR("up") : PSTR("down"), repeatCount, change, newLevel);
//         setChannel(channel, newLevel, _config.shortpress_fadetime);
//     }
// }

#endif
