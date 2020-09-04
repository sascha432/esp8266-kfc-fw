/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <array>
#include <kfc_fw_config.h>
// #include "./plugins/mqtt/mqtt_client.h"
#include "serial_handler.h"
#include "dimmer_channel.h"
#include "pin_monitor.h"

#if IOT_DIMMER_MODULE_HAS_BUTTONS
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#endif

class DimmerChannels {
protected:
    std::array<DimmerChannel, IOT_DIMMER_MODULE_CHANNELS> _channels;
};

#if IOT_DIMMER_MODULE_HAS_BUTTONS

// buttons
class DimmerButtons : public DimmerChannels {
public:
    using DimmerChannels::DimmerChannels;
    using DimmerChannels::_channels;

    DimmerButtons() : _loopAdded(false) {}
    ~DimmerButtons() {}

    static void pinCallback(void *arg);
    static void loop();

    static void onButtonPressed(Button& btn);
    static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button& btn, uint16_t duration);

private:
    void _pinCallback(PinMonitor::Pin &pin, PushButton &button);
    void _loop();

    void _buttonShortPress(uint8_t channel, bool up);
    void _buttonLongPress(uint8_t channel, bool up);
    void _buttonRepeat(uint8_t channel, bool up, uint16_t repeatCount);

    // get number of pressed buttons, channel and up or down button. returns false if no match
    bool _findButton(Button &btn, uint8_t &pressed, uint8_t &channel, bool &buttonUp);

private:
    class DimmerButton {
    public:
        DimmerButton(uint8_t pin) : _pin(pin), _button(pin, IOT_SWITCH_ACTIVE_STATE) {
            _button.onPress(nullptr);
        }
        inline uint8_t getPin() const {
            return _pin;
        }
        inline PushButton &getButton() {
            return _button;
        }
    private:
        uint8_t _pin;
        PushButton _button;
    };

    typedef std::vector<DimmerButton> DimmerButtonVector;

    DimmerButtonVector _buttons;
    bool _loopAdded;
    std::array<Event::Timer, IOT_DIMMER_MODULE_CHANNELS> _turnOffTimer;
    std::array<uint8_t, IOT_DIMMER_MODULE_CHANNELS> _turnOffTimerRepeat;
    std::array<int16_t, IOT_DIMMER_MODULE_CHANNELS> _turnOffLevel;
};

#else

using DimmerButtons = DimmerChannels;

#endif
