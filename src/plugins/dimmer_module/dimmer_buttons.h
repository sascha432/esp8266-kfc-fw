/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "dimmer_base.h"
#include "dimmer_channel.h"

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
    DimmerButtonsImpl();

    void _beginButtons();
    void _endButtons();

protected:
    friend DimmerButton;
};

#endif
