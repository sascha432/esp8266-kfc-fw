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
