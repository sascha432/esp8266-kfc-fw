/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include <serial_handler.h>
#include "dimmer_def.h"
#include "../src/plugins/sensor/Sensor_DimmerMetrics.h"

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

#if IOT_DIMMER_MODULE_HAS_BUTTONS
#if !PIN_MONITOR
#error PIN_MONITOR=1 required
#endif
#include <PinMonitor.h>
#endif

using Plugins = KFCConfigurationClasses::PluginsType;

class AsyncWebServerRequest;

#include "firmware_protocol.h"

namespace Dimmer {

    using ConfigType = Plugins::DimmerConfig::DimmerConfig_t;

    class Channel;
    class Module;
    class ButtonsImpl;
    class NoButtonsImpl;
    class Channels;
    class Button;
    class Plugin;

    extern Plugin dimmer_plugin;

    #if IOT_DIMMER_MODULE_HAS_BUTTONS
        class ButtonsImpl;
        using Buttons = ButtonsImpl;
    #else
        class NoButtonsImpl;
        using Buttons = NoButtonsImpl;
    #endif

    // class MinMaxLevel {
    // public:
    //     MinMaxLevel(uint16_t minLevel, uint16_t maxLevel) : _minLevel(minLevel), _maxLevel(maxLevel) {
    //     }
    //     inline int16_t getMin() const {
    //         return _minLevel;
    //     }
    //     inline int16_t getMax() const {
    //         return _maxLevel;
    //     }
    //     inline int16_t getRange() {
    //         return _maxLevel - _minLevel;
    //     }

    // private:
    //     uint16_t _minLevel;
    //     uint16_t _maxLevel;
    // };

    // using MinMaxLevelArray = std::array<MinMaxLevel, IOT_DIMMER_MODULE_CHANNELS>;

    // --------------------------------------------------------------------
    // Dimmer::Base
    // --------------------------------------------------------------------

    class Base {
    public:
        static const uint32_t METRICS_DEFAULT_UPDATE_RATE = 60000;

        // stepSize = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * ((repeatTime / 1000.0) / fadetime)
        // fadetime = (IOT_DIMMER_MODULE_MAX_BRIGHTNESS * repeatTime) / (1000.0 * stepSize)
        // repeatTime = (1000 * fadetime * stepSize) / IOT_DIMMER_MODULE_MAX_BRIGHTNESS

    public:
        Base();
        virtual ~Base();

    #if IOT_DIMMER_MODULE_INTERFACE_UART
        static void onData(Stream &client);
        static void onReceive(int length);
    #else
        static void fetchMetrics(Event::CallbackTimerPtr timer);
    #endif

        virtual bool on(uint8_t channel = -1, float transition = NAN) = 0;
        virtual bool off(uint8_t channel = -1, float transition = NAN) = 0;
    #if IOT_DIMMER_MODULE_HAS_BUTTONS
        virtual bool isAnyOn() const = 0;
    #endif
        virtual int16_t getChannel(uint8_t channel) const = 0;
        virtual bool getChannelState(uint8_t channel) const = 0;
        virtual void setChannel(uint8_t channel, int16_t level, float transition = NAN) = 0;
        virtual uint8_t getChannelCount() const = 0;

        virtual int16_t getRange(uint8_t channel) const = 0;
        virtual int16_t getOffset(uint8_t channel) const = 0;

        virtual void publishChannel(uint8_t channel) = 0;

        // read config from dimmer
        void readConfig(ConfigType &config);
        // write config to dimmer
        void writeConfig(ConfigType &config);

    protected:
        friend Channel;

        void begin();
        void end();

        void getStatus(Print &out);
        void _updateMetrics(const MetricsType &metrics);

        void _fade(uint8_t channel, int16_t toLevel, float fadeTime);
    #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
        void _setDimmingLevels();
    #endif
        void _forceMetricsUpdate(uint8_t delay);
        Sensor_DimmerMetrics *getMetricsSensor() const;

        // return absoluate fade time for changing to another level
        float getTransitionTime(int fromLevel, int toLevel, float transitionTimeOverride);

        int16_t _calcLevel(int16_t level, uint8_t channel) const;
        int16_t _calcLevelReverse(int16_t level, uint8_t channel) const;

    protected:
        String _getMetricsTopics(uint8_t num) const;
        void _updateConfig(ConfigType &config, ConfigReaderWriter &reader, bool status);
        uint8_t _endTransmission();

        bool _isEnabled() const;
        ConfigType &_getConfig();

    protected:
        MetricsType _metrics;
        ConfigType _config;
        // MinMaxLevelArray _minMaxLevel;

    protected:
    #if IOT_DIMMER_MODULE_INTERFACE_UART
        TwoWire _wire;
        SerialHandler::Client *_client;

    public:
        virtual void _onReceive(size_t length);
    #else
        TwoWire &_wire;
        Event::Timer _timer;

        void _fetchMetrics();
    #endif

    // WebUI
    protected:
        void getValues(WebUINS::Events &array);
        void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

    // ATMode
    #if AT_MODE_SUPPORTED
    protected:
        ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
        bool atModeHandler(AtModeArgs &args, const Base &dimmer, int32_t maxLevel);
    #endif

    public:
        static void setupWebServer();
        static void handleWebServer(AsyncWebServerRequest *request);
        static void resetDimmerMCU();
    };

    inline Base::~Base()
    {
    }

    inline int16_t Base::_calcLevel(int16_t level, uint8_t channel) const
    {
        if (level == 0) {
            return 0;
        }
        auto min = _config.level.from[channel];
        auto max = _config.level.to[channel];
        return (level * (max - min) + (IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 2)) / IOT_DIMMER_MODULE_MAX_BRIGHTNESS + min;
    }

    inline int16_t Base::_calcLevelReverse(int16_t level, uint8_t channel) const
    {
        if (level == 0) {
            return 0;
        }
        auto min = _config.level.from[channel];
        auto max = _config.level.to[channel];
        auto range = max - min;
        return ((level - min) * IOT_DIMMER_MODULE_MAX_BRIGHTNESS + (range / 2)) / range;
    }

    inline bool Base::_isEnabled() const
    {
        return _config.version;
    }

    inline ConfigType &Base::_getConfig()
    {
        return _config;
    }

}
