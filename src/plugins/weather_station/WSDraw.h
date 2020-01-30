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
#include <SpeedBooster.h>
#include <Timezone.h>
#if _WIN32
class EventTimer {
public:
    void detach() {}
};
class EventScheduler {
public:
    typedef EventTimer *TimerPtr;
    class Timer {
    public:
        Timer() {

        }
        void remove() {
        }
    };
};
class Config_WeatherStation {
public:
    typedef struct ____attribute__packed__ {
        uint8_t is_metric: 1;
        uint8_t time_format_24h: 1;
        uint16_t weather_poll_interval;
        uint16_t api_timeout;
        uint8_t backlight_level;
        uint8_t touch_threshold;
        uint8_t released_threshold;
        float temp_offset;
    } WeatherStationConfig_t;
    Config_WeatherStation() {
        config = {1,1,0,30,255,5,8,0};
    }
};
#else
#include <EventScheduler.h>
#include <EventTimer.h>
#include "kfc_fw_config.h"
#endif
#include "moon_phase.h"

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

#define HAVE_DEGREE_SYMBOL              0
#define DEGREE_STR                      " "

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

#define FONTS_DEFAULT_SMALL             &DejaVu_Sans_10
#define FONTS_DEFAULT_MEDIUM            &Dialog_bold_10
#define FONTS_DEFAULT_BIG               &DejaVu_Sans_Bold_20
#define FONTS_DATE                      &Dialog_bold_10
#define FONTS_TIME                      &DejaVu_Sans_Bold_20
#define FONTS_TIMEZONE                  &DejaVu_Sans_10
#define FONTS_TEMPERATURE               &DejaVu_Sans_Bold_20
#define FONTS_CITY                      &DialogInput_plain_9
#define FONTS_WEATHER_DESCR             &DialogInput_plain_9
#define FONTS_SUN_AND_MOON              &DejaVu_Sans_10
#define FONTS_MOON_PHASE                &moon_phases14pt7b
#define FONTS_MOON_PHASE_UPPERCASE      true

extern const GFXfont DejaVu_Sans_Bold_20 PROGMEM;
extern const GFXfont DejaVu_Sans_Bold_12 PROGMEM;
extern const GFXfont DejaVu_Sans_Bold_11 PROGMEM;
extern const GFXfont DejaVu_Sans_10 PROGMEM;
extern const GFXfont Dialog_bold_10 PROGMEM;
extern const GFXfont DialogInput_plain_9 PROGMEM;

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

#define X_POSITION_TEMPERATURE          X_POSITION_CITY
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

#define Y_START_POSITION_SUN_MOON       (Y_END_POSITION_WEATHER + 2)
#define Y_END_POSITION_SUN_MOON         (TFT_HEIGHT - 1)

#else

#error No theme available for TFT dimensions

#endif

class WSDraw {
public:
    WSDraw();
    virtual ~WSDraw();

    void _drawTime();
    void _drawWeather();
    void _drawWeather(GFXCanvasCompressed &canvas, uint8_t top);
    void _drawSunAndMoon();
    void _drawScreen0();

    void _doScroll();
    void _scrollTimer(WSDraw &draw);
    bool _isScrolling() const;

    void _updateTime();
    void _drawText(const String &text, const GFXfont *font, uint16_t color, bool clear = false);
    void _draw();

    void _displayScreen(int16_t x, int16_t y, int16_t w, int16_t h);
    virtual void _broadcastCanvas(int16_t x, int16_t y, int16_t w, int16_t h);

    Adafruit_ST7735& getST7735() {
        return _tft;
    }

    GFXCanvasCompressed& getCanvas() {
        return _canvas;
    }

public:
    class ScrollCanvas {
    public:
        ScrollCanvas(WSDraw &draw, uint16_t width, uint16_t height) : _draw(draw), _canvas(width, height)  {
            _draw._scrollCanvas = this;
        }
        ~ScrollCanvas() {
            _timer.remove();
            _draw._scrollCanvas = nullptr;
        }

        GFXCanvasCompressedPalette &getCanvas() {
            return _canvas;
        }

        static void create(WSDraw *draw, uint16_t width, uint16_t height) {
            destroy(draw);
            draw->_scrollCanvas = new ScrollCanvas(*draw, width, height);
        }

        static void destroy(WSDraw *draw) {
            if (draw->_scrollCanvas) {
                delete draw->_scrollCanvas;
            }
        }

    private:
        WSDraw &_draw;
        GFXCanvasCompressedPalette _canvas;
        EventScheduler::Timer _timer;
    };

protected:
    typedef enum {
        MAIN = 0,
        NUM_SCREENS,
        TEXT_CLEAR,
        TEXT,
    } ScreenEnum_t;

    void setText(const String &text, const GFXfont *textFont) {
        _text = text;
        _textFont = textFont;
    }

    Adafruit_ST7735 _tft;
    GFXCanvasCompressedPalette  _canvas;
    ScrollCanvas *_scrollCanvas;
    uint8_t _scrollPosition;
    OpenWeatherMapAPI _weatherApi;
    String _weatherError;

    uint8_t _currentScreen;
    String _text;
    const GFXfont *_textFont;

    time_t _lastTime;
    Config_WeatherStation::WeatherStationConfig_t _config;

    uint8_t _offsetX;
    uint8_t _offsetY;
};

#endif
