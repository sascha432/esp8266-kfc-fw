/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "dimmer_channel.h"
#include "dimmer_button.h"

//
// DimmerButtons -> DimmerButtonsImpl/DimmerNoButtonsImpl -> DimmerChannels -> Dimmer_Base
//

// ------------------------------------------------------------------------
// DimmerChannels
// ------------------------------------------------------------------------

class DimmerChannels : public Dimmer_Base {
public:
    using Dimmer_Base::Dimmer_Base;

protected:
    std::array<DimmerChannel, IOT_DIMMER_MODULE_CHANNELS> _channels;
};

// ------------------------------------------------------------------------
// DimmerNoButtonsImpl
// ------------------------------------------------------------------------

class DimmerNoButtonsImpl : public DimmerChannels
{
public:
    using DimmerChannels::DimmerChannels;

    void _beginButtons() {}
    void _endButtons() {}
};

#if IOT_DIMMER_MODULE_HAS_BUTTONS

// ------------------------------------------------------------------------
// DimmerButtonsImpl
// ------------------------------------------------------------------------

class DimmerButtonsImpl : public DimmerChannels {
public:
    using DimmerChannels::DimmerChannels;

public:
    DimmerButtonsImpl() {
        pinMonitor.begin();
    }

    void _beginButtons();
    void _endButtons();

    // static void onButtonPressed(Button& btn);
    // static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    // static void onButtonReleased(Button& btn, uint16_t duration);

protected:
    friend DimmerButton;

    // void _buttonShortPress(uint8_t channel, bool up);
    // void _buttonLongPress(uint8_t channel, bool up);
    // void _buttonRepeat(uint8_t channel, bool up, uint16_t repeatCount);

    // // get number of pressed buttons, channel and up or down button. returns false if no match
    // bool _findButton(Button &btn, uint8_t &pressed, uint8_t &channel, bool &buttonUp);

// protected:
//     std::array<Event::Timer, IOT_DIMMER_MODULE_CHANNELS> _turnOffTimer;
//     std::array<uint8_t, IOT_DIMMER_MODULE_CHANNELS> _turnOffTimerRepeat;
//     std::array<int16_t, IOT_DIMMER_MODULE_CHANNELS> _turnOffLevel;
};

#endif
