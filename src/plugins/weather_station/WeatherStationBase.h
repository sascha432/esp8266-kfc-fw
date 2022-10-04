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
#    define DEBUG_IOT_WEATHER_STATION 0
#endif

// Add MPR121 based touchpad
#ifndef IOT_WEATHER_STATION_HAS_TOUCHPAD
#    define IOT_WEATHER_STATION_HAS_TOUCHPAD 1
#endif

#ifndef IOT_WEATHER_STATION_TOUCHPAD_DEBUG
#    define IOT_WEATHER_STATION_TOUCHPAD_DEBUG 1
#endif

// IRQ pin
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

// if the BME680 is available, use it to display indoor temperature, humidity and pressure
#if !defined(IOT_SENSOR_USE_BME680_AS_INDOOR_SENSOR) || !IOT_SENSOR_HAVE_BME680
#    define IOT_SENSOR_USE_BME680_AS_INDOOR_SENSOR 0
#endif

// set to 1 to enable ILI9341 driver default is ST7735
#ifndef ILI9341_DRIVER
#    define ILI9341_DRIVER 0
#endif

#if ILI9341_DRIVER
#    define IOT_WEATHER_STATION_DRIVER "ILI9341"
#else
#    define IOT_WEATHER_STATION_DRIVER "ST7735"
#endif


#if IOT_WEATHER_STATION_HAS_TOUCHPAD
#    include "Mpr121Touchpad.h"
#endif

#if DEBUG_IOT_WEATHER_STATION
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

class WeatherStationBase : public WSDraw::Base
{
public:
    // in words:
    // poll weather information 60 seconds after booting and continue with configured interval
    // if an error occurs, retry after 60 seconds for 10 times, then retry every 15min.

    // delay after boot or if any error occurred
    static constexpr auto kPollDataErrorDelay = static_cast<uint32_t>(Event::seconds(60).count());
    // retries before pausing
    static constexpr uint16_t kPollDataRetries = 10;
    // pause for some time after too many errors
    static constexpr auto kPollDataErrorPauseDelay = static_cast<uint32_t>(Event::minutes(15).count());
    // min. delay between updates in case the configuration is invalid
    static constexpr auto kMinPollDataInterval = static_cast<uint32_t>(Event::minutes(5).count());

    static constexpr auto kNumScreens = WSDraw::kNumScreens;
    static constexpr auto kSkipScreen = WSDraw::kSkipScreen;
    static constexpr auto kManualScreen = WSDraw::kManualScreen;
    static constexpr uint8_t kDefaultScreenTimeInSeconds = 10;

    using HttpRequestCallback = KFCRestAPI::HttpRequest::Callback_t;
    using ScreenType = WSDraw::ScreenType;

public:
    WeatherStationBase();

    static void loop();
    static void wifiCallback(WiFiCallbacks::EventType event, void *payload);
    virtual void publishNow() = 0;
    // static void pollDataTimer(Event::CallbackTimerPtr timer);

    static WeatherStationBase &_getInstance();

public:
    void _loop();
    static void _pollDataTimerCallback(Event::CallbackTimerPtr timer);
    void _wifiCallback(WiFiCallbacks::EventType event, void *payload);
    void _pollDataUpdateLastTime(bool success);

    void _httpRequest(const String &url, int timeout, JsonBaseReader *jsonReader, HttpRequestCallback callback);
    void _getWeatherInfo(HttpRequestCallback callback);
    void _getWeatherForecast(HttpRequestCallback callback);
    void _openWeatherAPICallback(int16_t code, KFCRestAPI::HttpRequest &request);

    virtual void _fadeStatusLED() = 0;
    virtual void _rainbowStatusLED(bool stop = false) = 0;

    static void disableDisplay();
    static void enableDisplay();

protected:
    // timeout in seconds, -1 default, 0 no timeout
    void _setScreen(ScreenType screen, int16_t timeout = -1);
    void _setScreen(uint32_t screen, int16_t timeout = -1);
    void _draw();

protected:
    Event::Timer _pollDataTimer;
    uint32_t _pollDataLastMillis;
    uint16_t _pollDataRetries;

    uint16_t _backlightLevel;
    uint32_t _updateCounter;
    Event::Timer _fadeTimer;

    uint32_t _toggleScreenTimer;
    uint32_t _toggleScreenTimeout;
    uint32_t _lockCanvasUpdateEvents;

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        Mpr121Touchpad _touchpad;
        uint32_t _touchpadTimer{0};
        bool _touchpadDebug{false};
    #endif
};

inline void WeatherStationBase::disableDisplay()
{
    auto &base = WeatherStationBase::_getInstance();
    #if ESP8266
        if (base._canvas) {
            base.lock();
            delete base._canvas; //TODO has a memory leak
            base._canvas = nullptr;
            // WSDraw::ScrollCanvas::destroy(&base);
        }
    #elif ESP32
        base.lock();
    #endif
}

inline void WeatherStationBase::enableDisplay()
{
    auto &base = WeatherStationBase::_getInstance();
    #if ESP8266
        if (!base._canvas) {
            base._canvas = new WSDraw::CanvasType(base._tft.width(), base._tft.height());
            base.unlock();
            base.redraw();
        }
    #elif ESP32
        base.unlock();
        base.redraw();
    #endif
}

inline void WeatherStationBase::loop()
{
    _getInstance()._loop();
}

inline void WeatherStationBase::wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    _getInstance()._wifiCallback(event, payload);
}

inline void WeatherStationBase::_setScreen(uint32_t screen, int16_t timeout)
{
    _setScreen(static_cast<ScreenType>(screen), timeout);
}

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_disable.h>
#endif
