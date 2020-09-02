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
#include "NeoPixel_esp.h"
#include "plugins.h"
#if IOT_ALARM_PLUGIN_ENABLED
#include "./plugins/alarm/alarm.h"
#endif

class WeatherStationPlugin : public PluginComponent, public WeatherStationBase {
public:
    using WeatherStation = KFCConfigurationClasses::Plugins::WeatherStation;
    using MainConfig = KFCConfigurationClasses::MainConfig;

// PluginComponent
public:
    WeatherStationPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;

// WebUI
public:
    virtual void createWebUI(WebUI &webUI) override;
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

public:
#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

private:
    void _readConfig();
    void _init();
    static WeatherStationPlugin &_getInstance();
    static void _sendScreenCaptureBMP(AsyncWebServerRequest *request);
    void _installWebhooks();

private:
    void _drawEnvironmentalSensor(GFXCanvasCompressed& canvas, uint8_t top);
    virtual void _getIndoorValues(float *data) override;

    void _fadeBacklight(uint16_t fromLevel, uint16_t toLevel, uint8_t step = 8);
    void _fadeStatusLED();

    virtual void canvasUpdatedEvent(int16_t x, int16_t y, int16_t w, int16_t h) override;

private:
    // uint32_t _updateTimer;
    uint32_t _updateCounter;
    uint16_t _backlightLevel;
    Event::Timer _fadeTimer;

private:
    asyncHTTPrequest *_httpClient;

#if IOT_WEATHER_STATION_WS2812_NUM
    Event::Timer _pixelTimer;
    uint8_t _pixels[IOT_WEATHER_STATION_WS2812_NUM * 3];
#endif

private:
    void _setScreen(uint8_t screen);
    uint8_t _getNextScreen(uint8_t screen);

    uint32_t _toggleScreenTimer;
    uint32_t _lockCanvasUpdateEvents;

#if IOT_ALARM_PLUGIN_ENABLED
public:
    using Alarm = KFCConfigurationClasses::Plugins::Alarm;

    static void alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);

private:
    void _alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);
    bool _resetAlarm(); // returns true if alarm was reset

    Event::Timer _alarmTimer;
    Event::Callback _resetAlarmFunc;
#endif
};
