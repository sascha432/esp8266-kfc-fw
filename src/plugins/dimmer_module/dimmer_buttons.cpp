/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include "../include/templates.h"
#include <EventScheduler.h>
#include "plugins.h"
#include "dimmer_module.h"
#include "WebUISocket.h"

#include "firmware_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void Driver_DimmerModule::_beginButtons()
{
    PinMonitor &monitor = PinMonitor::createInstance();

    for(uint8_t i = 0; i < _channels.size() * 2; i++) {
        auto pinNum = _config.pins[i];
        __LDBG_printf("ch=%u btn=%u pin=%u lp_time=%u rep_time=%u", i / 2, i % 2, pinNum, _config.longpress_time, _config.repeat_time);
        if (pinNum) {
            _buttons.emplace_back(pinNum);
            auto &button = _buttons.back().getButton();

            auto iterator = monitor.addPin(pinNum, [this, &button](PinMonitor::Pin &pin) {
                _pinCallback(pin, button);
            }, this, IOT_DIMMER_MODULE_PINMODE);

            if (iterator != monitor.end()) {
#if DEBUG_IOT_DIMMER_MODULE
                button.onPress(Driver_DimmerModule::onButtonPressed);
#endif
                button.onHoldRepeat(_config.longpress_time, _config.repeat_time, Driver_DimmerModule::onButtonHeld);
                button.onRelease(Driver_DimmerModule::onButtonReleased);
            }
            else {
                __LDBG_printf("removeing pin=%u after error", pinNum);
                _buttons.erase(_buttons.end() - 1);
                monitor.removePin(pinNum, this);
            }
        }
    }
#endif
}

void Driver_DimmerModule::_endButtons()
{
    __LDBG_print("removing timers");
    for(uint8_t i = 0; i < _channels.size(); i++) {
        _turnOffTimer[i].remove();
    }

    __LDBG_print("removing pin monitor");
    if (PinMonitor::hasInstance()) {
        PinMonitor &monitor = PinMonitor::getInstance();
        for(auto &button: _buttons) {
            monitor.removePin(button.getPin(), this);
        }
        if (monitor.empty()) {
            PinMonitor::deleteInstance();
        }
    }
    _removeLoopFunction();
}

void Driver_DimmerModule::_addLoopFunction()
{
    if (!_loopAdded) {
        __LDBG_print("add loop");
        LoopFunctions::add(Driver_DimmerModule::loop);
        _loopAdded = true;
    }
}

void Driver_DimmerModule::_removeLoopFunction()
{
    if (_loopAdded) {
        __LDBG_print("remove loop");
        LoopFunctions::remove(Driver_DimmerModule::loop);
        _loopAdded = false;
    }
}

void Driver_DimmerModule::loop()
{
    dimmer_plugin._loop();
}

void Driver_DimmerModule::onButtonPressed(Button& btn)
{
    __LDBG_printf("onButtonPressed %p", &btn);
}

void Driver_DimmerModule::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    __LDBG_printf("onButtonHeld %p duration %u repeat %u", &btn, duration, repeatCount);

    uint8_t pressed, channel;
    bool buttonUp;
    if (dimmer_plugin._findButton(btn, pressed, channel, buttonUp)) {
        dimmer_plugin._buttonRepeat(channel, buttonUp, repeatCount);
    }
}

void Driver_DimmerModule::onButtonReleased(Button& btn, uint16_t duration)
{
    __LDBG_printf("onButtonReleased %p duration %u", &btn, duration);

    uint8_t pressed, channel;
    bool buttonUp;
    auto &config = dimmer_plugin._getConfig();

    if (dimmer_plugin._findButton(btn, pressed, channel, buttonUp)) {
        if (duration < config.shortpress_time) {   // short press
            dimmer_plugin._buttonShortPress(channel, buttonUp);
        }
        else if (duration < config.longpress_time) { // long press
            dimmer_plugin._buttonLongPress(channel, buttonUp);
        }
        else { // held

        }
    }
    if (!pressed) { // no button pressed anymore, remove main loop functions
        dimmer_plugin._removeLoopFunction();
    }
}

bool Driver_DimmerModule::_findButton(Button &btn, uint8_t &pressed, uint8_t &channel, bool &buttonUp)
{
    uint8_t num = 0;

    channel = 0xff;
    pressed = 0;
    for(auto &button: dimmer_plugin._buttons) {
        if (btn.is(button.getButton())) {
            channel = num / 2;
            buttonUp = !(num % 2);
        }
        if (button.getButton().isPressed()) {
            pressed++;
        }
        num++;
    }
    __LDBG_printf("pressed=%u channel=%u button=%s", pressed, channel, channel == 0xff ? PSTR("N/A") : (buttonUp ? PSTR("up") : PSTR("down")));

    return channel != 0xff;
}


void Driver_DimmerModule::_pinCallback(PinMonitor::Pin &pin, PushButton &button)
{
    // _addLoopFunction();

    __DBG_printf("callback=%p button=%p pin=%u micros=%u", this, &button, pin.getPin(), pin.getValue(), pin.getMicros());

    // pin.getValue();
    // pin.getMicros()

    // update button
    // button.update();
}

void Driver_DimmerModule::_loop()
{
    // update all buttons
    for(auto &button: _buttons) {
        button.getButton().update();
    }
}

void Driver_DimmerModule::_buttonShortPress(uint8_t channel, bool up)
{
    __LDBG_printf("channel=%d dir=%s repeat=%u", channel, up ? PSTR("up") : PSTR("down"), _turnOffTimerRepeat[channel]);
    if (getChannelState(channel)) {
        // single short press, start timer
        // _turnOffTimerRepeat[channel] will be 0 for a single button down press

        if (_turnOffTimer[channel]) {
            _turnOffTimerRepeat[channel]++;
            __LDBG_printf("rearm repeat=%u no_rep_time=%u", _turnOffTimerRepeat[channel], _config.shortpress_no_repeat_time);
            _turnOffTimer[channel]->rearm(_config.shortpress_no_repeat_time, false);     // rearm timer after last keypress
        }
        else {
            _turnOffLevel[channel] = getChannel(channel);
            _turnOffTimerRepeat[channel] = 0;
            __LDBG_printf("add repeat=%u no_rep_time=%u", _turnOffTimerRepeat[channel], _config.shortpress_no_repeat_time);
            _Timer(_turnOffTimer[channel]).add(_config.shortpress_no_repeat_time, false, [this, channel](Event::CallbackTimerPtr timer) {
                // update buttons
                _loop();
                __LDBG_printf("off_ch=%u timer_exp=%u repeat=%d", channel, _config.shortpress_no_repeat_time, _turnOffTimerRepeat[channel]);
                if (_turnOffTimerRepeat[channel] == 0) { // single button down press detected, turn off
                    if (off(channel)) {
                        _channels[channel].setStoredBrightness(_turnOffLevel[channel]); // restore level to when the button was pressed
                    }
                }
            }, Event::PriorityType::TIMER);
        }
        if (up) {
            _buttonRepeat(channel, true, 0);        // short press up = fire repeat event
            _turnOffTimerRepeat[channel]++;
        } else {
            _buttonRepeat(channel, false, 0);       // short press down = fire repeat event
        }
    }
    else {
        if (up) {                                   // short press up while off = turn on
            on(channel);
        }
        else {                                      // short press down while off = ignore
        }
    }
}

void Driver_DimmerModule::_buttonLongPress(uint8_t channel, bool up)
{
    __LDBG_printf("channel=%d dir=%s", channel, up ? PSTR("up") : PSTR("down"));
    //if (getChannelState(channel))
    {
        if (up) {
            // long press up = increase to max brightness
            setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.longpress_max_brightness / 100, _config.longpress_fadetime);
        }
        else {
            // long press down = decrease to min brightness
            setChannel(channel, IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.longpress_min_brightness / 100, _config.longpress_fadetime);
        }
    }
}

void Driver_DimmerModule::_buttonRepeat(uint8_t channel, bool up, uint16_t repeatCount)
{
    auto level = getChannel(channel);
    int16_t change = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.shortpress_step / 100;
    if (!up) {
        change = -change;
    }
    int16_t newLevel = max(IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100, min(IOT_DIMMER_MODULE_MAX_BRIGHTNESS, level + change));
    if (level != newLevel) {
        __LDBG_printf("channel=%d dir=%s repeat=%d change=%d new_level=%d", channel, up ? PSTR("up") : PSTR("down"), repeatCount, change, newLevel);
        setChannel(channel, newLevel, _config.shortpress_fadetime);
    }
}

#endif
