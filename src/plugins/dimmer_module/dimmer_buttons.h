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

    class ChannelsArray : public std::array<Channel, IOT_DIMMER_MODULE_CHANNELS>
    {
    public:
        void setAll(int16_t value) {
            for (auto &channel: *this) {
                channel.setLevel(value);
            }
            // auto ptr = data();
            // for (uint8_t i = 0; i < size(); i++) {
            //     ptr->_brightness = value;
            //     ptr++;
            // }
        }

        int32_t getSum() const {
            int32_t sum = 0;
            for (const auto &channel: *this) {
                sum += channel.getLevel();
            }
            return sum;
            // int32_t sum = 0;
            // auto ptr = data();
            // for (uint8_t i = 0; i < size(); i++) {
            //     sum += ptr->_brightness;
            //     ptr++;
            // }
            // return sum;
        }

        int32_t getSum(int16_t minLevel) const {
            int32_t sum = 0;
            for (const auto &channel: *this) {
                auto level = channel.getLevel();
                if (level > minLevel) {
                    sum += level;
                }
            }
            // int32_t sum = 0;
            // auto ptr = data();
            // for (uint8_t i = 0; i < size(); i++) {
            //     if (ptr->_brightness > minLevel) {
            //         sum += ptr->_brightness;
            //     }
            //     ptr++;
            // }
            return sum;
        }
    };

    // ------------------------------------------------------------------------
    // DimmerNoButtonsImpl
    // ------------------------------------------------------------------------

    class NoButtonsImpl : public Base {
    public:
        using Base::Base;

        void begin();
        void end();

    protected:
        ChannelsArray _channels;
    };

    inline void NoButtonsImpl::begin()
    {
        // auto size = _channels.size();
        // for(uint8_t i = 0; i < size; i++) {
        //     auto &channel = _channels.at(i);
        //     channel.setDimmer(reinterpret_cast<Dimmer::Module*>(0xffffffff));

        // }
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

            void begin();
            void end();

        protected:
            friend Button;

            ChannelsArray _channels;
        };

        inline void Buttons::end()
        {
            PinMonitor::pinMonitor.end();
        }

    #endif

}
