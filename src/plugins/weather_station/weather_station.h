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
#include <NeoPixelEx.h>
#include "plugins.h"
#if IOT_ALARM_PLUGIN_ENABLED
#include "../src/plugins/alarm/alarm.h"
#endif

class WeatherStationPlugin : public PluginComponent, public WeatherStationBase {
// PluginComponent
public:
    WeatherStationPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

// WebUI
public:
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

public:
#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

    static WeatherStationPlugin &_getInstance();

private:
    void _readConfig();
    void _init();
    static void _sendScreenCaptureBMP(AsyncWebServerRequest *request);
    void _installWebhooks();

private:
    void _drawEnvironmentalSensor(GFXCanvasCompressed& canvas, uint8_t top);
    virtual void _getIndoorValues(float *data) override;

    void _fadeBacklight(uint16_t fromLevel, uint16_t toLevel, uint8_t step = 8);
    void _fadeStatusLED();

    virtual void canvasUpdatedEvent(int16_t x, int16_t y, int16_t w, int16_t h) override;

private:
    asyncHTTPrequest *_httpClient;

#if IOT_WEATHER_STATION_WS2812_NUM
    Event::Timer _pixelTimer;
    uint8_t _pixels[IOT_WEATHER_STATION_WS2812_NUM * 3];
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
