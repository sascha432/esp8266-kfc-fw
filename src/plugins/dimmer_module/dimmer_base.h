/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include <serial_handler.h>
#include <kfc_fw_config.h>
#include "../src/plugins/sensor/Sensor_DimmerMetrics.h"
#include "dimmer_def.h"

#if IOT_DIMMER_MODULE_INTERFACE_UART
#include <SerialTwoWire.h>
using DimmerTwoWireClass = SerialTwoWire;
#else
#include <Wire.h>
using DimmerTwoWireClass = TwoWire;
#endif

#include "firmware_protocol.h"

#ifndef STK500V1_RESET_PIN
#error STK500V1_RESET_PIN not defined
#endif

#if IOT_SENSOR && (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
#include "../src/plugins/sensor/sensor.h"
#include "../src/plugins/sensor/Sensor_HLW80xx.h"
#endif

#if (!defined(IOT_DIMMER_MODULE) || !IOT_DIMMER_MODULE) && (!defined(IOT_ATOMIC_SUN_V2) || !IOT_ATOMIC_SUN_V2)
#error requires IOT_ALARM_PLUGIN_ENABLED=1 or IOT_ATOMIC_SUN_V2=1
#endif

class DimmerButtonsImpl;
class DimmerNoButtonsImpl;
class DimmerChannel;
class DimmerChannels;
class DimmerButton;
class AsyncWebServerRequest;

#if IOT_DIMMER_MODULE_HAS_BUTTONS
#if !PIN_MONITOR
#error PIN_MONITOR=1 required
#endif
#include <PinMonitor.h>
using DimmerButtons = DimmerButtonsImpl;
#else
using DimmerButtons = DimmerNoButtonsImpl;
#endif

using KFCConfigurationClasses::Plugins;

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
        return DimmerTwoWireClass::endTransmission(sendStop);
    }

    uint8_t endTransmission() {
        return endTransmission(true);
    }


#if IOT_DIMMER_MODULE_INTERFACE_UART

    DimmerTwoWireEx(Stream &stream) : DimmerTwoWireClass(stream, SerialHandler::Wrapper::pollSerial), _locked(false) {}

    bool lock() {
        noInterrupts();
        if (_locked) {
            interrupts();
            __DBG_print("Wire locked");
            return false;
        }
        interrupts();
        _locked = true;
        return true;
    }

    void unlock() {
        noInterrupts();
        _locked = false;
        interrupts();
    }

private:
    volatile bool _locked;
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

class Dimmer_Base {
public:
    static const uint32_t METRICS_DEFAULT_UPDATE_RATE = 60000;
    using ConfigType = Plugins::DimmerConfig::DimmerConfig_t;

    // stepSize = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * ((repeatTime / 1000.0) / fadetime)
    // fadetime = (IOT_DIMMER_MODULE_MAX_BRIGHTNESS * repeatTime) / (1000.0 * stepSize)
    // repeatTime = (1000 * fadetime * stepSize) / IOT_DIMMER_MODULE_MAX_BRIGHTNESS

public:
    Dimmer_Base();
    virtual ~Dimmer_Base() {}

#if IOT_DIMMER_MODULE_INTERFACE_UART
    static void onData(Stream &client);
    static void onReceive(int length);
#else
    static void fetchMetrics(Event::CallbackTimerPtr timer);
#endif

    virtual bool on(uint8_t channel = -1) = 0;
    virtual bool off(uint8_t channel = -1) = 0;
    virtual int16_t getChannel(uint8_t channel) const = 0;
    virtual bool getChannelState(uint8_t channel) const = 0;
    virtual void setChannel(uint8_t channel, int16_t level, float time = -1) = 0;
    virtual uint8_t getChannelCount() const = 0;

    // read config from dimmer
    void _readConfig(ConfigType &config);
    // write config to dimmer
    void _writeConfig(ConfigType &config);
    // store dimmer config in EEPROM
    void writeEEPROM(bool noLocking = false);

protected:
    friend DimmerChannel;

    void _begin();
    void _end();

    void _printStatus(Print &out);
    void _updateMetrics(const dimmer_metrics_t &metrics);
    void _readVersion();

    void _fade(uint8_t channel, int16_t toLevel, float fadeTime);
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    void _setDimmingLevels();
#endif
    void _forceMetricsUpdate(uint8_t delay);
    Sensor_DimmerMetrics *getMetricsSensor() const;

    // return fade time for changing to another level
    float getFadeTime(int curLevel, int newLevel) {
        if (newLevel == 0) {
            return _config.off_fadetime;
        }
        if (curLevel == 0 && newLevel) {
            return _config.on_fadetime;
        }
        return _config.lp_fadetime;
    }

protected:
    String _getMetricsTopics(uint8_t num) const;

    using Version = DimmerRetrieveVersionLegacy::Version;
    static constexpr uint16_t DIMMER_DISABLED = DimmerRetrieveVersionLegacy::INVALID_VERSION;

    bool _isEnabled() const {
        return _version;
    }

    uint8_t _endTransmission();

    Version _version;
    DimmerMetrics _metrics;
    ConfigType _config;

    inline ConfigType &_getConfig() {
        return _config;
    }

protected:
#if IOT_DIMMER_MODULE_INTERFACE_UART
    DimmerTwoWireEx _wire;
    SerialHandler::Client *_client;

public:
    virtual void _onReceive(size_t length);
#else
    DimmerTwoWireEx &_wire;
    Event::Timer _timer;

    void _fetchMetrics();
#endif

// WebUI
protected:
    void _getValues(JsonArray &array);
    void _setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

#if AT_MODE_SUPPORTED
// AT Mode
protected:
    void _atModeHelpGenerator(PGM_P name);
    bool _atModeHandler(AtModeArgs &args, const Dimmer_Base &dimmer, int32_t maxLevel);
#endif

public:
    static void setupWebServer();
    static void handleWebServer(AsyncWebServerRequest *request);
    static void resetDimmerMCU();
};
