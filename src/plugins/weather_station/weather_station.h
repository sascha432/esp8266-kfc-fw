/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <SPI.h>
#include <vector>
#include <StreamString.h>
#include "WebUIComponent.h"
#include "WeatherStationBase.h"
#include "../src/plugins/plugins.h"
#include "../src/plugins/mqtt/mqtt_client.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include <../src/plugins/mqtt/mqtt_client.h>
#include "../src/plugins/ntp/ntp_plugin.h"
#if IOT_ALARM_PLUGIN_ENABLED
#    include "../src/plugins/alarm/alarm.h"
#endif

class WeatherStationPlugin : public PluginComponent, public WeatherStationBase, public MQTTComponent {
// PluginComponent
public:
    WeatherStationPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

    virtual void createMenu() override;

// WebUI and MQTTComponent helpers
public:
    void publishDelayed();
    virtual void publishNow();

private:
    Event::Timer _publishTimer;
    uint32_t _publishLastTime{9};

// WebUI
public:
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;
    void _publishWebUI();

// MQTTComponent
public:
    enum class TopicType : uint8_t {
        COMMAND_SET,
        COMMAND_STATE,
    };

    virtual MQTT::AutoDiscovery::EntityPtr getAutoDiscovery(MQTT::FormatType format, uint8_t num);
    inline uint8_t getAutoDiscoveryCount() const;
    virtual void onConnect() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len);

private:
    String _createTopics(TopicType type) const;
    void onJsonMessage(const MQTT::Json::Reader &json);
    void _publishMQTT();

// AT commands
public:
    #if AT_MODE_SUPPORTED
        #if AT_MODE_HELP_SUPPORTED
            virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
        #endif
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

    static WeatherStationPlugin &_getInstance();

private:
    void _readConfig();
    void _initTFT();
    #if WEATHER_STATION_HAVE_BMP_SCREENSHOT
        void __recvTFTCtrl(AsyncWebServerRequest *request);
        static void _recvTFTCtrl(AsyncWebServerRequest *request);
        void __sendScreenCaptureBMP(AsyncWebServerRequest *request);
        static void _sendScreenCaptureBMP(AsyncWebServerRequest *request);
    #endif
    void _installWebhooks();

public:
    // pulses the status LEDs
    void _setRGBLeds(uint32_t color);
    void _fadeStatusLED();
    void _rainbowStatusLED(bool stop = false);

    int16_t _rainbowBrightness;
    #if WEATHER_STATION_HAVE_BMP_SCREENSHOT
        int32_t _updateProgress{-2};
    #endif

    static IndoorValues getIndoorValues() {
        return _getInstance()._getIndoorValues();
    }

private:
    void _drawEnvironmentalSensor(GFXCanvasCompressed& canvas, int16_t top);
    virtual IndoorValues _getIndoorValues();

    // set backlight level
    void _setBacklightLevel(uint16_t level);
    // sets the backlight level by slowly/decreasing increased it
    void _fadeBacklight(uint16_t fromLevel, uint16_t toLevel, uint8_t step = 8);
    void _updateBacklight(uint16_t toLevel, uint8_t step = 8);

private:
    asyncHTTPrequest *_httpClient;

    #if IOT_WEATHER_STATION_WS2812_NUM
        Event::Timer _pixelTimer;
        decltype(WS2812LEDTimer::_pixels) &_pixels;
    #endif

    #if IOT_ALARM_PLUGIN_ENABLED
    public:
        using ModeType = KFCConfigurationClasses::Plugins::AlarmConfigNS::ModeType;
        static constexpr auto STOP_ALARM = KFCConfigurationClasses::Plugins::AlarmConfigNS::AlarmConfigType::STOP_ALARM;

        static void alarmCallback(ModeType mode, uint16_t maxDuration);

    private:
        void _alarmCallback(ModeType mode, uint16_t maxDuration);
        bool _resetAlarm(); // returns true if alarm was reset

        Event::Timer _alarmTimer;
        Event::Callback _resetAlarmFunc;
    #endif
};

extern WeatherStationPlugin ws_plugin;

inline WeatherStationPlugin &WeatherStationPlugin::_getInstance()
{
    return ws_plugin;
}

inline void WeatherStationPlugin::createMenu()
{
    auto configMenu = bootstrapMenu.getMenuItem(navMenu.config);
    auto subMenu = configMenu.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(getFriendlyName(), F("weather.html"));
    subMenu.addMenuItem(F("World Clock"), F("world-clock.html"));
    #if HAVE_WEATHER_STATION_CURATED_ART
        subMenu.addMenuItem(F("Curated Art"), F("curated-art.html"));
    #endif
    #if WEATHER_STATION_HAVE_BMP_SCREENSHOT
        subMenu.addMenuItem(F("Show Screen"), F("display-screen.html"));
    #endif
}


inline void WeatherStationPlugin::_setBacklightLevel(uint16_t level)
{
    #if ESP32
        analogWrite(TFT_PIN_LED, level, PWMRANGE);
    #else
        analogWrite(TFT_PIN_LED, level);
    #endif
}

inline uint8_t WeatherStationPlugin::getAutoDiscoveryCount() const
{
    return 1;
}

inline void WeatherStationPlugin::onConnect()
{
    subscribe(_createTopics(TopicType::COMMAND_SET));
    publishNow();
}

inline void WeatherStationPlugin::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("topic=%s payload=%s", topic, payload);
    auto stream = HeapStream(payload, len);
    auto reader = MQTT::Json::Reader(&stream);
    if (reader.parse()) {
        onJsonMessage(reader);
    }
}

inline void WeatherStationPlugin::onJsonMessage(const MQTT::Json::Reader &json)
{
    if (json.state != -1) {
        if (json.state && !_backlightLevel) {
            _updateBacklight(_config.backlight_level);
        }
        else if (!json.state && _backlightLevel) {
            _updateBacklight(0);
        }
    }
    if (json.brightness != -1) {
        if (json.brightness == 0 && _backlightLevel) {
            _updateBacklight(0);
        }
        else if (json.brightness) {
            _updateBacklight(json.brightness);
        }
    }
}

inline void WeatherStationPlugin::_publishMQTT()
{
    if (isConnected()) {
        using namespace MQTT::Json;
        publish(_createTopics(TopicType::COMMAND_STATE), true, UnnamedObject(State(_backlightLevel != 0), Brightness(_backlightLevel), Transition(5)).toString());
    }
}

inline void WeatherStationPlugin::publishNow()
{
    _publishMQTT();
    _publishWebUI();
    _publishLastTime = millis();
}
