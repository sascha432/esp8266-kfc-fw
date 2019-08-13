/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_ATOMIC_SUN_V2

#include <Arduino_compat.h>
#include <HardwareSerial.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include "../mqtt/mqtt_client.h"
#include "serial_handler.h"

#ifndef DEBUG_4CH_DIMMER
#define DEBUG_4CH_DIMMER 0
#endif

#ifndef IOT_ATOMIC_SUN_BAUD_RATE
#define IOT_ATOMIC_SUN_BAUD_RATE            57600
#endif

#ifndef IOT_ATOMIC_SUN_MAX_BRIGHTNESS
#define IOT_ATOMIC_SUN_MAX_BRIGHTNESS       8333
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
        int16_t value;
    } brightness;
    struct {
        String set;
        String state;
        uint16_t value;
    } color;
} Driver_4ChDimmer_MQTTComponentData_t;

typedef struct {
    uint16 brightness;
    uint16_t vcc;
    uint8_t temperature;
} Driver_4ChDimmer_Level_t;

class Driver_4ChDimmer : public MQTTComponent
{
private:
    Driver_4ChDimmer(HardwareSerial &serial);

public:
    virtual ~Driver_4ChDimmer();

    static void setup();

    MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format) override;
    void onConnect(MQTTClient *client) override;
    void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    bool on();
    bool off();
    void setLevel(float fadetime);

    static const String getStatus();
    static void onData(uint8_t type, const uint8_t *buffer, size_t len);

    static void onReceive(int length);

    void writeConfig();
    static Driver_4ChDimmer *getInstance() {
        return _dimmer;
    }

    inline bool getOnState() const {
        return _data.state.value;
    }
    inline int16_t getBrightness() const {
        return _data.brightness.value;
    }
    inline uint16_t getColor() const {
        return _data.color.value;
    }
    inline void setColor(uint16_t color) {
        _data.color.value = color;
    }
    void setBrightness(int16_t level) {
        _data.brightness.value = level;
        _data.state.value = level != 0;
    }

    void publishState(MQTTClient *client = nullptr);

private:
    void _printStatus(PrintHtmlEntitiesString &out);
    void _createTopics();

    void begin();
    void end();

    void _publishState(MQTTClient *client);
    void _writeState();

    void _setChannel(uint8_t channel, int16_t brightness, float fadetime);
    void _setChannels(int16_t ch1, int16_t ch2, int16_t ch3, int16_t ch4, float fadetime);
    void _getChannels();

    void _fade(uint8_t channel, int16_t toLevel, float fadeTime);
    void _onReceive(int length);

private:
#if DEBUG_4CH_DIMMER
    uint8_t endTransmission();
#else
    inline uint8_t endTransmission();
#endif

    inline float _getFadeTime() {
        return _fadeTime;
    }
    inline float _getOnOffFadeTime() {
        return _onOffFadeTime;
    }

    HardwareSerial &_serial;
    Driver_4ChDimmer_MQTTComponentData_t _data;
    Driver_4ChDimmer_Level_t _stored;
    int16_t _channels[4];
    uint8_t _qos;
    String _metricsTopic;
    float _fadeTime;
    float _onOffFadeTime;

    static Driver_4ChDimmer *_dimmer;
};

#endif
