/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_MODULE

// Trailing edge dimmer module
// I2C or UART interface
// 1 - 8 channels
// https://github.com/sascha432/trailing_edge_dimmer
//
// default I2C pins are D3 (0) and D5 (14)
// NOTE: Wire.onReceive() is not working on ESP8266

#include <Arduino_compat.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include "kfc_fw_config.h"
#include "../mqtt/mqtt_client.h"
#include "serial_handler.h"
#include "dimmer_module_form.h"
#include "dimmer_channel.h"
#include "dimmer_base.h"

#ifndef DEBUG_IOT_DIMMER_MODULE
#define DEBUG_IOT_DIMMER_MODULE             0
#endif

// number of channels
#ifndef IOT_DIMMER_MODULE_CHANNELS
#define IOT_DIMMER_MODULE_CHANNELS          1
#endif

// use UART instead of I2C
#ifndef IOT_DIMMER_MODULE_INTERFACE_UART
#define IOT_DIMMER_MODULE_INTERFACE_UART    0
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART

// UART only change baud rate of the Serial port to match the dimmer module
#ifndef IOT_DIMMER_MODULE_BAUD_RATE
#define IOT_DIMMER_MODULE_BAUD_RATE         57600
#endif

#else

// I2C only. SDA PIN
#ifndef IOT_DIMMER_MODULE_INTERFACE_SDA
#define IOT_DIMMER_MODULE_INTERFACE_SDA     D3
#endif

// I2C only. SCL PIN
#ifndef IOT_DIMMER_MODULE_INTERFACE_SCL
#define IOT_DIMMER_MODULE_INTERFACE_SCL     D5
#endif

#endif

// max. brightness level
#ifndef IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#define IOT_DIMMER_MODULE_MAX_BRIGHTNESS    16666
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART
#include "SerialTwoWire.h"
#else
#include <Wire.h>
#endif

class DimmerModuleForm;

class Driver_DimmerModule: public Dimmer_Base, public DimmerModuleForm
{
public:
    Driver_DimmerModule();

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector);
    void onConnect(MQTTClient *client);

    virtual bool on(uint8_t channel = -1) override;
    virtual bool off(uint8_t channel = -1) override;
    virtual int16_t getChannel(uint8_t channel) const override;
    virtual bool getChannelState(uint8_t channel) const override;
    virtual void setChannel(uint8_t channel, int16_t level, float time = -1) override;
    virtual uint8_t getChannelCount() const override;

protected:
    void _begin();
    void _end();
    void _printStatus(PrintHtmlEntitiesString &out);

private:
    void _getChannels();

    DimmerChannel _channels[IOT_DIMMER_MODULE_CHANNELS];
};

class DimmerModulePlugin : public Driver_DimmerModule {
public:
    DimmerModulePlugin() : Driver_DimmerModule() {
        register_plugin(this);
    }

    virtual PGM_P getName() const override;
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)100;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;

    virtual bool hasStatus() const override;
    virtual const String getStatus() override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

#if AT_MODE_SUPPORTED && !IOT_DIMMER_MODULE_INTERFACE_UART
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
#endif
};

extern DimmerModulePlugin dimmer_plugin;

#endif
