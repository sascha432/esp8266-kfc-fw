/**
 * Author: sascha_lammers@gmx.de
 */

// #if IOT_WEATHER_STATION

#pragma once

#include <Arduino_compat.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#if ILI9341_DRIVER
#include <Adafruit_ILI9341.h>
#else
#include <Adafruit_ST7735.h>
#endif
#include <GFXCanvasCompressed.h>
#include <GFXCanvasCompressedPalette.h>
#include <OpenWeatherMapAPI.h>
#include <EventScheduler.h>
#include <stl_ext/fixed_circular_buffer.h>
#include "fonts/fonts.h"
#include "moon_phase.h"

#ifndef _MSC_VER
#include <kfc_fw_config.h>
#endif

#if _MSC_VER

namespace KFCConfigurationClasses {
    namespace Plugins {
        namespace WeatherStationConfigNS {
            class WeatherStationConfig {
                struct __attribute__packed__ Config_t {
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
                };
            };
        }
    }
}

#endif

#ifndef DISPLAY_PLUGIN_TFT_TYPE
#    define DISPLAY_PLUGIN_TFT_TYPE Adafruit_ST7735
#endif

#define WSDRAW_STATS 0

#ifndef TFT_PIN_CS
#    define TFT_PIN_CS -1
#endif
#ifndef TFT_PIN_DC
#    define TFT_PIN_DC 5
#endif
#ifndef TFT_PIN_RST
#    define TFT_PIN_RST 4
#endif
#ifndef TFT_PIN_LED
#    define TFT_PIN_LED 16
#endif

#ifndef TFT_WIDTH
#    define TFT_WIDTH 128
#endif
#ifndef TFT_HEIGHT
#    define TFT_HEIGHT 160
#endif

#if TFT_WIDTH == 128 && TFT_HEIGHT == 160
#    include "./themes/theme1_160x240.h"
#elif TFT_WIDTH == 240 && TFT_HEIGHT == 320
#    include "./themes/theme2_240x320.h"
#else
#    error No theme available for TFT dimensions
#endif

// // using WeatherStationCanvas = GFXCanvasCompressedPalette;
// // using Canvas = WeatherStationCanvas;
// // using WeatherStationCanvas = GFXCanvasCompressed;

// namespace WeatherStation {
//     namespace Screen {
//         class BaseScreen;
//     }
// }

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

namespace WSDraw {

    using DisplayType = GFXExtension<DISPLAY_PLUGIN_TFT_TYPE>;
    using CanvasType = GFXCanvasCompressedPalette;
    using ConfigType = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStationConfig::Config_t;
    using WSConfigType = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation;
    // using BaseScreen = WeatherStation::Screen::BaseScreen;

    class ScrollCanvas;

    enum class ScreenType : uint8_t {
        MAIN = 0,
        INDOOR,
        FORECAST,
        NUM_SCREENS = FORECAST, //forecast=disabled
        TEXT_CLEAR,
        TEXT_UPDATE,
        TEXT,
    };

    static constexpr auto kNumScreens = static_cast<uint8_t>(ScreenType::NUM_SCREENS);

    class Base {
    public:
        // friend BaseScreen;
        friend ScrollCanvas;

        Base();
        virtual ~Base();

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
        CanvasType *getCanvasAndLock();
        void releaseCanvasLock();

        //// can be called while the canvas is detached
        //// without a canvas, it is using the TFT directly and clear is forced true
        void drawText(const String &text, const GFXfont *font, uint16_t color, bool clear = false);

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

        DisplayType &getDisplay();
        CanvasType *getCanvas();

    public:
        void _initScreen() {
            if (isCanvasAttached()) {
                redraw();
            }
        }

        void setText(const String &text, const GFXfont *textFont) {
           _text = text;
           _textFont = textFont;
        }

    protected:
        DisplayType _tft;
        CanvasType *_canvas;
        uint16_t _canvasLocked;

    public:
        void _drawTime();
        void _updateTime();

        void _drawWeather();
        void _drawWeather(GFXCanvasCompressed *canvas, int16_t top);
        void _drawWeatherIndoor(GFXCanvasCompressed *canvas, int16_t top);
        void _updateWeatherIndoor();
        void _drawIndoor();
        void _drawIndoor(GFXCanvasCompressed *canvas, int16_t top);
        void _drawSunAndMoon();

        void _drawScreenMain();

        void _drawScreenIndoor();
        void _updateScreenIndoor();

        void _drawScreenForecast();

        void _doScroll();
        void _scrollTimer(Base &draw);
        bool _isScrolling() const;

        void _displayMessage(const String &title, const String &message, uint16_t titleColor, uint16_t messageColor, uint32_t timeout);

        void _drawText(const String &text, const GFXfont *font, uint16_t color, bool clear = false);
        void _draw();

        void _displayScreen(int16_t x, int16_t y, int16_t w, int16_t h);

    public:

        static const __FlashStringHelper *getScreenName(ScreenType screen);
        static const __FlashStringHelper *getScreenName(uint8_t screen) {
            return getScreenName(static_cast<ScreenType>(screen));
        }

    public:
        // friend WeatherStation::Screen::BaseScreen;
        ConfigType &getConfig() {
            return _config;
        }

        // void setScreen(BaseScreen *screen);

        // BaseScreen *_screen;
        // Event::Timer _screenTimer;

    protected:
        String _getTemperature(float value, bool kelvin = false);
        virtual void _getIndoorValues(float *data);

        ScrollCanvas *_scrollCanvas;
        OpenWeatherMapAPI _weatherApi;
        ConfigType _config;
        Event::Timer _displayMessageTimer;
        String _text;
        const GFXfont *_textFont;

        time_t _lastTime;
        uint32_t _screenLastUpdateTime;
        uint16_t _offsetX;
        int16_t _offsetY;
        uint8_t _scrollPosition;
        ScreenType _currentScreen;
        bool _redrawFlag;

    #if WSDRAW_STATS
    public:
        void _statsBegin();
        void _statsEnd(const __FlashStringHelper *name);
        MicrosTimer _statsTimer;

        bool _debug_stats;
        using StatsBuffer = FixedCircularBuffer<float, 10>;
        std::map<String, StatsBuffer> _stats;
    #endif
    };

    class ScrollCanvas {
    public:
        ScrollCanvas(Base &draw, uint16_t width, uint16_t height) :
            _draw(draw),
            _canvas(width, height)
        {
            if (_draw._scrollCanvas) {
                __DBG_panic("scroll canvas set ptr=%p this=%p", _draw._scrollCanvas, this);
            }
            _draw._scrollCanvas = this;
        }
        ~ScrollCanvas() {
            _timer.remove();
            _draw._scrollCanvas = nullptr;
        }

        CanvasType &getCanvas() {
            return _canvas;
        }

        static void create(Base *draw, uint16_t width, int16_t height) {
            destroy(draw);
            draw->_scrollCanvas = new ScrollCanvas(*draw, width, height);
        }

        static void destroy(Base *draw) {
            if (draw->_scrollCanvas) {
                delete draw->_scrollCanvas;
                draw->_scrollCanvas = nullptr;
            }
        }

    private:
        Base &_draw;
        CanvasType _canvas;
        Event::Timer _timer;
    };

}

// #endif
