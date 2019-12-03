/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_WEATHER_STATION

#include <Arduino_compat.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <vector>
#include <EventScheduler.h>
#include <OpenWeatherMapAPI.h>
#include <asyncHTTPrequest.h>
#include <StreamString.h>
#include "plugins.h"

#ifndef DEBUG_IOT_WEATHER_STATION
#define DEBUG_IOT_WEATHER_STATION 0
#endif

#ifndef TFT_PIN_CS
#define TFT_PIN_CS              D4
#endif
#ifndef TFT_PIN_DC
#define TFT_PIN_DC              D3
#endif
#ifndef TFT_PIN_RST
#define TFT_PIN_RST             D0
#endif
#ifndef TFT_PIN_LED
#define TFT_PIN_LED             D8
#endif

#ifndef TFT_WIDTH
#define TFT_WIDTH               128
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT              160
#endif

class WeatherStationPlugin : public PluginComponent {
// PluginComponent
public:
    WeatherStationPlugin();

    virtual PGM_P getName() const;
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return MIN_PRIORITY;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;

    virtual bool hasStatus() const override;
    virtual const String getStatus() override;

    static void loop();
    static void serialHandler(uint8_t type, const uint8_t *buffer, size_t len);

private:
    typedef enum {
        LEFT,
        CENTER,
        RIGHT,
    } TextAlignEnum_t;

    typedef enum {
        TOP,
        MIDDLE,
        BOTTOM,
    } TextVAlignEnum_t;

    typedef struct {
        int16_t x;
        int16_t y;
        uint16_t w;
        uint16_t h;
    } Position_t;

    typedef struct {
        uint16_t w;
        uint16_t h;
    } Dimensions_t;

    typedef std::function<void (bool status)> Callback_t;

    void _httpRequest(const String &url, uint16_t timeout, JsonBaseReader *jsonReader, Callback_t finishedCallback);
    void _getWeatherInfo(Callback_t finishedCallback);

    void _serialHandler(const uint8_t *buffer, size_t len);
    void _loop();

    void _fadeBacklight(uint16_t fromLevel, uint16_t toLevel, int8_t step = 16);

    void _drawTextAligned(Adafruit_GFX &gfx, int16_t x, int16_t y, const String &text, TextAlignEnum_t align = LEFT, TextVAlignEnum_t valign = TOP, Position_t *pos = nullptr);
    void _drawTextAligned(Adafruit_GFX &gfx, int16_t x, int16_t y, const char *text, TextAlignEnum_t align = LEFT, TextVAlignEnum_t valign = TOP, Position_t *pos = nullptr);
    // draw bitmap with 2 bit color depth
    void _drawBitmap(Adafruit_GFX &gfx, int16_t x, int16_t y, PGM_P bmp, const uint16_t *palette, Dimensions_t *dim = nullptr);
    // wrapper for _tft._drawRGBBitmap()
    void _drawRGBBitmap(int16_t x, int16_t y, uint16_t *pcolors, int16_t w, int16_t h);

    void _drawTime();
    void _drawWeather();
    void _drawSunAndMoon();
    void _drawScreen0();
    void _draw();

private:
    Adafruit_ST7735 _tft;
    GFXcanvas16 *_canvas;
    uint8_t _currentScreen;

private:
    uint32_t _updateTimer;
    time_t _lastTime;
    bool _timeFormat24h;
    bool _isMetric;
    uint16_t _backlightLevel;

private:
    uint32_t _pollInterval;
    time_t _pollTimer;
    String _weatherError;
    OpenWeatherMapAPI _weatherApi;
    asyncHTTPrequest *_httpClient;

#if DEBUG_IOT_WEATHER_STATION
public:
    bool _debugDisplayCanvasBorder;
#endif
};

#endif
