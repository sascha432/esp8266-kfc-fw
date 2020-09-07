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
#include "dimmer_module_form.h"
#include "dimmer_channel.h"
#include "dimmer_base.h"

// number of channels
#ifndef IOT_DIMMER_MODULE_CHANNELS
#define IOT_DIMMER_MODULE_CHANNELS          1
#endif

// max. brightness level
#ifndef IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#define IOT_DIMMER_MODULE_MAX_BRIGHTNESS    16666
#endif

// enable or disable buttons
#ifndef IOT_DIMMER_MODULE_HAS_BUTTONS
#define IOT_DIMMER_MODULE_HAS_BUTTONS       0
#endif

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#if !PIN_MONITOR
#error PIN_MONITOR=1 required
#endif

#include <PinMonitor.h>

class DimmerButtonsImpl;
using DimmerButtons = DimmerButtonsImpl;

#else

class DimmerNoButtonsImpl;
using DimmerButtons = DimmerNoButtonsImpl;

#endif

// pins for dimmer buttons
// each channel needs 2 buttons, up & down
#ifndef IOT_DIMMER_MODULE_BUTTONS_PINS
#define IOT_DIMMER_MODULE_BUTTONS_PINS      D6, D7
#endif

#ifndef IOT_DIMMER_MODULE_PINMODE
#define IOT_DIMMER_MODULE_PINMODE           INPUT
#endif

#ifndef IOT_SWITCH_PRESSED_STATE
#define IOT_SWITCH_PRESSED_STATE            PinMonitor::ActiveStateType::PRESSED_WHEN_LOW
#endif

//
// DimmerButtons -> [ DimmerButtonsImpl -> ] DimmerChannels -> Dimmer_Base
//
#include "dimmer_buttons.h"

class Driver_DimmerModule: public MQTTComponent, public DimmerButtons, public DimmerModuleForm
{
public:
    Driver_DimmerModule();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override {
        return nullptr;
    }
    virtual uint8_t getAutoDiscoveryCount() const override {
        return 0;
    }
    virtual void onConnect(MQTTClient *client) override;

    virtual bool on(uint8_t channel = -1) override;
    virtual bool off(uint8_t channel = -1) override;
    virtual int16_t getChannel(uint8_t channel) const override;
    virtual bool getChannelState(uint8_t channel) const override;
    virtual void setChannel(uint8_t channel, int16_t level, float time = -1) override;
    virtual uint8_t getChannelCount() const override {
        return _channels.size();
    }

    virtual void _onReceive(size_t length) override;

protected:
    void _begin();
    void _end();
    void _beginMqtt();
    void _endMqtt();
    void _printStatus(Print &out);

private:
    void _getChannels();
};
