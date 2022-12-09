/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../src/plugins/sensor/sensor.h"
#include "dimmer_colortemp.h"
#include "dimmer_def.h"
#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include <kfc_fw_config.h>
#include <serial_handler.h>
#include <web_server.h>

#ifndef STK500V1_RESET_PIN
#    error STK500V1_RESET_PIN not defined
#endif

#if IOT_SENSOR && (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
#    include "../src/plugins/sensor/Sensor_HLW80xx.h"
#    include "../src/plugins/sensor/sensor.h"
#endif

#if !IOT_DIMMER_MODULE && !IOT_ATOMIC_SUN_V2
#    error requires IOT_ALARM_PLUGIN_ENABLED=1 or IOT_ATOMIC_SUN_V2=1
#endif

#if IOT_DIMMER_MODULE_HAS_BUTTONS
#    if !PIN_MONITOR
#        error PIN_MONITOR=1 required
#    endif
#    include <PinMonitor.h>
#endif

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using Plugins = KFCConfigurationClasses::PluginsType;

class AsyncWebServerRequest;

#include "firmware_protocol.h"

namespace Dimmer {

    struct ConfigType {

        using DimmerConfig_t = KFCConfigurationClasses::Plugins::DimmerConfigNS::Dimmer::DimmerConfig_t;

        ConfigType() :
            _version({}),
            _info({}),
            _firmwareConfig({}),
            _base({})
        {
        }

        operator bool() const
        {
            return _version;
        }

        void invalidate()
        {
            _version = dimmer_version_t({});
            _info = dimmer_config_info_t({});
            _firmwareConfig = register_mem_cfg_t({});
            _cubicInterpolation = CubicInterpolation();
        }

        dimmer_version_t _version;
        dimmer_config_info_t _info;
        register_mem_cfg_t _firmwareConfig;
        DimmerConfig_t _base;
        CubicInterpolation _cubicInterpolation;
    };

    class Channel;
    class Module;
    class ButtonsImpl;
    class NoButtonsImpl;
    class Channels;
    class Button;
    class Plugin;
    class ColorTemperature;
    class RGBChannels;
    class ChannelsArray;

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
        static const uint32_t kMetricsDefaultUpdateRate = 60000;

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
        virtual bool setChannel(uint8_t channel, int16_t level, float transition = NAN) = 0;
        virtual void stopFading(uint8_t channel) = 0;
        virtual void publishChannelState(uint8_t channel);
        virtual ChannelsArray &getChannels();

        virtual int16_t getRange(uint8_t channel) const = 0;
        virtual int16_t getOffset(uint8_t channel) const = 0;

        virtual void publishChannel(uint8_t channel) = 0;
        virtual void publishChannels() = 0;

        // read config from dimmer
        bool readConfig(ConfigType &config);
        // write config to dimmer
        bool writeConfig(ConfigType &config);

        void createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request);

        #if IOT_ATOMIC_SUN_V2
            const __FlashStringHelper *getChannelName(uint8_t channel);
        #endif

    protected:
        friend Channel;
        friend ColorTemperature;
        friend RGBChannels;
        friend Plugin;

        void begin();
        void end();

        void getStatus(Print &out);
        void _updateMetrics(const MetricsType &metrics);

        void _fade(uint8_t channel, int16_t toLevel, float fadeTime);
        void _stopFading(uint8_t channel);
        #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
            void _setDimmingLevels();
        #endif
        #if IOT_SENSOR_HAVE_HLW8012
            void _forceMetricsUpdate(uint8_t delay);
        #endif
        Sensor_DimmerMetrics *getMetricsSensor() const;

        // return absolute fade time for changing to another level
        float getTransitionTime(int fromLevel, int toLevel, float transitionTimeOverride);

        int16_t _calcLevel(int16_t level, uint8_t channel) const;
        int16_t _calcLevelReverse(int16_t level, uint8_t channel) const;

    protected:
        String _getMetricsTopics(uint8_t num) const;
        void _updateConfig(ConfigType &config, const ConfigReaderWriter &reader, bool status);
        uint8_t _endTransmission();

        bool _isEnabled() const;
        void _setOnOffState(bool val);

    public:
        ConfigType &_getConfig();

    protected:
        ConfigType _config;
        MetricsType _metrics;

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
#    if AT_MODE_HELP_SUPPORTED
        ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
#    endif
        bool atModeHandler(AtModeArgs &args, const Base &dimmer, int32_t maxLevel);
#endif

    public:
        static void setupWebServer();
        static void handleWebServer(AsyncWebServerRequest *request);
        static void resetDimmerMCU();

    private:
        static uint8_t _getChannelFrom(AsyncWebServerRequest *request);

    protected:
        #if IOT_DIMMER_HAS_COLOR_TEMP
            ColorTemperature _color;
        #endif
        #if IOT_DIMMER_HAS_RGB
            RGBChannels _rgb;
        #endif
    };

    inline Base::~Base()
    {
    }

    #if IOT_ATOMIC_SUN_V2

        inline const __FlashStringHelper *Base::getChannelName(uint8_t channel)
        {
            if (channel == _color._channel_ww1) {
                return F("Warm White #1");
            }
            else if (channel == _color._channel_ww2) {
                return F("Warm White #2");
            }
            else if (channel == _color._channel_cw1) {
                return F("Cold White #1");
            }
            else if (channel == _color._channel_cw2) {
                return F("Cold White #2");
            }
            __builtin_unreachable();
        }

    #endif

    inline int16_t Base::_calcLevel(int16_t level, uint8_t channel) const
    {
        if (level == 0) {
            return 0;
        }
        auto min = _config._base.level.from[channel];
        auto max = _config._base.level.to[channel];
        return (level * (max - min) + (IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 2)) / IOT_DIMMER_MODULE_MAX_BRIGHTNESS + min;
    }

    inline int16_t Base::_calcLevelReverse(int16_t level, uint8_t channel) const
    {
        if (level == 0) {
            return 0;
        }
        auto min = _config._base.level.from[channel];
        auto max = _config._base.level.to[channel];
        auto range = max - min;
        return ((level - min) * IOT_DIMMER_MODULE_MAX_BRIGHTNESS + (range / 2)) / range;
    }

    inline bool Base::_isEnabled() const
    {
        return _config._version;
    }

    inline ConfigType &Base::_getConfig()
    {
        return _config;
    }

    inline void Base::setupWebServer()
    {
        __LDBG_printf("server=%p", WebServer::Plugin::getWebServerObject());
        WebServer::Plugin::addHandler(F("/dimmer-fw"), Base::handleWebServer);
    }

    inline void Base::resetDimmerMCU()
    {
        digitalWrite(STK500V1_RESET_PIN, LOW);
        pinMode(STK500V1_RESET_PIN, OUTPUT);
        digitalWrite(STK500V1_RESET_PIN, LOW);
        delay(10);
        pinMode(STK500V1_RESET_PIN, INPUT);
    }

    inline Sensor_DimmerMetrics *Base::getMetricsSensor() const
    {
        return SensorPlugin::getSensor<MQTT::SensorType::DIMMER_METRICS>();
    }

    inline void Base::publishChannelState(uint8_t channel)
    {
        // do nothing
    }

    inline ChannelsArray &Base::getChannels()
    {
        __DBG_panic("pure virtual");
        return *(ChannelsArray *)(nullptr);
    }

}

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_disable.h>
#endif
