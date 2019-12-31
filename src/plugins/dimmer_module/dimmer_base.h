/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include <EventScheduler.h>
#if IOT_DIMMER_MODULE_INTERFACE_UART
#include <HardwareSerial.h>
#include "SerialTwoWire.h"
#else
#include <Wire.h>
#endif

#ifndef STK500V1_RESET_PIN
#error STK500V1_RESET_PIN not defined
#endif

class DimmerChannel;
class AsyncWebServerRequest;

class Dimmer_Base : public WebUIInterface {
public:
    static const uint32_t METRICS_DEFAULT_UPDATE_RATE = 60e3;

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
    void writeEEPROM(bool noLocking = false);

protected:
    friend DimmerChannel;

    void _begin();
    void _end();

    void _printStatus(Print &out);
    void _updateMetrics(uint16_t vcc, float frequency, float internalTemperature, float ntcTemperature);

    void _fade(uint8_t channel, int16_t toLevel, float fadeTime);

    inline float getFadeTime() {
        return _fadeTime;
    }
    inline float getOnOffFadeTime() {
        return _onOffFadeTime;
    }

    inline bool _lockWire() {
#if IOT_DIMMER_MODULE_INTERFACE_UART
        if (_wireLocked) {
            _debug_println("Wire locked");
            return false;
        }
        _wireLocked = true;
#else
        noInterrupts();
#endif
        return true;
    }
    inline void _unlockWire() {
#if IOT_DIMMER_MODULE_INTERFACE_UART
        _wireLocked = false;
#else
        interrupts();
        delay(1);
#endif
    }
protected:
    String _getMetricsTopics(uint8_t num) const;

#if DEBUG_IOT_DIMMER_MODULE
    uint8_t _endTransmission();
#else
    inline uint8_t _endTransmission() {
        return _wire.endTransmission();
    }
#endif
    uint16_t _version;
    uint16_t _vcc;
    float _frequency;
    float _internalTemperature;
    float _ntcTemperature;
    // float _powerUsage;

    float _fadeTime;
    float _onOffFadeTime;

#if IOT_DIMMER_MODULE_INTERFACE_UART
    HardwareSerial &_serial;
    SerialTwoWire &_wire;
    bool _wireLocked;

    void _onReceive(size_t length);
#else
    TwoWire &_wire;
    EventScheduler::TimerPtr _timer;

    void _fetchMetrics();
#endif

public:
    virtual void getValues(JsonArray &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

public:
    static void setupWebServer();
    static void handleWebServer(AsyncWebServerRequest *request);
    static void resetDimmerFirmware();
};

#endif
