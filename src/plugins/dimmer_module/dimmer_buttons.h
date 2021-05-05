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

        void begin();
        void end();

    protected:
        std::array<Channel, IOT_DIMMER_MODULE_CHANNELS> _channels;
    };

    inline void NoButtonsImpl::begin()
    {
    }

    inline void NoButtonsImpl::end()
    {
    }

    #if IOT_DIMMER_MODULE_HAS_BUTTONS

    // ------------------------------------------------------------------------
    // DimmerButtonsImpl
    // ------------------------------------------------------------------------

    class ButtonsImpl : public Base {
    public:
        using Base::Base;
        using ChannelsArray = std::array<Channel, IOT_DIMMER_MODULE_CHANNELS>;

        void begin();
        void end();

    protected:
        friend Button;

        ChannelsArray _channels;
    };

    inline void Buttons::end()
    {
        pinMonitor.detach(static_cast<const void *>(this));
    }

    #endif

}
