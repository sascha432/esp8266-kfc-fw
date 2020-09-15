/**
 * Author: sascha_lammers@gmx.de
 */

#include <LoopFunctions.h>
#include <KFCForms.h>
#include "buttons.h"

static ButtonsPlugin plugin;

void ButtonsPlugin::setup(SetupModeType mode)
{
    _readConfig();
    // for(auto &button: _buttons) {

    //     auto iterator = monitor.addPin(button.getPin(), pinCallback, this, button.getPinMode());
    //     if (iterator != monitor.end()) {
    //         __LDBG_printf("pin=%u added", button.getPin());
    //         PushButton &pushButton = button.getButton();
    //         pushButton.onPress(onButtonPressed);
    //         pushButton.onHoldRepeat(button.getConfig().longpress.time, button.getConfig().repeat.time, onButtonHeld);
    //         pushButton.onRelease(onButtonReleased);

    //     }
    //     else {
    //         __LDBG_printf("failed to add pin=%u", button.getPin());
    //         monitor.removePin(button.getPin(), this);
    //     }
    // }
}

void ButtonsPlugin::shutdown()
{
    // if (PinMonitor::hasInstance()) {
    //     PinMonitor &monitor = PinMonitor::getInstance();
    //     for(auto &button: _buttons) {
    //         monitor.removePin(button.getPin(), this);
    //     }
    //     if (monitor.empty()) {
    //         PinMonitor::deleteInstance();
    //     }
    // }
    // LoopFunctions::remove(loop);
}

void ButtonsPlugin::reconfigure(PGM_P source)
{
    // if (PinMonitor::hasInstance()) {
    //     PinMonitor &monitor = PinMonitor::getInstance();
    //     for(auto &button: _buttons) {
    //         monitor.removePin(button.getPin(), this);
    //     }
    // }
    // setup(SetupModeType::DEFAULT);
    // if (PinMonitor::hasInstance() && PinMonitor::getInstance().empty()) {
    //     PinMonitor::deleteInstance();
    // }
}

void ButtonsPlugin::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    form.finalize();
}

void ButtonsPlugin::_readConfig()
{
    _buttons.clear();
    Config_Button::ButtonVector buttons;
    Config_Button::getButtons(buttons);
    for(auto &button: buttons) {
        __LDBG_printf("pin=%u", button.pin);
        _buttons.emplace_back(button);
    }
    __LDBG_printf("size=%u", _buttons.size());
}

void ButtonsPlugin::pinCallback(Pin &arg)
{

}

void ButtonsPlugin::loop() {

}

// void ButtonsPlugin::onButtonPressed(::Button& btn)
// {

// }

// void ButtonsPlugin::onButtonHeld(::Button& btn, uint16_t duration, uint16_t repeatCount)
// {

// }

// void ButtonsPlugin::onButtonReleased(::Button& btn, uint16_t duration)
// {

// }
