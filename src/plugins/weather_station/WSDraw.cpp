/**
 * Author: sascha_lammers@gmx.de
 */

#include <LoopFunctions.h>
#include <GFXCanvasConfig.h>
#include "WSDraw.h"
#include "WSScreen.h"

#if 1
#include "fonts/fonts_includes.h"
#else
#include "fonts/fonts.h"
#include "fonts/output/DejaVuSans_Bold/DejaVuSans_Bold_10pt8b.h"
#include "fonts/output/DejaVuSans/DejaVuSans_6pt8b.h"
#include "fonts/output/Dialog/Dialog_6pt8b.h"
#endif

// const GFXfont moon_phases12pt7b
// const GFXfont moon_phases14pt7b
// const GFXfont moon_phases16pt7b
// const GFXfont moon_phases18pt7b
// const GFXfont moon_phases20pt7b
// const GFXfont moon_phases24pt7b
// const GFXfont moon_phases28pt7b
// const GFXfont moon_phases32pt7b
#include "moon.h"

// https://github.com/ThingPulse/esp8266-weather-station-color/blob/master/weathericons.h
#include "weather_icons.h"


// sunrise 7x9px
const unsigned char icon_sunrise [] PROGMEM = {
    0b11111110,
    0b11101110,
    0b11000110,
    0b10000010,
    0b11101110,
    0b11101110,
    0b11101110,
    0b11111110,
    0b00000000,
};
// sunset 7x9px
const unsigned char icon_sunset [] PROGMEM = {
    0b11101110,
    0b11101110,
    0b11101110,
    0b11101110,
    0b10000010,
    0b11000110,
    0b11101110,
    0b11111110,
    0b00000000,
};

// house 36x37px
const unsigned char icon_house[] PROGMEM = {
    0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x03, 0xc1, 0xf8, 0x00, 0x00, 0x07,
    0xc3, 0xfc, 0x00, 0x00, 0x07, 0xc7, 0x9e, 0x00, 0x00, 0x07, 0xcf, 0x0f, 0x00, 0x00, 0x07, 0x9e,
    0x07, 0x80, 0x00, 0x07, 0x3c, 0x63, 0xc0, 0x00, 0x06, 0x78, 0xf1, 0xe0, 0x00, 0x00, 0xf1, 0xf8,
    0xf0, 0x00, 0x01, 0xe3, 0xfc, 0x78, 0x00, 0x03, 0xc7, 0xfe, 0x3c, 0x00, 0x07, 0x8f, 0xff, 0x1e,
    0x00, 0x0f, 0x1f, 0xff, 0x8f, 0x00, 0x1e, 0x3f, 0xff, 0xc7, 0x80, 0x3c, 0x7f, 0xff, 0xe3, 0xc0,
    0x78, 0xff, 0xff, 0xf1, 0xe0, 0xf1, 0xff, 0xff, 0xf8, 0xf0, 0x63, 0xff, 0xff, 0xfc, 0x60, 0x07,
    0xff, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x07, 0xff,
    0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff,
    0xfe, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 0xfe,
    0x00, 0x07, 0xfc, 0x03, 0xfe, 0x00, 0x07, 0xfc, 0x03, 0xfe, 0x00, 0x07, 0xfc, 0x03, 0xfe, 0x00,
    0x07, 0xfc, 0x03, 0xfe, 0x00, 0x07, 0xfc, 0x03, 0xfe, 0x00, 0x07, 0xfc, 0x03, 0xfe, 0x00, 0x07,
    0xfc, 0x03, 0xfe, 0x00, 0x03, 0xfc, 0x03, 0xfe, 0x00
};

#include <debug_helper_enable.h>

#define __LDBG_isCanvasAttached() \
        if (!isCanvasAttached()) { \
            __DBG_panic("canvas not attached"); \
        }

// using namespace WeatherStation::Screen;

namespace WSDraw {

    Base::Base() :
        _tft(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST),
        _canvas(new CanvasType(_tft.width(), _tft.height())),
        _canvasLocked(0),
        _scrollCanvas(nullptr),
        _textFont(nullptr),
        _lastTime(0),
        _screenLastUpdateTime(~0),
        _offsetX(0),
        _offsetY(0),
        _scrollPosition(0),
        _currentScreen(ScreenType::MAIN),
        _redrawFlag(false)
    {
    }

    Base::~Base()
    {
        // _screenTimer.remove();
        // delete _screen;
        // _displayMessageTimer.remove();
        //ScrollCanvas::destroy(this);
        if (_canvas) {
            delete _canvas;
        }
    }

    void Base::redraw()
    {
        _redrawFlag = true;
    }

    bool Base::_attachCanvas()
    {
        if (_canvasLocked > 0) {
            _canvasLocked--;
        }
        // __DBG_printf("locked=%u canvas=%p", _canvasLocked, _canvas);
        if (_canvasLocked == 0) {
            if (!_canvas) {
                _canvas = new CanvasType(_tft.width(), _tft.height());
                __DBG_printf("new canvas=%p invoking redraw", _canvas);
            }
            else {
                __DBG_printf("canvas=%p invoking redraw", _canvas);
            }
            redraw();
        }
        return (_canvasLocked == 0);
    }

    bool Base::_detachCanvas(bool release)
    {
        if (_canvasLocked++ == 0 && release) {
            __DBG_printf("canvas released=%p", _canvas);
            delete _canvas;
            _canvas = nullptr;
        }
        __DBG_printf("locked=%d release=%u canvas=%p", _canvasLocked, release, _canvas);
        return (_canvasLocked == 1);
    }

    bool Base::isCanvasAttached() const
    {
        return (_canvas != nullptr) && _canvasLocked == 0;
    }

    CanvasType *Base::getCanvasAndLock()
    {
        return _canvasLocked++ == 0 ? _canvas : nullptr;
    }

    void Base::releaseCanvasLock()
    {
        if (_canvasLocked) {
            _canvasLocked--;
        }
        if (_canvasLocked == 0) {

        }
    }

#if 1

    void Base::drawText(const String &text, const GFXfont *font, uint16_t color, bool clear)
    {
        if (isCanvasAttached()) {
            _drawText(text, font, color, clear);
        }
        else {
            ScrollCanvas::destroy(this);

            _tft.fillScreen(COLORS_BACKGROUND);
            _tft.setTextColor(color);
            _tft.setFont(font);
            DisplayType::Position_t pos;
            if (clear) {
                _tft.drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, DisplayType::CENTER, DisplayType::MIDDLE, &pos);
            }
            else {
                _tft.drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, DisplayType::CENTER, DisplayType::MIDDLE, &pos);
            }
        }
    }

    // void Base::_drawTime()
    // {
    //     __LDBG_isCanvasAttached();
    //     // __LDBG_printf("_drawTime()");

    //     _offsetY = 0;

    // #if 0
    //     int n = 0;
    //     for (uint16_t y = 0; y < 45; y++) {
    //         for (uint16_t x = 0; x < 128; x++) {
    //             uint8_t br = 0xff;// - (0xff * y / 45);
    //             int ww = 128 * 5;
    //             auto color = GFXCanvas::convertRGBtoRGB565(
    //                 (n / ww) % 3 == 0 ? br : 0,
    //                 (n / ww) % 3 == 1 ? br : 0,
    //                 (n / ww) % 3 == 2 ? br : 0
    //                 );
    //             _canvas->drawPixel(x, y, color);
    //             n++;
    //         }
    //     }
    //     _lastTime = time(nullptr);
    //     return;
    // #endif

    //     char buf[32];
    //     _lastTime = time(nullptr);
    //     struct tm *tm = localtime(&_lastTime);

    //     _canvas->setFont(FONTS_DATE);
    //     _canvas->setTextColor(COLORS_DATE);
    //     strftime_P(buf, sizeof(buf), PSTR("%a %b %d %Y"), tm);
    //     _canvas->drawTextAligned(X_POSITION_DATE, Y_POSITION_DATE, buf, H_POSITION_DATE);

    //     _canvas->setFont(FONTS_TIME);
    //     _canvas->setTextColor(COLORS_TIME);
    //     if (_config.time_format_24h) {
    //         strftime_P(buf, sizeof(buf), PSTR("%H:%M:%S"), tm);
    //     }
    //     else {
    //         strftime_P(buf, sizeof(buf), PSTR("%I:%M:%S"), tm);
    //     }
    //     _canvas->drawTextAligned(X_POSITION_TIME, Y_POSITION_TIME, buf, H_POSITION_TIME);

    //     _canvas->setFont(FONTS_TIMEZONE);
    //     _canvas->setTextColor(COLORS_TIMEZONE);
    //     if (_config.time_format_24h) {
    //         strftime_P(buf, sizeof(buf), PSTR("%Z"), tm);
    //     }
    //     else {
    //         strftime_P(buf, sizeof(buf), PSTR("%p - %Z"), tm);
    //     }
    //     _canvas->drawTextAligned(X_POSITION_TIMEZONE, Y_POSITION_TIMEZONE, buf, H_POSITION_TIMEZONE);
    // }

    void Base::_updateTime()
    {
        __LDBG_isCanvasAttached();

    #if WSDRAW_STATS
        _statsBegin();
    #endif

        _canvas->fillScreenPartial(Y_START_POSITION_TIME, Y_END_POSITION_TIME - Y_START_POSITION_TIME, COLORS_BACKGROUND);
        _offsetX = 0;
        _offsetY = 0;
        _drawTime();

        _displayScreen(0, Y_START_POSITION_TIME, TFT_WIDTH, Y_END_POSITION_TIME - Y_START_POSITION_TIME);

    #if WSDRAW_STATS
        _statsEnd(F("updateTime"));
    #endif
    }

    void Base::_drawIndoor()
    {
        __LDBG_isCanvasAttached();
        _drawIndoor(_canvas, Y_START_POSITION_WEATHER);
    }

    void Base::_drawIndoor(GFXCanvasCompressed *canvas, int16_t top)
    {
        _offsetY = top;

        float data[3];
        _getIndoorValues(data);

        _offsetY += 5;

        canvas->drawBitmap(6, _offsetY + 10, icon_house, 36, 37, 0xD6DA, COLORS_BACKGROUND);

        canvas->setFont(FONTS_TEMPERATURE);
        canvas->setTextColor(COLORS_WHITE);
        canvas->drawTextAligned(TFT_WIDTH - 2, _offsetY, _getTemperature(data[0]), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
        _offsetY += canvas->getFontHeight(FONTS_TEMPERATURE);

        canvas->setTextColor(COLORS_CYAN);
        canvas->drawTextAligned(TFT_WIDTH - 2, _offsetY, PrintString(F("%.0f%%"), data[1]), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
        _offsetY += canvas->getFontHeight(FONTS_TEMPERATURE);

        canvas->setTextColor(COLORS_WHITE);
        canvas->setFont(&DejaVuSans_Bold_7pt8b);
        canvas->drawTextAligned(TFT_WIDTH - 2, _offsetY, PrintString(F("%.1fhPa"), data[2]), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
    }

    void Base::_drawWeather()
    {
        __LDBG_isCanvasAttached();
        _drawWeather(_canvas, Y_START_POSITION_WEATHER);
    }

    void Base::_drawWeather(GFXCanvasCompressed *canvas, int16_t top)
    {
        _offsetY = top;

        auto &info = _weatherApi.getWeatherInfo();
        if (info.hasData()) {

            // --- location
            static const uint16_t palette[] PROGMEM = { COLORS_LOCATION };
            canvas->drawBitmap(X_POSITION_WEATHER_ICON, Y_POSITION_WEATHER_ICON, getMiniMeteoconIconFromProgmem(info.weather[0].icon), palette);

            // create kind of shadow effect in case the text is drawn over the icon
            canvas->setFont(FONTS_CITY);
            canvas->setTextColor(COLORS_BACKGROUND);
            for(int8_t x = -2; x <= 2; x++) {
                for(int8_t y = 0; y < 2; y++) {
                    if (!(y == 0 && x == 0)) {
                        canvas->drawTextAligned(X_POSITION_CITY + x, Y_POSITION_CITY + y, info.location, H_POSITION_CITY, AdafruitGFXExtension::TOP);
                    }
                }
            }
            canvas->setTextColor(COLORS_CITY);
            canvas->drawTextAligned(X_POSITION_CITY, Y_POSITION_CITY, info.location, H_POSITION_CITY);

            // --- temperature

            canvas->setFont(FONTS_TEMPERATURE);
            canvas->setTextColor(COLORS_TEMPERATURE);
            canvas->drawTextAligned(X_POSITION_TEMPERATURE, Y_POSITION_TEMPERATURE, _getTemperature(info.val.temperature, true), H_POSITION_TEMPERATURE, AdafruitGFXExtension::TOP);

            // --- weather description

            canvas->setFont(FONTS_WEATHER_DESCR);
            canvas->setTextColor(COLORS_WEATHER_DESCR);

            AdafruitGFXExtension::Position_t pos;
            String tmp = info.weather[0].descr;
            if (tmp.length() > 10) {
                auto idx = tmp.indexOf(' ', 7); // wrap after first word thats longer than 7 characters and align to the right
                if (idx != -1) {
                    canvas->drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp.substring(0, idx), H_POSITION_WEATHER_DESCR, AdafruitGFXExtension::TOP, &pos);
                    canvas->drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR + pos.h + 2, tmp.substring(idx + 1), H_POSITION_WEATHER_DESCR, AdafruitGFXExtension::TOP);
                    tmp = String();
                }
            }
            if (tmp.length()) {
                canvas->drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp, H_POSITION_WEATHER_DESCR);
            }

        }
        else {
            canvas->setFont(FONTS_DEFAULT_MEDIUM);
            canvas->setTextColor(COLORS_DEFAULT_TEXT);
            canvas->drawTextAligned(TFT_WIDTH / 2, (Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER), _weatherApi.getWeatherInfo().getError(), AdafruitGFXExtension::CENTER, AdafruitGFXExtension::MIDDLE);
        }

        _drawWeatherIndoor(canvas, Y_START_POSITION_WEATHER);
    }

    void Base::_drawWeatherIndoor(GFXCanvasCompressed *canvas, int16_t top)
    {
        float data[3];
        _getIndoorValues(data);

        _offsetY = top;

        canvas->setFont(FONTS_WEATHER_INDOOR);
        canvas->setTextColor(COLORS_WHITE);
        canvas->drawTextAligned(X_POSITION_WEATHER_INDOOR_TEMP, Y_POSITION_WEATHER_INDOOR_TEMP, _getTemperature(data[0]), AdafruitGFXExtension::LEFT, AdafruitGFXExtension::TOP);

        canvas->setTextColor(COLORS_CYAN);
        canvas->drawTextAligned(X_POSITION_WEATHER_INDOOR_HUMIDITY, Y_POSITION_WEATHER_INDDOR_HUMIDITY, PrintString(F("%.1f%%"), data[1]), AdafruitGFXExtension::CENTER, AdafruitGFXExtension::TOP);

        canvas->setTextColor(COLORS_WHITE);
        canvas->drawTextAligned(X_POSITION_WEATHER_INDOOR_PRESSURE, Y_POSITION_WEATHER_INDDOR_PRESSURE, PrintString(F("%.0fhPa"), data[2]), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
    }

    void Base::_updateWeatherIndoor()
    {
        __LDBG_isCanvasAttached();

        #if WSDRAW_STATS
            _statsBegin();
        #endif
        int height = _canvas->getFontHeight(FONTS_WEATHER_INDOOR);

        _canvas->fillScreenPartial(Y_START_POSITION_WEATHER, height, COLORS_BACKGROUND);
        _drawWeatherIndoor(_canvas, Y_START_POSITION_WEATHER);
        _displayScreen(0, Y_POSITION_WEATHER_INDOOR_TEMP, TFT_WIDTH, height);

        #if WSDRAW_STATS
            _statsEnd(F("updateWeatherIndoor"));
        #endif
    }

    void Base::_drawSunAndMoon()
    {
        __LDBG_isCanvasAttached();
        // __LDBG_printf("WSDraw::_drawSunAndMoon()");

        _offsetY = Y_START_POSITION_SUN_MOON;

        float moonDay;
        uint8_t moonPhase;
        char moonPhaseFont;
        calcMoon(time(nullptr), moonDay, moonPhase, moonPhaseFont, FONTS_MOON_PHASE_UPPERCASE);

        _canvas->setFont(FONTS_SUN_AND_MOON);
        _canvas->setTextColor(COLORS_SUN_AND_MOON);
        _canvas->drawTextAligned(X_POSITION_SUN_TITLE, Y_POSITION_SUN_TITLE, F("Sun"), H_POSITION_SUN_TITLE);
        _canvas->drawTextAligned(X_POSITION_MOON_PHASE_NAME, Y_POSITION_MOON_PHASE_NAME, moonPhaseName(moonPhase), H_POSITION_MOON_PHASE_NAME);
        _canvas->drawTextAligned(X_POSITION_MOON_PHASE_DAYS, Y_POSITION_MOON_PHASE_DAYS, PrintString("%dd", (int)round(moonDay)), H_POSITION_MOON_PHASE_NAME);

        auto &info = _weatherApi.getWeatherInfo();
        if (info.hasData()) {
            _canvas->setTextColor(COLORS_SUN_RISE_SET);
            _canvas->drawBitmap(X_POSITION_SUN_RISE_ICON, Y_POSITION_SUN_RISE_ICON, icon_sunrise, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);
            _canvas->drawBitmap(X_POSITION_SUN_SET_ICON, Y_POSITION_SUN_SET_ICON, icon_sunset, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);

            char buf[8];
            PGM_P timeFormat = PSTR("%H:%M");
            time_t time = info.getSunRiseAsGMT();
            struct tm *tm = gmtime(&time);
            strftime_P(buf, sizeof(buf), timeFormat, tm);
            _canvas->drawTextAligned(X_POSITION_SUN_SET, Y_POSITION_SUN_RISE, buf, H_POSITION_SUN_RISE);
            time = info.getSunSetAsGMT();
            tm = gmtime(&time);
            strftime_P(buf, sizeof(buf), timeFormat, tm);
            _canvas->drawTextAligned(X_POSITION_SUN_SET, Y_POSITION_SUN_SET, buf, H_POSITION_SUN_SET);
        }

        _canvas->setFont(FONTS_MOON_PHASE);
        _canvas->setTextColor(COLORS_MOON_PHASE);
        _canvas->drawTextAligned(X_POSITION_MOON_PHASE, Y_POSITION_MOON_PHASE, String(moonPhaseFont), H_POSITION_MOON_PHASE);
    }

    void Base::_drawScreenMain()
    {
        __LDBG_isCanvasAttached();

        _drawTime();
        _drawWeather();
        _drawSunAndMoon();
        _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
    }

    void Base::_drawScreenIndoor()
    {
        __LDBG_isCanvasAttached();

        _drawTime();
        _drawIndoor();
        _drawSunAndMoon();
        _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
    }

    void Base::_updateScreenIndoor()
    {
        __LDBG_isCanvasAttached();

        #if WSDRAW_STATS
            _statsBegin();
        #endif

        _canvas->fillScreenPartial(Y_START_POSITION_WEATHER, Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER, COLORS_BACKGROUND);
        _drawIndoor();
        _displayScreen(0, Y_START_POSITION_WEATHER, TFT_WIDTH, Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER);

        #if WSDRAW_STATS
            _statsEnd(F("updateScreenIndoor"));
        #endif
    }

    void Base::_drawScreenForecast()
    {
        __LDBG_isCanvasAttached();

        _drawTime();
        _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
    }

    void Base::_doScroll()
    {
        __LDBG_println();
        ScrollCanvas::create(this, TFT_WIDTH, Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER);
        _scrollPosition = 0;
    }

    void Base::_scrollTimer(Base &draw)
    {
        if (_scrollCanvas) {
            // __LDBG_printf("WSDraw::_scrollTimer(): _scrollPosition=%u", _scrollPosition);
            if (_scrollPosition < TFT_WIDTH) {
                static const uint8_t numScroll = 8;
                uint8_t height = (uint8_t)_scrollCanvas->getCanvas().height();
                for (uint8_t y = 0; y < height; y++) {
                    auto &canvasLine = _canvas->getLine(y + Y_START_POSITION_WEATHER);
                    memmove(canvasLine.getBuffer(), &canvasLine.getBuffer()[numScroll], (TFT_WIDTH - numScroll) * sizeof(uint16_t)); // move content to the left
                    canvasLine.setWriteFlag(true);
                    // copy new content
                    auto scrollCanvasLine = _scrollCanvas->getCanvas().getLine(y).getBuffer();
                    for (uint8_t x = 0; x < numScroll; x++) {
                        canvasLine.setPixel(TFT_WIDTH - numScroll + x, scrollCanvasLine[_scrollPosition + x]);
                    }
                }
                _scrollPosition += numScroll;
                _displayScreen(0, Y_START_POSITION_WEATHER, TFT_WIDTH, height);
            }
            else {
                ScrollCanvas::destroy(&draw);
            }
        }
    }

    bool Base::_isScrolling() const
    {
        return _scrollCanvas != nullptr;
    }

    void Base::_displayMessage(const String &title, const String &message, uint16_t titleColor, uint16_t messageColor, uint32_t timeout)
    {
        __LDBG_isCanvasAttached();

        ScrollCanvas::destroy(this);
        _Timer(_displayMessageTimer).add(Event::seconds(timeout), false, [this](Event::CallbackTimerPtr timer) {
            redraw();
        });

        int16_t _top = 16;

        _canvas->fillScreen(COLORS_BACKGROUND);
        _canvas->fillScreenPartial(0, _top, COLORS_ORANGE);

        _top -= 2;
        // TODO wordwrap
        if (title.length()) {

            _top += _canvas->getFontHeight(FONTS_MESSAGE_TITLE);
            _canvas->setCursor(0, _top);
            _canvas->setTextColor(titleColor);
            _canvas->setFont(FONTS_MESSAGE_TITLE);
            _canvas->println(title);

            _top += 5;
        }

        _top += _canvas->getFontHeight(FONTS_MESSAGE_TEXT);
        _canvas->setCursor(0, _top);
        _canvas->setTextColor(messageColor);
        _canvas->setFont(FONTS_MESSAGE_TEXT);
        _canvas->println(message);

        _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
    }

    void Base::_drawText(const String &text, const GFXfont *font, uint16_t color, bool clear)
    {
        __LDBG_isCanvasAttached();

        ScrollCanvas::destroy(this);
        _canvas->fillScreen(COLORS_BACKGROUND);
        _canvas->setTextColor(color);
        _canvas->setFont(font);
        GFXCanvasCompressed::Position_t pos;
        if (clear) {
            _canvas->drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, GFXCanvasCompressed::CENTER, GFXCanvasCompressed::MIDDLE, &pos);
            _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
        }
        else {
            _canvas->drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, GFXCanvasCompressed::CENTER, GFXCanvasCompressed::MIDDLE, &pos);
            _displayScreen(0, pos.y, TFT_WIDTH, pos.h);
        }
    }

    void Base::_draw()
    {
        __LDBG_isCanvasAttached();

        if (_displayMessageTimer) {
            return;
        }
        if (_isScrolling()) {
            return;
        }
        #if WSDRAW_STATS
            _statsBegin();
        #endif

        _canvas->fillScreen(COLORS_BACKGROUND);

        #if 0
            int n = 0;
            for (uint16_t y = 0; y < 160; y++) {
                uint8_t br = 0xff;// - (0xff * y / 45);
                int ww = 5;
                auto color = GFXCanvas::convertRGBtoRGB565(
                    (n / ww) % 3 == 0 ? br : 0,
                    (n / ww) % 3 == 1 ? br : 0,
                    (n / ww) % 3 == 2 ? br : 0
                );
                _canvas->fillScreenPartial(y, 1, color);
                n++;
            }
        #endif

        _offsetX = 0;
        _offsetY = 0;
        switch(_currentScreen) {
            case ScreenType::MAIN:
                _drawScreenMain();
                break;
            case ScreenType::INDOOR:
                _drawScreenIndoor();
                break;
            case ScreenType::FORECAST:
                _drawScreenForecast();
                break;
            case ScreenType::TEXT_CLEAR:
            case ScreenType::TEXT_UPDATE:
                _drawText(_text, _textFont, COLORS_DEFAULT_TEXT, true);
                _currentScreen = ScreenType::TEXT;
                #ifndef _WIN32
                    _Scheduler.add(150, true, [this](Event::CallbackTimerPtr timer) {
                        if (_currentScreen != ScreenType::TEXT) {
                            timer->disarm();
                        }
                        else {
                            _drawText(_text, _textFont, COLORS_DEFAULT_TEXT);
                        }
                    });
                #endif
                break;
            default:
                break;
        }

        #if WSDRAW_STATS
            _statsEnd(F("draw"));
        #endif
    }

    #endif

    void Base::_displayScreen(int16_t x, int16_t y, int16_t w, int16_t h)
    {
        __LDBG_isCanvasAttached();

        // copy canvas into tft memory
        _tft.startWrite();
        _tft.setAddrWindow(x, y, w, h);
        _canvas->drawInto(_tft, x, y, w, h);
        _tft.endWrite();

        // promote full or partial update
        canvasUpdatedEvent(x, y, w, h);
    }

    void Base::canvasUpdatedEvent(int16_t x, int16_t y, int16_t w, int16_t h)
    {
    }

    DisplayType &Base::getDisplay()
    {
        return _tft;
    }

    CanvasType *Base::getCanvas()
    {
        if (isCanvasAttached()) {
            return _canvas;
        }
        return nullptr;
    }

    // void Base::setScreen(BaseScreen *screen)
    // {
    //     if (_screen) {
    //         screen->deletePrevScreen(_screen);
    //     }
    //     _screen = screen;
    //     if (_screen) {
    //         _screen->initNextScreen();
    //     }
    //     _screenLastUpdateTime = ~0;
    //     redraw();
    // }

    #if WSDRAW_STATS

    void Base::_statsBegin()
    {
        __DBG_STATS(_canvas->clearStats());
        _statsTimer.start();
    }

    void Base::_statsEnd(const __FlashStringHelper *name)
    {
        if (_debug_stats) {
            float time = _statsTimer.getTime() / 1000.0f;
            PrintString str(F("redraw %s %.2fms - "), name, time);
            str += _canvas->getDetails();
            str.replace(String('\n'), String(' '));
            debug_println(str);

            auto iterator = _stats.find(name);
            if (iterator == _stats.end()) {
                _stats[name] = StatsBuffer();
            }
            _stats[name].push_back(time);
        }
    }

    #endif

    String Base::_getTemperature(float value, bool kelvin)
    {
        if (_config.is_metric) {
            return String(kelvin ? OpenWeatherMapAPI::kelvinToC(value) : value, 1) + '\xb0' + 'C';
        }
        else {
            return String(kelvin ? OpenWeatherMapAPI::kelvinToF(value) : (value * 1.8f + 32.0f), 1) + '\xb0' + 'F';
        }
    }

    void Base::_getIndoorValues(float *data)
    {
        // dummy data
        data[0] = 23.38f;
        data[1] = 49.13f;
        data[2] = 1022.17f;
    }

    void Base::_drawTime()
    {
        _offsetY = 0;
        _offsetX = 0;

        PrintString timeStr;
        auto tm = localtime(&_lastTime);

        _canvas->setFont(FONTS_DATE);
        _canvas->setTextColor(COLORS_DATE);
        timeStr.strftime(F("%a %b %d %Y"), tm);
        _canvas->drawTextAligned(X_POSITION_DATE, Y_POSITION_DATE, timeStr.c_str(), H_POSITION_DATE);

        _canvas->setFont(FONTS_TIME);
        _canvas->setTextColor(COLORS_TIME);
        timeStr = PrintString();
        timeStr.strftime(_config.time_format_24h ? F("%H:%M:%S") : F("%I:%M:%S"), tm);
        _canvas->drawTextAligned(X_POSITION_TIME, Y_POSITION_TIME, timeStr.c_str(), H_POSITION_TIME);

        _canvas->setFont(FONTS_TIMEZONE);
        _canvas->setTextColor(COLORS_TIMEZONE);
        timeStr = PrintString();
        timeStr.strftime(_config.time_format_24h ? F("%Z") : F("%p - %Z"), tm);
        _canvas->drawTextAligned(X_POSITION_TIMEZONE, Y_POSITION_TIMEZONE, timeStr.c_str(), H_POSITION_TIMEZONE);
    }


    const __FlashStringHelper *Base::getScreenName(ScreenType screen)
    {
        switch(screen) {
            case ScreenType::MAIN:
                return F("Current Weather, Sun and Moon");
            case ScreenType::INDOOR:
                return F("Indoor Climate");
            case ScreenType::FORECAST:
                return F("Weather Forecast");
        }
        return nullptr;
    }

}
