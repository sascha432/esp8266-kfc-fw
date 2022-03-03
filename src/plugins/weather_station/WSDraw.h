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
                    uint8_t screenTimer[8];
                    Config_t() : weather_poll_interval(15),
                        api_timeout(30),
                        backlight_level(100),
                        touch_threshold(5),
                        released_threshold(8),
                        is_metric(true),
                        time_format_24h(true),
                        show_webui(false),
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

// execute block between clear and display helper
#define CLEAR_AND_DISPLAY(minY, maxY) \
    for(bool init = _clearPartially(minY, maxY, COLORS_BACKGROUND); \
        init; \
        init = _displayScreenFalse(0, minY, TFT_WIDTH, ((maxY) - (minY))))

namespace WSDraw {

    using DisplayType = GFXExtension<DISPLAY_PLUGIN_TFT_TYPE>;
    #if ESP32
        using CanvasType = GFXCanvasCompressed;
    #else
        using CanvasType = GFXCanvasCompressedPalette;
    #endif
    using ConfigType = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStationConfig::Config_t;
    using WSConfigType = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation;
    using ScreenType = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::ScreenType;

    class ScrollCanvas;

    static constexpr auto kNumScreens = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStationConfig::Config_t::kNumScreens;
    static constexpr auto kSkipScreen = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStationConfig::Config_t::kSkipScreen;

    static constexpr int16_t _offsetX = 2;

    class Base {
    public:
        // friend BaseScreen;
        friend ScrollCanvas;

        Base();
        virtual ~Base();

    public:
        // safely redraw in next main loop
        void redraw();

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
        virtual void canvasUpdatedEvent(int16_t x, int16_t y, int16_t w, int16_t h) = 0;

        DisplayType &getDisplay();
        CanvasType *getCanvas();

    public:
        void setText(const String &text, const GFXfont *textFont);

        bool lock();
        void unlock();
        bool isLocked();

        void printDebugInfo(Print &output);

    protected:
        DisplayType _tft;
        CanvasType *_canvas;

    public:
        // update date, time and timezone at the top
        void _drawTime();

        // display local weather info
        void _drawLocalWeather();

        // draw temperature, humidity and pressure in one line
        void _drawIndoorClimate();

        // displays indoor as main screen
        void _drawIndoorClimateBottom();

        // calls _drawIndoorClimate() after clearing the part of the screen and then displays it
        void _updateIndoorClimateBottom();

        // draws sun and moon info
        void _drawSunAndMoon();

        // draw weather forcast
        void _drawForecast();

        // draw info
        void _drawInfo();

        // drawScreen does not clear or display the changes. this is usually used in _draw() where
        // the entire display is cleared and displayed
        // updateScreen will only clear and display the content

        void _drawScreenMain();
        void _updateScreenMain();

        void _drawScreenIndoorClimate();
        void _updateScreenIndoorClimate();

        void _drawScreenForecast();
        void _updateScreenForecast();

        void _drawScreenInfo();
        void _updateScreenInfo();

        #if DEBUG
            void _drawDebugInfo();
            void _drawScreenDebug();
            void _updateScreenDebug();

            enum class DebugMode : uint8_t {
                ICON,
                LEFT,
                RIGHT
            };

            const char  *_icon{nullptr};
            int16_t _pos{0};
            DebugMode _mode{DebugMode::ICON};
        #endif

        void _updateScreenTime();

        void _doScroll();
        void _scrollTimer(Base &draw);
        bool _isScrolling() const;

        void _displayMessage(const String &title, const String &message, uint16_t titleColor, uint16_t messageColor, uint32_t timeout);

        void _drawText(const String &text, const GFXfont *font, uint16_t color, bool clear = false);

        // clears display, calls the custom screen method and displays the canvas
        void _draw();

        // sends the canvas area to the screen
        void _displayScreen(int16_t x, int16_t y, int16_t w, int16_t h);

        inline bool _displayScreenFalse(int16_t x, int16_t y, int16_t w, int16_t h) {
            _displayScreen(x, y, w, h);
            return false;
        }

        // clear/fill partial screen
        bool _clearPartially(int16_t y, int16_t endY, ColorType color = COLORS_BACKGROUND);

    public:
        ScreenType _getScreen(ScreenType screen, bool allowZeroTimeout = false) const;
        ScreenType _getScreen(uint32_t screen, bool allowZeroTimeout = false) const;
        ScreenType _getNextScreen(ScreenType screen, bool allowZeroTimeout = false) const;
        ScreenType _getNextScreen(uint32_t screen, bool allowZeroTimeout = false) const;
        ScreenType _getPrevScreen(ScreenType screen, bool allowZeroTimeout = false) const;
        ScreenType _getPrevScreen(uint32_t screen, bool allowZeroTimeout = false) const;
        uint8_t _getCurrentScreen() const;
        bool _isScreenValid(ScreenType screen, bool allowZeroTimeout = false) const;
        bool _isScreenValid(uint8_t screen, bool allowZeroTimeout = false) const;

        static const __FlashStringHelper *getScreenName(ScreenType screen);
        static const __FlashStringHelper *getScreenName(uint8_t screen);

        ConfigType &getConfig();

    protected:
        struct IndoorValues {

            static constexpr float kFactor = 100; // fixed point factor for temperature and humidity

            IndoorValues(float temperature, float humidity, float pressure) :
                _temperature(temperature * kFactor),
                _humidity(humidity * kFactor),
                _pressure(pressure)
            {
            }

            void setTemperature(float temperature) {
                _temperature = temperature * kFactor;
            }
            void setHumidity(float humidity) {
                _humidity = humidity * kFactor;
            }
            void setPressure(float pressure) {
                _pressure = pressure;
            }

            // °C
            float getTemperature() const {
                return _temperature / kFactor;
            }
            // %
            float getHumidity() const {
                return _humidity / kFactor;
            }
            // hPa
            float getPressure() const {
                return _pressure;
            }

        private:
            int16_t _temperature;
            uint16_t _humidity;
            float _pressure;
        };

        // get temperature as string (°C or °F), depending on the user setting
        // if kelvin is true, value be treated as K instead C
        String _getTemperature(float value, bool kelvin = false);

        // read indoor values
        virtual IndoorValues _getIndoorValues() = 0;

       #if DEBUG
            uint32_t _debugLastUpdate{0};
            float _debugFPS{0.0f};
            float _debugPPS{0.0f};
            float _debugDrawTime{0.0f};
        #endif

        ScrollCanvas *_scrollCanvas;
        OpenWeatherMapAPI _weatherApi;
        ConfigType _config;
        Event::Timer _displayMessageTimer;
        String _text;
        const GFXfont *_textFont;
        SemaphoreMutex _lock;
        time_t _lastTime;
        uint8_t _scrollPosition;
        ScreenType _currentScreen;
        volatile bool _redrawFlag;
        volatile bool _locked;
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

    inline void Base::redraw()
    {
        MUTEX_LOCK_BLOCK(_lock) {
            _redrawFlag = true;
        }
    }

    inline bool Base::lock()
    {
        MUTEX_LOCK_BLOCK(_lock) {
            if (_locked) {
                return false;
            }
            _locked = true;
        }
        return true;
    }

    inline void Base::unlock()
    {
        MUTEX_LOCK_BLOCK(_lock) {
            _locked = false;
        }
    }

    inline bool Base::isLocked()
    {
        MUTEX_LOCK_BLOCK(_lock) {
            return _locked;
        }
        __builtin_unreachable();
    }

    inline ConfigType &Base::getConfig()
    {
        return _config;
    }

    inline const __FlashStringHelper *Base::getScreenName(uint8_t screen)
    {
        return getScreenName(static_cast<ScreenType>(screen));
    }

    inline bool Base::_clearPartially(int16_t minY, int16_t maxY, ColorType color)
    {
        _canvas->fillScreenPartial(minY, maxY - minY, color);
        return true;
    }

    inline void Base::setText(const String &text, const GFXfont *textFont)
    {
        _text = text;
        _textFont = textFont;
    }

    inline DisplayType &Base::getDisplay()
    {
        return _tft;
    }

    inline CanvasType *Base::getCanvas()
    {
        return _canvas;
    }

    inline uint8_t Base::_getCurrentScreen() const
    {
        return static_cast<uint8_t>(_currentScreen);
    }

    inline bool Base::_isScreenValid(ScreenType screen, bool allowZeroTimeout) const
    {
        return _isScreenValid(static_cast<uint8_t>(screen), allowZeroTimeout);
    }

    inline bool Base::_isScreenValid(uint8_t screen, bool allowZeroTimeout) const
    {
        if (screen >= kNumScreens) {
            return false;
        }
        if (_config.screenTimer[screen] == kSkipScreen) {
            return false;
        }
        else if (_config.screenTimer[screen] == 0) {
            if (!allowZeroTimeout) {
                return false;
            }
        }
        return true;
    }

    inline ScreenType Base::_getScreen(uint32 screen, bool allowZeroTimeout) const
    {
        return _getScreen(static_cast<ScreenType>(screen), allowZeroTimeout);
    }

    inline ScreenType Base::_getNextScreen(uint32_t screen, bool allowZeroTimeout) const
    {
        return _getNextScreen(static_cast<ScreenType>(screen), allowZeroTimeout);
    }

    inline ScreenType Base::_getPrevScreen(uint32_t screen, bool allowZeroTimeout) const
    {
        return _getPrevScreen(static_cast<ScreenType>(screen), allowZeroTimeout);
    }

    inline ScreenType Base::_getScreen(ScreenType screen, bool allowZeroTimeout) const
    {
        return _isScreenValid(screen, allowZeroTimeout) ? screen : _getNextScreen(screen, allowZeroTimeout);
    }

    inline ScreenType Base::_getNextScreen(ScreenType screen, bool allowZeroTimeout) const
    {
        for(uint8_t i = static_cast<uint8_t>(screen) + 1; i < static_cast<uint8_t>(screen) + kNumScreens; i++) {
            uint8_t n = i % kNumScreens;
            if (_isScreenValid(n, allowZeroTimeout)) {
                return static_cast<ScreenType>(n);
            }
        }
        return screen;
    }

    inline ScreenType Base::_getPrevScreen(ScreenType screen, bool allowZeroTimeout) const
    {
        for(uint8_t i = static_cast<uint8_t>(screen) + kNumScreens - 1; i > static_cast<uint8_t>(screen); i--) {
            uint8_t n = i % kNumScreens;
            if (_isScreenValid(n, allowZeroTimeout)) {
                return static_cast<ScreenType>(n);
            }
        }
        return screen;
    }

}
