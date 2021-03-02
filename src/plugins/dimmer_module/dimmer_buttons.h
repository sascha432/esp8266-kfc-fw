/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "dimmer_base.h"
#include "dimmer_channel.h"

//
// DimmerButtons -> DimmerButtonsImpl/DimmerNoButtonsImpl -> Dimmer_Base
//


// ------------------------------------------------------------------------
// DimmerNoButtonsImpl
// ------------------------------------------------------------------------

class DimmerNoButtonsImpl : public Dimmer_Base {
public:
    using Dimmer_Base::Dimmer_Base;

    void _beginButtons() {}
    void _endButtons() {}

protected:
    std::array<DimmerChannel, IOT_DIMMER_MODULE_CHANNELS> _channels;
};

#if IOT_DIMMER_MODULE_HAS_BUTTONS

// ------------------------------------------------------------------------
// DimmerButtonsImpl
// ------------------------------------------------------------------------

class DimmerButtonsImpl : public Dimmer_Base {
public:
    using Dimmer_Base::Dimmer_Base;
    using DimmerChannelsArray = std::array<DimmerChannel, IOT_DIMMER_MODULE_CHANNELS>;

    void _beginButtons();
    void _endButtons();

protected:
    friend DimmerButton;

    DimmerChannelsArray _channels;
};

#endif
