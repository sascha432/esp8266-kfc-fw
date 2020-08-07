/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <SPI.h>
#include <vector>
#include <EventScheduler.h>
#include <StreamString.h>
#include <KFCRestApi.h>
#include "WebUIComponent.h"
#include "WSDraw.h"
#include "NeoPixel_esp.h"
#include "plugins.h"
#if IOT_ALARM_PLUGIN_ENABLED
#include "./plugins/alarm/alarm.h"
#endif

#ifndef DEBUG_IOT_WEATHER_STATION
#define DEBUG_IOT_WEATHER_STATION               0
#endif

#ifndef IOT_WEATHER_STATION_HAS_TOUCHPAD
#define IOT_WEATHER_STATION_HAS_TOUCHPAD        1
#endif

// calculate RH from compensated temperature
#ifndef IOT_WEATHER_STATION_COMP_RH
#define IOT_WEATHER_STATION_COMP_RH             0
#endif

// IRC pin
#ifndef IOT_WEATHER_STATION_MPR121_PIN
#define IOT_WEATHER_STATION_MPR121_PIN          12
#endif

// number of pixels
#ifndef IOT_WEATHER_STATION_WS2812_NUM
#ifndef __LED_BUILTIN_WS2812_NUM_LEDS
#define IOT_WEATHER_STATION_WS2812_NUM          0
#else
#define IOT_WEATHER_STATION_WS2812_NUM          __LED_BUILTIN_WS2812_NUM_LEDS
#endif
#endif

// WS2812 data pin
#ifndef IOT_WEATHER_STATION_WS2812_PIN
#ifndef __LED_BUILTIN_WS2812_PIN
#define IOT_WEATHER_STATION_WS2812_PIN          0
#else
#define IOT_WEATHER_STATION_WS2812_PIN          __LED_BUILTIN_WS2812_PIN
#endif
#endif

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
#include "Mpr121Touchpad.h"
#endif

class WeatherStationPlugin : public PluginComponent, public WSDraw {
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
    static void loop();
    // static void serialHandler(uint8_t type, const uint8_t *buffer, size_t len);

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
    typedef std::function<void (bool status)> Callback_t;

    void _drawEnvironmentalSensor(GFXCanvasCompressed& canvas, uint8_t top);
    virtual void _getIndoorValues(float *data) override;

    void _httpRequest(const String &url, int timeout, JsonBaseReader *jsonReader, Callback_t finishedCallback);
    void _getWeatherInfo(Callback_t finishedCallback);
    void _getWeatherForecast(Callback_t finishedCallback);

    // void _serialHandler(const uint8_t *buffer, size_t len);
    void _loop();

    void _fadeBacklight(uint16_t fromLevel, uint16_t toLevel, int8_t step = 16);
    void _fadeStatusLED();
    void _broadcastCanvas(int16_t x, int16_t y, int16_t w, int16_t h);

    // safely redraw in next main loop
    virtual void _redraw() override {
        _redrawFlag = true;
    }

private:
    // uint32_t _updateTimer;
    uint32_t _updateCounter;
    uint16_t _backlightLevel;

private:
    uint32_t _pollTimer;
    asyncHTTPrequest *_httpClient;
    EventScheduler::Timer _fadeTimer;

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    Mpr121Touchpad _touchpad;
    bool _touchpadDebug;
#endif

#if IOT_WEATHER_STATION_WS2812_NUM
    EventScheduler::Timer _pixelTimer;
    uint8_t _pixels[IOT_WEATHER_STATION_WS2812_NUM * 3];
#endif

private:
    void _setScreen(uint8_t screen);
    uint8_t _getNextScreen(uint8_t screen);

    uint32_t _toggleScreenTimer;
    bool _redrawFlag;

#if IOT_ALARM_PLUGIN_ENABLED
public:
    using Alarm = KFCConfigurationClasses::Plugins::Alarm;

    static void alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);

private:
    void _alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration);
    bool _resetAlarm(); // returns true if alarm was reset

    EventScheduler::Timer _alarmTimer;
    EventScheduler::Callback _resetAlarmFunc;
#endif

#if DEBUG_IOT_WEATHER_STATION
public:
    bool _debugDisplayCanvasBorder;
#endif
};
