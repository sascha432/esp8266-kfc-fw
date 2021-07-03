/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION

#pragma once

#include <Arduino_compat.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <GFXCanvasCompressed.h>
#include <GFXCanvasCompressedPalette.h>
#include <OpenWeatherMapAPI.h>
#include <EventScheduler.h>
#include "FixedCircularBuffer.h"
#include "fonts/fonts.h"
#include "moon_phase.h"

#ifndef _MSC_VER
#include <kfc_fw_config.h>
#endif

#if _MSC_VER

namespace WeatherStationConfig {

    typedef struct __attribute__packed__ Config_t {
        using Type = Config_t;

        uint8_t weather_poll_interval;
        uint16_t api_timeout;
        uint8_t backlight_level;
        uint8_t touch_threshold;
        uint8_t released_threshold;
        CREATE_UINT8_BITFIELD(is_metric, 1);
        CREATE_UINT8_BITFIELD(time_format_24h, 1);
        CREATE_UINT8_BITFIELD(show_webui, 1);
        float temp_offset;
        float humidity_offset;
        float pressure_offset;
        uint8_t screenTimer[8];

        Config_t() : weather_poll_interval(15),
            api_timeout(30),
            backlight_level(100),
            touch_threshold(5),
            released_threshold(8),
            is_metric(true),
            time_format_24h(true),
            show_webui(false),
            temp_offset(0),
            humidity_offset(0),
            pressure_offset(0),
            screenTimer{ 10, 10 }
        {
        }
    } Config_t;

}

#endif

#define WSDRAW_STATS            0

#ifndef TFT_PIN_CS
#define TFT_PIN_CS              -1
#endif
#ifndef TFT_PIN_DC
#define TFT_PIN_DC              5
#endif
#ifndef TFT_PIN_RST
#define TFT_PIN_RST             4
#endif
#ifndef TFT_PIN_LED
#define TFT_PIN_LED             16
#endif

#ifndef TFT_WIDTH
#define TFT_WIDTH               128
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT              160
#endif

#if TFT_WIDTH == 128 && TFT_HEIGHT == 160

#define COLORS_BACKGROUND               ST77XX_BLACK
#define COLORS_DEFAULT_TEXT             ST77XX_WHITE
#define COLORS_DATE                     ST77XX_WHITE
#define COLORS_TIME                     ST77XX_WHITE
#define COLORS_TIMEZONE                 ST77XX_YELLOW
#define COLORS_CITY                     ST77XX_CYAN
#define COLORS_CURRENT_WEATHER          ST77XX_YELLOW
#define COLORS_TEMPERATURE              ST77XX_WHITE
#define COLORS_WEATHER_DESCR            ST77XX_YELLOW
#define COLORS_SUN_AND_MOON             ST77XX_YELLOW
#define COLORS_SUN_RISE_SET             ST77XX_WHITE
#define COLORS_MOON_PHASE               ST77XX_WHITE

#define FONTS_DEFAULT_SMALL             &DejaVuSans_5pt8b
#define FONTS_DEFAULT_MEDIUM            &Dialog_Bold_7pt8b
#define FONTS_DEFAULT_BIG               &DejaVuSans_Bold_10pt8b
#define FONTS_DATE                      &DejaVuSans_Bold_5pt8b
#define FONTS_TIME                      &DejaVuSans_Bold_10pt8b
#define FONTS_TIMEZONE                  &DejaVuSans_5pt8b
#define FONTS_TEMPERATURE               &DejaVuSans_Bold_10pt8b
#define FONTS_CITY                      &Dialog_6pt8b
#define FONTS_WEATHER_DESCR             &Dialog_6pt8b
#define FONTS_SUN_AND_MOON              &DejaVuSans_5pt8b
#define FONTS_MOON_PHASE                &moon_phases14pt7b
#define FONTS_MOON_PHASE_UPPERCASE      true
#define FONTS_MESSAGE_TITLE             &DejaVuSans_Bold_10pt8b
#define FONTS_MESSAGE_TEXT              &Dialog_Bold_7pt8b

// time

#define X_POSITION_DATE                 (TFT_WIDTH / 2) + _offsetX
#define Y_POSITION_DATE                 5 + _offsetY
#define H_POSITION_DATE                 AdafruitGFXExtension::CENTER

#define X_POSITION_TIME                 (TFT_WIDTH / 2) + _offsetX
#define Y_POSITION_TIME                 17 + _offsetY
#define H_POSITION_TIME                 AdafruitGFXExtension::CENTER

#define X_POSITION_TIMEZONE             (TFT_WIDTH / 2) + _offsetX
#define Y_POSITION_TIMEZONE             35 + _offsetY
#define H_POSITION_TIMEZONE             AdafruitGFXExtension::CENTER

// weather

#define X_POSITION_WEATHER_ICON         (2 + _offsetX)
#define Y_POSITION_WEATHER_ICON         (0 + _offsetY)

#define X_POSITION_CITY                 (TFT_WIDTH - 2 + _offsetX)
#define Y_POSITION_CITY                 (3 + _offsetY)
#define H_POSITION_CITY                 AdafruitGFXExtension::RIGHT

#define X_POSITION_TEMPERATURE          (X_POSITION_CITY - 2)
#define Y_POSITION_TEMPERATURE          (17 + _offsetY)
#define H_POSITION_TEMPERATURE          AdafruitGFXExtension::RIGHT

#define X_POSITION_WEATHER_DESCR        X_POSITION_CITY
#define Y_POSITION_WEATHER_DESCR        (35 + _offsetY)
#define H_POSITION_WEATHER_DESCR        AdafruitGFXExtension::RIGHT

// sun and moon

#define X_POSITION_SUN_TITLE            2 + _offsetX
#define Y_POSITION_SUN_TITLE            0 + _offsetY
#define H_POSITION_SUN_TITLE            AdafruitGFXExtension::LEFT

#define X_POSITION_MOON_PHASE_NAME      (TFT_WIDTH - X_POSITION_SUN_TITLE)
#define Y_POSITION_MOON_PHASE_NAME      Y_POSITION_SUN_TITLE
#define H_POSITION_MOON_PHASE_NAME      AdafruitGFXExtension::RIGHT

#define X_POSITION_MOON_PHASE_DAYS      X_POSITION_MOON_PHASE_NAME
#define Y_POSITION_MOON_PHASE_DAYS      (12 + _offsetY)
#define H_POSITION_MOON_PHASE_DAYS      H_POSITION_MOON_PHASE_NAME

#define X_POSITION_SUN_RISE_ICON        (4 + _offsetX)
#define Y_POSITION_SUN_RISE_ICON        (10 + _offsetY)

#define X_POSITION_SUN_SET_ICON         X_POSITION_SUN_RISE_ICON
#define Y_POSITION_SUN_SET_ICON         (21 + _offsetY)

#define X_POSITION_SUN_RISE             (TFT_WIDTH / 3 + 2 + _offsetX)
#define Y_POSITION_SUN_RISE             (12 + _offsetY)
#define H_POSITION_SUN_RISE             AdafruitGFXExtension::RIGHT

#define X_POSITION_SUN_SET              X_POSITION_SUN_RISE
#define Y_POSITION_SUN_SET              (23 + _offsetY)
#define H_POSITION_SUN_SET              H_POSITION_SUN_RISE

#define X_POSITION_MOON_PHASE           (TFT_WIDTH / 2 + _offsetX)
#define Y_POSITION_MOON_PHASE           (12 + _offsetY)
#define H_POSITION_MOON_PHASE           AdafruitGFXExtension::LEFT


#define Y_START_POSITION_TIME           (0)
#define Y_END_POSITION_TIME             (45 + Y_START_POSITION_TIME)

#define Y_START_POSITION_WEATHER        (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_WEATHER          (70 + Y_START_POSITION_WEATHER)

#define Y_START_POSITION_INDOOR         (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_INDOOR           (70 + Y_START_POSITION_WEATHER)

#define Y_START_POSITION_SUN_MOON       (Y_END_POSITION_WEATHER + 2)
#define Y_END_POSITION_SUN_MOON         (TFT_HEIGHT - 1)

#define FONTS_WEATHER_INDOOR                            &DejaVuSans_5pt8b
#define X_POSITION_WEATHER_INDOOR_TEMP                  (2 + _offsetX)
#define X_POSITION_WEATHER_INDOOR_HUMIDITY              (TFT_WIDTH / 2 - 5)
#define X_POSITION_WEATHER_INDOOR_PRESSURE              (128 - 2 + _offsetX)
#define Y_POSITION_WEATHER_INDOOR_TEMP                  (60 + _offsetY)
#define Y_POSITION_WEATHER_INDDOR_HUMIDITY              (60 + _offsetY)
#define Y_POSITION_WEATHER_INDDOR_PRESSURE              (60 + _offsetY)

#else

#error No theme available for TFT dimensions

#endif

using WeatherStationCanvas = GFXCanvasCompressedPalette;
using Canvas = WeatherStationCanvas;
// using WeatherStationCanvas = GFXCanvasCompressed;

using Adafruit_ST7735_Ex = GFXExtension<Adafruit_ST7735>;

namespace WeatherStation {
    namespace Screen {
        class BaseScreen;
    }
}

#if 0

class CanvasObject {
public:
    CanvasObject() {}
    virtual void draw(Canvas *canvas, int x, int y) = 0;

    int width() const{
        return TFT_WIDTH;
    }
    int height() const{
        return TFT_HEIGHT;
    }
};

class TextObject : public CanvasObject
{
public:
    TextObject(const String &text, const GFXfont *font, ColorType color) : _text(text), _font(font), _color(color) {}

    virtual void draw(Canvas *canvas, int x, int y) {
        canvas->fillScreen(COLORS_BACKGROUND);
        canvas->setTextColor(_color);
        canvas->setFont(_font);
        GFXCanvasCompressed::Position_t pos;
        canvas->drawTextAligned(width() / 2, height() / 2, _text, GFXCanvasCompressed::CENTER, GFXCanvasCompressed::MIDDLE, &pos);
        _displayScreen(0, 0, width(), height());
    }

private:
    String _text;
    const GFXfont *_font;
    ColorType _color;
};
#endif

class WSDraw {
public:
    WSDraw();
    virtual ~WSDraw();

public:
    // safely redraw in next main loop
    void redraw();

    // reattach canvas object
    bool _attachCanvas();
    // detach canvas object and free memory
    // calling it multiple times will lock the object until attachCanvas has been called the same amount of times
    bool _detachCanvas(bool release = true);

    // check if canvas is attached
    bool isCanvasAttached() const;
    WeatherStationCanvas *getCanvasAndLock();
    void releaseCanvasLock();

    //// can be called while the canvas is detached
    //// without a canvas, it is using the TFT directly and clear is forced true
    //void drawText(const String &text, const GFXfont *font, uint16_t color, bool clear = false);

    //void displayMessage(const String &title, const String &message, uint16_t titleColor, uint16_t messageColor, uint32_t timeout) {
    //    if (isCanvasAttached()) {
    //        _displayMessage(title, message, ST77XX_YELLOW, ST77XX_WHITE, timeout);
    //    }
    //    else {
    //        // this->callOnce([&]() {
    //        //     _displayMessage(title, message, ST77XX_YELLOW, ST77XX_WHITE, timeout);
    //        // });
    //    }
    //}

public:
    // called for partial or full updates of the screen
    virtual void canvasUpdatedEvent(int16_t x, int16_t y, int16_t w, int16_t h);

    Adafruit_ST7735_Ex& getST7735();
    WeatherStationCanvas *getCanvas();

public:
    void _initScreen() {
        if (isCanvasAttached()) {
            redraw();
        }
    }

    //void setText(const String &text, const GFXfont *textFont) {
    //    _text = text;
    //    _textFont = textFont;
    //}

protected:
    Adafruit_ST7735_Ex _tft;
    WeatherStationCanvas *_canvas;
    uint16_t _canvasLocked;

public:
    void _drawTime();
    //void _drawTime();
    //void _updateTime();

    //void _drawWeather();
    //void _drawWeather(GFXCanvasCompressed *canvas, int16_t top);
    //void _drawWeatherIndoor(GFXCanvasCompressed *canvas, int16_t top);
    //void _updateWeatherIndoor();
    //void _drawIndoor();
    //void _drawIndoor(GFXCanvasCompressed *canvas, int16_t top);
    //void _drawSunAndMoon();

    //void _drawScreenMain();

    //void _drawScreenIndoor();
    //void _updateScreenIndoor();

    //void _drawScreenForecast();

    //void _doScroll();
    //void _scrollTimer(WSDraw &draw);
    //bool _isScrolling() const;

    //void _displayMessage(const String &title, const String &message, uint16_t titleColor, uint16_t messageColor, uint32_t timeout);

    //void _drawText(const String &text, const GFXfont *font, uint16_t color, bool clear = false);
    //void _draw();

    void _displayScreen(int16_t x, int16_t y, int16_t w, int16_t h);

public:
    //class ScrollCanvas {
    //public:
    //    ScrollCanvas(WSDraw &draw, uint16_t width, uint16_t height) : _draw(draw), _canvas(width, height)  {
    //        if (_draw._scrollCanvas) {
    //            __DBG_panic("scroll canvas set ptr=%p this=%p", _draw._scrollCanvas, this);
    //        }
    //        _draw._scrollCanvas = this;
    //    }
    //    ~ScrollCanvas() {
    //        _timer.remove();
    //        _draw._scrollCanvas = nullptr;
    //    }

    //    WeatherStationCanvas &getCanvas() {
    //        return _canvas;
    //    }

    //    static void create(WSDraw *draw, uint16_t width, int16_t height) {
    //        destroy(draw);
    //        draw->_scrollCanvas = __DBG_new(ScrollCanvas, *draw, width, height);
    //    }

    //    static void destroy(WSDraw *draw) {
    //        if (draw->_scrollCanvas) {
    //            __DBG_delete(draw->_scrollCanvas);
    //            draw->_scrollCanvas = nullptr;
    //        }
    //    }

    //private:
    //    WSDraw &_draw;
    //    WeatherStationCanvas _canvas;
    //    Event::Timer _timer;
    //};

#if WSDRAW_STATS
private:
    void _statsBegin();
    void _statsEnd(const __FlashStringHelper *name);
    MicrosTimer _statsTimer;
#endif
public:
    typedef enum {
        MAIN = 0,
        INDOOR,
        FORECAST,
        NUM_SCREENS = FORECAST, //forecast=disabled
        TEXT_CLEAR,
        TEXT_UPDATE,
        TEXT,
    } ScreenEnum_t;

    static const __FlashStringHelper *getScreenName(uint8_t screen) {
        switch(screen) {
            case MAIN:
                return F("Current Weather, Sun and Moon");
            case INDOOR:
                return F("Indoor Climate");
            case FORECAST:
                return F("Weather Forecast");
        }
        return F("Unknown");
    }

public:
#ifdef _MSC_VER
    using WeatherStationConfigType = WeatherStationConfig::Config_t;
#else
    using WeatherStationConfigType = KFCConfigurationClasses::Plugins::WeatherStation::ConfigStructType;
#endif
    using BaseScreen = WeatherStation::Screen::BaseScreen;
    // friend WeatherStation::Screen::BaseScreen;
    friend BaseScreen;

    WeatherStationConfigType &getConfig() {
        return _config;
    }

    void setScreen(BaseScreen *screen);

    BaseScreen *_screen;
    Event::Timer _screenTimer;
    uint32_t _screenLastUpdateTime;

protected:

    String _getTemperature(float value, bool kelvin = false/*celsius = true*/);
    virtual void _getIndoorValues(float *data);

    //ScrollCanvas *_scrollCanvas;
    //uint8_t _scrollPosition;
    OpenWeatherMapAPI _weatherApi;

    bool _redrawFlag: 1;
    // uint8_t _currentScreen;
    // String _text;
    // const GFXfont *_textFont;

    // time_t _lastTime;
    WeatherStationConfigType _config;

    // uint16_t _offsetX;
    // int16_t _offsetY;

    // Event::Timer _displayMessageTimer;

#if WSDRAW_STATS
    bool _debug_stats;
    using StatsBuffer = FixedCircularBuffer<float, 10>;
    std::map<String, StatsBuffer> _stats;
#endif
};

#endif
