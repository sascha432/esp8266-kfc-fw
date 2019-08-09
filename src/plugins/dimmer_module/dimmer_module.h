/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE

// Trailing edge dimmer module
// I2C or UART interface
// 1 - 8 channels
// https://github.com/sascha432/trailing_edge_dimmer
//
// default I2C pins are D3 (0) and D5 (14)
// NOTE: Wire.onReceive() is not working on ESP8266

#pragma once

#include <Arduino_compat.h>
#include <HardwareSerial.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include "../mqtt/mqtt_client.h"
#include "serial_handler.h"

#ifndef DEBUG_IOT_DIMMER_MODULE
#define DEBUG_IOT_DIMMER_MODULE             0
#endif

#ifndef IOT_DIMMER_MODULE_CHANNELS
#define IOT_DIMMER_MODULE_CHANNELS          1
#endif

#ifndef IOT_DIMMER_MODULE_INTERFACE_UART
#define IOT_DIMMER_MODULE_INTERFACE_UART    1
#endif

#ifndef IOT_DIMMER_MODULE_BAUD_RATE
#define IOT_DIMMER_MODULE_BAUD_RATE         57600
#endif

#ifndef IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#define IOT_DIMMER_MODULE_MAX_BRIGHTNESS    16666
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART
#include "SerialTwoWire.h"
#else
#include <Wire.h>
#endif

#ifndef IOT_DIMMER_MODULE_INTERFACE_SDA
#define IOT_DIMMER_MODULE_INTERFACE_SDA     0
#endif

#ifndef IOT_DIMMER_MODULE_INTERFACE_SCL
#define IOT_DIMMER_MODULE_INTERFACE_SCL     14
#endif

typedef struct {
    struct {
        String set;
        String state;
        bool value;
    } state;
    struct {
        String set;
        String state;
        uint16_t value;
    } brightness;
} Driver_DimmerModule_MQTTComponentData_t;

typedef struct {
    uint16_t brightness[IOT_DIMMER_MODULE_CHANNELS];
    uint16_t vcc;
    uint8_t temperature;
} Driver_DimmerModule_Level_t;

class Driver_DimmerModule : public MQTTComponent
{
private:
#if IOT_DIMMER_MODULE_INTERFACE_UART
    Driver_DimmerModule(HardwareSerial &serial);
#else
    Driver_DimmerModule();
#endif

public:
    virtual ~Driver_DimmerModule();

    static void setup();

    const String getName() override;
    PGM_P getComponentName() override;
    MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format) override;
    MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format, Driver_DimmerModule_MQTTComponentData_t *data);

    void onConnect(MQTTClient *client) override;
    void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    bool on(uint8_t channel);
    bool off(uint8_t channel);
    void setLevel(uint8_t channel, float fadetime);

    static const String getStatus();

#if IOT_DIMMER_MODULE_INTERFACE_UART
    static void onData(uint8_t type, const uint8_t *buffer, size_t len);
#endif

    static void onReceive(int length);

    void writeConfig();

public:
    // for AT mode
    int16_t *getChannels() {
        _getChannels();
        return _channels;
    }
    void setChannel(uint8_t channel, int16_t level, float time) {
        _data[channel].brightness.value = level;
        _data[channel].state.value = (level != 0);
        setLevel(channel, time);
    }
    void writeEEPROM() {
        _writeState();
    }
    static Driver_DimmerModule *getInstance() {
        return _dimmer;
    }

private:
#if DEBUG_IOT_DIMMER_MODULE
    uint8_t endTransmission();
#else
    inline uint8_t endTransmission() {
        return _wire->endTransmission();
    }
#endif
    void printStatus(PrintHtmlEntitiesString &out);

    void begin();
    void end();

    void _publishState(uint8_t channel, MQTTClient *client);
    void _writeState();

    void _getChannels();
    void _fade(uint8_t channel, int16_t toLevel, float fadeTime);
    void _onReceive(int length);

private:
    Driver_DimmerModule_MQTTComponentData_t _data[IOT_DIMMER_MODULE_CHANNELS];
    Driver_DimmerModule_Level_t _stored;
    int16_t _channels[IOT_DIMMER_MODULE_CHANNELS];
    uint8_t _qos;
    String _metricsTopic;
    float _fadeTime;
    float _onOffFadeTime;

#if IOT_DIMMER_MODULE_INTERFACE_UART
    HardwareSerial &_serial;
    SerialTwoWire *_wire;
#else
    TwoWire *_wire;
#endif

    static Driver_DimmerModule *_dimmer;
};

#endif
