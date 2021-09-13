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

class WeatherStationBase;

extern WeatherStationBase &__weatherStationGetPlugin();

class WeatherStationBase : public WSDraw::Base
{
public:
    // in words:
    // poll weather information 60 seconds after booting and continue with configured interval
    // if an error occurs, retry after 60 seconds for 10 times, then retry every 15min.

    // delay after boot or if any error occured
    static constexpr uint32_t kPollDataErrorDelay = (uint32_t)Event::seconds(60).count();
    // retries before pausing
    static constexpr uint16_t kPollDataRetries = 10;
    // pause for some time after too many errors
    static constexpr uint32_t kPollDataErrorPauseDelay = (uint32_t)Event::minutes(15).count();
    // min. delay between updates in case the configuration is invalid
    static constexpr uint32_t kMinPollDataInterval = (uint32_t)Event::minutes(5).count();

    using HttpRequestCallback = KFCRestAPI::HttpRequest::Callback_t;

public:
    WeatherStationBase();

    static void loop();
    static void wifiCallback(WiFiCallbacks::EventType event, void *payload);
    static void pollDataTimer(Event::CallbackTimerPtr timer);

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
    Event::Timer _pollDataTimer;
    uint32_t _pollDataLastMillis;
    uint16_t _pollDataRetries;

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        Mpr121Touchpad _touchpad;
    #endif
    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        bool _touchpadDebug: 1;
    #endif
};

inline void WeatherStationBase::loop()
{
    __weatherStationGetPlugin()._loop();
}

inline void WeatherStationBase::wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    __weatherStationGetPlugin()._wifiCallback(event, payload);
}

inline void WeatherStationBase::pollDataTimer(Event::CallbackTimerPtr timer)
{
    __weatherStationGetPlugin()._pollDataTimerCallback(timer);
}
