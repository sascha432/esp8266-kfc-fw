/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2

#include <Arduino_compat.h>
#include <HardwareSerial.h>
#include <PrintHtmlEntitiesString.h>
#include "EventScheduler.h"
#include "WebUIComponent.h"
#if IOT_DIMMER_MODULE_INTERFACE_UART
#include "SerialTwoWire.h"
#endif

#define DIMMER_FIRMWARE_DEBUG 1

#if DIMMER_FIRMWARE_DEBUG
#include <push_pack.h>
#include "../../trailing_edge_dimmer/src/dimmer_reg_mem.h"
#include <pop_pack.h>
#endif

class DimmerChannel;

class Dimmer_Base : public WebUIInterface {
public:
    Dimmer_Base();
    virtual ~Dimmer_Base();

#if IOT_DIMMER_MODULE_INTERFACE_UART
    static void onData(uint8_t type, const uint8_t *buffer, size_t len);
    static void onReceive(int length);
#else
    static void fetchMetrics(EventScheduler::TimerPtr timer);
#endif

    virtual bool on(uint8_t channel = -1) = 0;
    virtual bool off(uint8_t channel = -1) = 0;
    virtual int16_t getChannel(uint8_t channel) const = 0;
    virtual bool getChannelState(uint8_t channel) const = 0;
    virtual void setChannel(uint8_t channel, int16_t level, float time = -1) = 0;
    virtual uint8_t getChannelCount() const = 0;

    void writeConfig();
    void writeEEPROM();

protected:
    friend DimmerChannel;

    void _begin();
    void _end();

    void _printStatus(PrintHtmlEntitiesString &out);
    void _updateMetrics(float temperature, uint16_t vcc, float frequency);

    void _fade(uint8_t channel, int16_t toLevel, float fadeTime);

    inline float getFadeTime() {
        return _fadeTime;
    }
    inline float getOnOffFadeTime() {
        return _onOffFadeTime;
    }
protected:
#if DEBUG_IOT_DIMMER_MODULE
    uint8_t _endTransmission();
#else
    inline uint8_t _endTransmission() {
        return _wire.endTransmission();
    }
#endif

    float _temperature;
    uint16_t _vcc;
    float _frequency;

    float _fadeTime;
    float _onOffFadeTime;

#if IOT_DIMMER_MODULE_INTERFACE_UART
    HardwareSerial &_serial;
    SerialTwoWire &_wire;

    void _onReceive(int length);
#else
    TwoWire &_wire;
    EventScheduler::TimerPtr _timer;

    void _fetchMetrics();
#endif

#if DIMMER_FIRMWARE_DEBUG
public:
    void resetDimmerFirmware();
    void readDimmerFirmware(Print &output, register_mem_cfg_t &config);
    void setDimmerFirmwareZCDelay(uint8_t zcDelay);
    void setDimmerFirmwareSetMinTime(uint16_t minTime);
    void setDimmerFirmwareSetMaxTime(uint16_t maxTime);
#endif

public:
    virtual void getValues(JsonArray &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);
};

#endif
