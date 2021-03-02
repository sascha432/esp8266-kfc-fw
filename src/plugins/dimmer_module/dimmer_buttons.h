/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "dimmer_base.h"
#include "dimmer_channel.h"

//
// Dimmer::Buttons -> Dimmer::ButtonsImpl/Dimmer::NoButtonsImpl -> Dimmer::Base
//

namespace Dimmer {

    // ------------------------------------------------------------------------
    // DimmerNoButtonsImpl
    // ------------------------------------------------------------------------

    class NoButtonsImpl : public Base {
    public:
        using Base::Base;

        void _begin() {}
        void _end() {}

    protected:
        std::array<Channel, IOT_DIMMER_MODULE_CHANNELS> _channels;
    };

    #if IOT_DIMMER_MODULE_HAS_BUTTONS

    // ------------------------------------------------------------------------
    // DimmerButtonsImpl
    // ------------------------------------------------------------------------

    class ButtonsImpl : public Base {
    public:
        using Base::Base;
        using ChannelsArray = std::array<Channel, IOT_DIMMER_MODULE_CHANNELS>;

        // ButtonsImpl();

        void _begin();
        void _end();

    protected:
        friend Button;

        ChannelsArray _channels;
    };

    #endif

}
