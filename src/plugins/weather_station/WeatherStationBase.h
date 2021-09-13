/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <OpenWeatherMapAPI.h>
#include "RestAPI.h"
#include "WSDraw.h"

#ifndef DEBUG_IOT_WEATHER_STATION
#    define DEBUG_IOT_WEATHER_STATION 1
#endif

#ifndef IOT_WEATHER_STATION_HAS_TOUCHPAD
#    define IOT_WEATHER_STATION_HAS_TOUCHPAD 1
#endif

// calculate RH from compensated temperature
#ifndef IOT_WEATHER_STATION_COMP_RH
#    define IOT_WEATHER_STATION_COMP_RH 0
#endif

// IRC pin
#ifndef IOT_WEATHER_STATION_MPR121_PIN
#    define IOT_WEATHER_STATION_MPR121_PIN 12
#endif

// number of pixels
#ifndef IOT_WEATHER_STATION_WS2812_NUM
#    ifndef __LED_BUILTIN_WS2812_NUM_LEDS
#        define IOT_WEATHER_STATION_WS2812_NUM 0
#    else
#        define IOT_WEATHER_STATION_WS2812_NUM __LED_BUILTIN_WS2812_NUM_LEDS
#    endif
#endif

// WS2812 data pin
#ifndef IOT_WEATHER_STATION_WS2812_PIN
#    ifndef __LED_BUILTIN_WS2812_PIN
#        define IOT_WEATHER_STATION_WS2812_PIN 0
#    else
#        define IOT_WEATHER_STATION_WS2812_PIN __LED_BUILTIN_WS2812_PIN
#    endif
#endif

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
#    include "Mpr121Touchpad.h"
#endif

class WeatherStationBase : public WSDraw::Base
{
public:
    // in words:
    // poll weather information 60 seconds after booting and continue with configured interval
    // if an error occurs, retry after 60 seconds for 10 times, then retry every 15min.

    // delay after boot or if any error occured
    static constexpr auto kPollDataErrorDelay = static_cast<uint32_t>(Event::seconds(60).count());
    // retries before pausing
    static constexpr uint16_t kPollDataRetries = 10;
    // pause for some time after too many errors
    static constexpr auto kPollDataErrorPauseDelay = static_cast<uint32_t>(Event::minutes(15).count());
    // min. delay between updates in case the configuration is invalid
    static constexpr auto kMinPollDataInterval = static_cast<uint32_t>(Event::minutes(5).count());

    static constexpr auto kNumScreens = WSDraw::kNumScreens;

    using HttpRequestCallback = KFCRestAPI::HttpRequest::Callback_t;
    using ScreenType = WSDraw::ScreenType;

public:
    WeatherStationBase();

    static void loop();
    static void wifiCallback(WiFiCallbacks::EventType event, void *payload);
    static void pollDataTimer(Event::CallbackTimerPtr timer);

    static WeatherStationBase &_getInstance();

#if !_MSC_VER
protected:
#endif
    void _loop();
    void _wifiCallback(WiFiCallbacks::EventType event, void *payload);
    void _pollDataTimerCallback(Event::CallbackTimerPtr timer);
    void _pollDataUpdateLastTime(bool success);
    uint32_t _pollDataGetNextUpdate() const;

    void _httpRequest(const String &url, int timeout, JsonBaseReader *jsonReader, HttpRequestCallback callback);
    void _getWeatherInfo(HttpRequestCallback callback);
    void _getWeatherForecast(HttpRequestCallback callback);

    void begin();
    void end();


protected:
    void _setScreen(ScreenType screen);
    void _setScreen(uint32_t screen);
    ScreenType _getNextScreen(ScreenType screen);
    ScreenType _getNextScreen(uint32_t screen);
    uint32_t _getCurrentScreen() const;

protected:
    Event::Timer _pollDataTimer;
    uint32_t _pollDataLastMillis;
    uint16_t _pollDataRetries;

    // uint32_t _updateTimer;
    uint32_t _updateCounter;
    uint16_t _backlightLevel;
    Event::Timer _fadeTimer;

    uint32_t _toggleScreenTimer;
    uint32_t _lockCanvasUpdateEvents;

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        Mpr121Touchpad _touchpad;
        bool _touchpadDebug{false};
    #endif
};

inline void WeatherStationBase::loop()
{
    _getInstance()._loop();
}

inline void WeatherStationBase::wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    _getInstance()._wifiCallback(event, payload);
}

inline void WeatherStationBase::pollDataTimer(Event::CallbackTimerPtr timer)
{
    _getInstance()._pollDataTimerCallback(timer);
}

inline void WeatherStationBase::_setScreen(uint32_t screen)
{
    _setScreen(static_cast<ScreenType>(screen));
}

inline WeatherStationBase::ScreenType WeatherStationBase::_getNextScreen(uint32_t screen)
{
    return _getNextScreen(static_cast<ScreenType>(screen));
}

inline uint32_t WeatherStationBase::_getCurrentScreen() const
{
    return static_cast<uint32_t>(_currentScreen);
}

class WeatherStationPlugin;

extern WeatherStationPlugin ws_plugin;

inline WeatherStationBase &WeatherStationBase::_getInstance()
{
    return *reinterpret_cast<WeatherStationBase *>(&ws_plugin);
}
