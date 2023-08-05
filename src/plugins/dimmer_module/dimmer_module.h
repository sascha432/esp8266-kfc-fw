/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Trailing edge dimmer module
// I2C or UART interface
// 1 - 8 channels
// https://github.com/sascha432/trailing_edge_dimmer

#include "dimmer_buttons.h"
#include <Arduino_compat.h>
#if IOT_DIMMER_HAS_COLOR_TEMP
#    include "dimmer_colortemp.h"
#endif

namespace Dimmer {

    class ColorTemperature;
    class ChannelsArray;

    class Module: public MQTTComponent, public Buttons {
    protected:
        friend ColorTemperature;

        using Buttons::_channels;

    public:
        Module();

        #if IOT_DIMMER_MODULE_INTERFACE_UART == 0
            virtual void onConnect() override;
        #endif

        virtual bool on(uint8_t channel = -1, float transition = NAN) override;
        virtual bool off(uint8_t channel = -1, float transition = NAN) override;

        virtual ChannelsArray &getChannels() override;
        virtual bool isAnyOn() const;
        uint8_t isAnyOnInt() const;
        virtual void publishChannelState(uint8_t channel) override;
        virtual bool getChannelState(uint8_t channel) const override;

        virtual int16_t getChannel(uint8_t channel) const override;
        virtual bool setChannel(uint8_t channel, int16_t level, float transition = NAN) override;
        virtual void stopFading(uint8_t channel) override;

        virtual void publishChannel(uint8_t channel) override;
        virtual void publishChannels() override;

        virtual int16_t getRange(uint8_t channel) const override;
        virtual int16_t getOffset(uint8_t channel) const override;

        virtual void _onReceive(size_t length) override;

    protected:
        void setup();
        void shutdown();
        void getStatus(Print &out);

        void _beginMqtt();
        void _endMqtt();

    private:
        void _getChannels();
    };

    inline Module::Module() :
        MQTTComponent(ComponentType::SENSOR)
    {
    }

    #if IOT_DIMMER_MODULE_INTERFACE_UART == 0

        inline void Module::onConnect()
        {
            _fetchMetrics();
        }

    #endif

    inline uint8_t Module::isAnyOnInt() const
    {
        __LDBG_printf("anyon=%u", isAnyOn() ? 1 : 0);
        return isAnyOn() ? 1 : 0;
    }

    inline bool Module::on(uint8_t channel, float transition)
    {
        __LDBG_printf("on ch=%u t=%f p=%u", channel, transition);
        return _channels[channel].on(transition);
    }

    inline bool Module::off(uint8_t channel, float transition)
    {
        __LDBG_printf("off ch=%u t=%f p=%u", channel, transition);
        return _channels[channel].off(&_config, transition);
    }

    inline int16_t Module::getChannel(uint8_t channel) const
    {
        __LDBG_printf("get ch=%u lvl=%u", channel, _channels[channel].getLevel());
        return _channels[channel].getLevel();
    }

    inline bool Module::getChannelState(uint8_t channel) const
    {
        __LDBG_printf("get ch=%u state=%u", channel, _channels[channel].getOnState());
        return _channels[channel].getOnState();
    }

    inline bool Module::setChannel(uint8_t channel, int16_t level, float transition)
    {
        __LDBG_printf("set ch=%u lvl=%u t=%f p=%u", channel, level, transition);
        #if IOT_ATOMIC_SUN_V2
            return _color.setChannel(channel, level, transition);
        #else
            _channels[channel].setLevel(level, transition);
            return true;
        #endif
    }

    inline void Module::stopFading(uint8_t channel)
    {
        __LDBG_printf("stop ch=%u", channel);
        _channels[channel].stopFading();
    }

    inline ChannelsArray &Module::getChannels()
    {
        return _channels;
    }

    inline int16_t Module::getRange(uint8_t channel) const
    {
        return 0;
    }

    inline int16_t Module::getOffset(uint8_t channel) const
    {
        return 0;
    }

    inline void Module::publishChannel(uint8_t channel)
    {
        _channels[channel].publishState();
    }

    inline void Module::publishChannels()
    {
        for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            _channels[i].publishState();
        }
    }

}
