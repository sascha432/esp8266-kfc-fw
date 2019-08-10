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
#include "kfc_fw_config.h"
#include "../mqtt/mqtt_client.h"
#include "serial_handler.h"
#include "dimmer_channel.h"

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
#define IOT_DIMMER_MODULE_INTERFACE_SDA     0
#endif

// I2C only. SCL PIN
#ifndef IOT_DIMMER_MODULE_INTERFACE_SCL
#define IOT_DIMMER_MODULE_INTERFACE_SCL     14
#endif

#endif

// max. brightness level
#ifndef IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#define IOT_DIMMER_MODULE_MAX_BRIGHTNESS    16666
#endif

// Set this PIN to HIGH when the dimmer is initialized
#ifndef IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE
//#define IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE 14
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART
#include "SerialTwoWire.h"
#else
#include <Wire.h>
#endif

class Driver_DimmerModule //: public MQTTComponent
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

    void onConnect(MQTTClient *client, DimmerChannel *channel);
    void onMessage(MQTTClient *client, DimmerChannel *channel, char *topic, char *payload, size_t len);

    bool on(uint8_t channel);
    bool off(uint8_t channel);

    static const String getStatus();

#if IOT_DIMMER_MODULE_INTERFACE_UART
    static void onData(uint8_t type, const uint8_t *buffer, size_t len);
#endif

    static void onReceive(int length);

    void writeConfig();

    int16_t getChannel(uint8_t channel);
    void setChannel(uint8_t channel, int16_t level, float time = -1);
    void writeEEPROM();

    static Driver_DimmerModule *getInstance();

public:
    // friend class DimmerModule;
    
    inline float getFadeTime() {
        return _fadeTime;
    }
    inline float getOnOffFadeTime() {
        return _onOffFadeTime;
    }
    inline void _setChannel(uint8_t channel, int16_t level, float time) {
        _fade(channel, level, time);
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
    DimmerChannel _channels[IOT_DIMMER_MODULE_CHANNELS];

    uint16_t _vcc;
    uint8_t _temperature;
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
