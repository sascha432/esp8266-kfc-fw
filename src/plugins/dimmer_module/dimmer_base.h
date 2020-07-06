/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

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

#include "firmware_protocol.h"

#ifndef STK500V1_RESET_PIN
#error STK500V1_RESET_PIN not defined
#endif

#if IOT_SENSOR && (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
#include "./plugins/sensor/sensor.h"
#include "./plugins/sensor/Sensor_HLW80xx.h"
#endif

#if IOT_DIMMER_MODULE_INTERFACE_UART
using DimmerTwoWireClass = SerialTwoWire;
#else
using DimmerTwoWireClass = TwoWire;
#endif

class DimmerTwoWireEx : public DimmerTwoWireClass
{
public:
    using DimmerTwoWireClass::read;
    using DimmerTwoWireClass::write;

    template<class T>
    size_t read(T &data) {
        return readBytes(reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }

    template<class T>
    size_t write(const T &data) {
        return write(reinterpret_cast<const uint8_t *>(&data), sizeof(data));
    }

    uint8_t endTransmission(uint8_t sendStop)
    {
#if DEBUG_IOT_DIMMER_MODULE
        return _debug_print_result(DimmerTwoWireClass::endTransmission(sendStop));
#else
        return DimmerTwoWireClass::endTransmission(sendStop);
#endif
    }

    uint8_t endTransmission() {
        return endTransmission(true);
    }


#if IOT_DIMMER_MODULE_INTERFACE_UART

    DimmerTwoWireEx(Stream &stream) : DimmerTwoWireClass(stream), _locked(false) {}

    bool lock() {
        if (_locked) {
            _debug_println("Wire locked");
            return false;
        }
        _locked = true;
        return true;
    }

    void unlock() {
        _locked = false;
    }

private:
    bool _locked;
#else

    using DimmerTwoWireClass::DimmerTwoWireClass;

    bool lock() {
        noInterrupts();
        return true;
    }

    void unlock() {
        interrupts();
        delay(1);
    }

#endif
};

class DimmerChannel;
class AsyncWebServerRequest;

class Dimmer_Base : public WebUIInterface {
public:
    static const uint32_t METRICS_DEFAULT_UPDATE_RATE = 60e3;

    Dimmer_Base();

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

    // read config from dimmer
    virtual void readConfig();
    // write config to dimmer
    virtual void writeConfig();
    // store dimmer config in EEPROM
    void writeEEPROM(bool noLocking = false);

protected:
    friend DimmerChannel;

    void _begin();
    void _end();

    void _printStatus(Print &out);
    void _updateMetrics(const dimmer_metrics_t &metrics);

    void _fade(uint8_t channel, int16_t toLevel, float fadeTime);
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    void _setDimmingLevels();
#endif

    inline float getFadeTime() {
        return _fadeTime;
    }
    inline float getOnOffFadeTime() {
        return _onOffFadeTime;
    }

protected:
    String _getMetricsTopics(uint8_t num) const;

    static const uint16_t DIMMER_DISABLED = ~0;

    bool _isEnabled() const {
        return _version != DIMMER_DISABLED;
    }

    uint8_t _endTransmission();

    uint16_t _version;
    uint16_t _vcc;
    float _frequency;
    float _internalTemperature;
    float _ntcTemperature;
    // float _powerUsage;

    float _fadeTime;
    float _onOffFadeTime;

#if IOT_DIMMER_MODULE_INTERFACE_UART
    DimmerTwoWireEx _wire;

public:
    virtual void _onReceive(size_t length);
#else
    DimmerTwoWireEx &_wire;
    EventScheduler::Timer _timer;

    void _fetchMetrics();
#endif

public:
    virtual void getValues(JsonArray &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

public:
    static void setupWebServer();
    static void handleWebServer(AsyncWebServerRequest *request);
    static void resetDimmerMCU();
};
