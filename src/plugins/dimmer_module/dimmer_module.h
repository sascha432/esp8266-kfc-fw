/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Trailing edge dimmer module
// I2C or UART interface
// 1 - 8 channels
// https://github.com/sascha432/trailing_edge_dimmer
//
// default I2C pins are D3 (0) and D5 (14)
// NOTE: I2C Wire.onReceive() is not working on ESP8266

#include <Arduino_compat.h>
#include "dimmer_form.h"
#include "dimmer_buttons.h"
#if IOT_DIMMER_HAS_COLOR_TEMP
#include "dimmer_colortemp.h"
#endif

namespace Dimmer {

    class Module: public MQTTComponent, public Buttons, public Form {
    public:
        Module();

    #if IOT_DIMMER_MODULE_INTERFACE_UART == 0
        virtual void onConnect() override;
    #endif

        virtual bool on(uint8_t channel = -1, float transition = NAN) override;
        virtual bool off(uint8_t channel = -1, float transition = NAN) override;

        virtual uint8_t getChannelCount() const override;
        virtual bool isAnyOn() const;
        uint8_t isAnyOnInt() const;
        virtual bool getChannelState(uint8_t channel) const override;

        virtual int16_t getChannel(uint8_t channel) const override;
        virtual void setChannel(uint8_t channel, int16_t level, float transition = NAN) override;

        virtual void publishChannel(uint8_t channel) override;

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

        #if IOT_DIMMER_HAS_COLOR_TEMP
            ColorTemperature _color;
        #endif
    };

    inline Module::Module() :
        MQTTComponent(ComponentType::SENSOR)
        #if IOT_DIMMER_HAS_COLOR_TEMP
            , _color(this)
        #endif
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
        return isAnyOn() ? 1 : 0;
    }

    inline bool Module::on(uint8_t channel, float transition)
    {
        return _channels[channel].on(transition);
    }

    inline bool Module::off(uint8_t channel, float transition)
    {
        return _channels[channel].off(&_config, transition);
    }

    inline int16_t Module::getChannel(uint8_t channel) const
    {
        return _channels[channel].getLevel();
    }

    inline bool Module::getChannelState(uint8_t channel) const
    {
        return _channels[channel].getOnState();
    }

    inline void Module::setChannel(uint8_t channel, int16_t level, float transition)
    {
        _channels[channel].setLevel(level, transition);
    }

    inline uint8_t Module::getChannelCount() const
    {
        return _channels.size();
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

}
