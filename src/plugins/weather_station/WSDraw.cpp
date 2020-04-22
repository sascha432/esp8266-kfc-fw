/**
 * Author: sascha_lammers@gmx.de
 */

#include <LoopFunctions.h>
#include "WSDraw.h"

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

WSDraw::WSDraw() :
    _tft(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST),
    _canvas(_tft.width(), _tft.height()),
    _scrollCanvas(nullptr),
    _weatherError(F("No data available")),
    _currentScreen(0),
    _lastTime(0)
{
#if _MSC_VER
    const char * data = PSTR("{\"coord\":{\"lon\":-123.07,\"lat\":49.32},\"weather\":[{\"id\":500,\"main\":\"Rain\",\"description\":\"light rain\",\"icon\":\"10n\"},{\"id\":701,\"main\":\"Mist\",\"description\":\"mist\",\"icon\":\"50n\"}],\"base\":\"stations\",\"main\":{\"temp\":277.55,\"pressure\":1021,\"humidity\":100,\"temp_min\":275.37,\"temp_max\":279.26},\"visibility\":8047,\"wind\":{\"speed\":1.19,\"deg\":165},\"rain\":{\"1h\":0.93},\"clouds\":{\"all\":90},\"dt\":1575357173,\"sys\":{\"type\":1,\"id\":5232,\"country\":\"CA\",\"sunrise\":1575301656,\"sunset\":1575332168},\"timezone\":-28800,\"id\":6090785,\"name\":\"North Vancouver\",\"cod\":200}");
    StreamString stream;
    stream.write((uint8_t*)data, strlen(data));
    //ProgmemStream stream(data, strlen(data));
    _weatherApi.clear();
    if (!_weatherApi.parseWeatherData(*(Stream*)&stream)) {
        _weatherError = F("Invalid data");
    }
#endif
}

WSDraw::~WSDraw()
{
    _displayMessageTimer.remove();
    ScrollCanvas::destroy(this);
}


void WSDraw::_drawTime()
{
    // _debug_printf_P(PSTR("WSDraw::_drawTime()\n"));

    _offsetY = 0;

#if 0
    int n = 0;
    for (uint16_t y = 0; y < 45; y++) {
        for (uint16_t x = 0; x < 128; x++) {
            uint8_t br = 0xff;// - (0xff * y / 45);
            int ww = 128 * 5;
            auto color = GFXCanvas::convertRGBtoRGB565(
                (n / ww) % 3 == 0 ? br : 0,
                (n / ww) % 3 == 1 ? br : 0,
                (n / ww) % 3 == 2 ? br : 0
                );
            _canvas.drawPixel(x, y, color);
            n++;
        }
    }
    _lastTime = time(nullptr);
    return;
#endif

    char buf[32];
    _lastTime = time(nullptr);

    struct tm *tm = timezone_localtime(&_lastTime);

    _canvas.setFont(FONTS_DATE);
    _canvas.setTextColor(COLORS_DATE);
    timezone_strftime_P(buf, sizeof(buf), PSTR("%a %b %d %Y"), tm);
    _canvas._drawTextAligned(X_POSITION_DATE, Y_POSITION_DATE, buf, H_POSITION_DATE);

    _canvas.setFont(FONTS_TIME);
    _canvas.setTextColor(COLORS_TIME);
    if (_config.time_format_24h) {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%H:%M:%S"), tm);
    }
    else {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%I:%M:%S"), tm);
    }
    _canvas._drawTextAligned(X_POSITION_TIME, Y_POSITION_TIME, buf, H_POSITION_TIME);

    _canvas.setFont(FONTS_TIMEZONE);
    _canvas.setTextColor(COLORS_TIMEZONE);
    if (_config.time_format_24h) {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%Z"), tm);
    }
    else {
        timezone_strftime_P(buf, sizeof(buf), PSTR("%p - %Z"), tm);
    }
    _canvas._drawTextAligned(X_POSITION_TIMEZONE, Y_POSITION_TIMEZONE, buf, H_POSITION_TIMEZONE);
}

void WSDraw::_drawIndoor()
{
    _drawIndoor(_canvas, Y_START_POSITION_WEATHER);
}

void WSDraw::_drawIndoor(GFXCanvasCompressed& canvas, uint8_t top)
{
    _offsetY = top;

    float data[3];
    _getIndoorValues(data);

    _offsetY += 5;

    _canvas.drawBitmap(6, _offsetY + 10, icon_house, 36, 37, 0xD6DA, COLORS_BACKGROUND);

    canvas.setFont(FONTS_TEMPERATURE);
    canvas.setTextColor(ST77XX_WHITE);
    canvas._drawTextAligned(TFT_WIDTH - 2, _offsetY, _getTemperature(data[0]), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
    _offsetY += canvas.getFontHeight(FONTS_TEMPERATURE);

    canvas.setTextColor(ST77XX_CYAN);
    canvas._drawTextAligned(TFT_WIDTH - 2, _offsetY, PrintString(F("%.1f%%"), data[1]), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
    _offsetY += canvas.getFontHeight(FONTS_TEMPERATURE);

    canvas.setTextColor(ST77XX_WHITE);
    canvas.setFont(&DejaVuSans_Bold_7pt8b);
    canvas._drawTextAligned(TFT_WIDTH - 2, _offsetY, PrintString(F("%.1fhPa"), data[2]), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
}

void WSDraw::_drawWeather()
{
    _drawWeather(_canvas, Y_START_POSITION_WEATHER);
}

void WSDraw::_drawWeather(GFXCanvasCompressed& canvas, uint8_t top)
{
    _offsetY = top;

    auto &info = _weatherApi.getWeatherInfo();
    if (info.hasData()) {

        // --- location

        static const uint16_t palette[] PROGMEM = { COLORS_BACKGROUND, ST77XX_WHITE, ST77XX_YELLOW, ST77XX_BLUE };
        canvas._drawBitmap(X_POSITION_WEATHER_ICON, Y_POSITION_WEATHER_ICON, getMiniMeteoconIconFromProgmem(info.weather[0].icon), palette);

        // create kind of shadow effect in case the text is drawn over the icon
        canvas.setFont(FONTS_CITY);
        canvas.setTextColor(COLORS_BACKGROUND);
        for(int8_t x = -2; x <= 2; x++) {
            for(int8_t y = 0; y < 2; y++) {
                if (!(y == 0 && x == 0)) {
                    canvas._drawTextAligned(X_POSITION_CITY + x, Y_POSITION_CITY + y, info.location, H_POSITION_CITY, AdafruitGFXExtension::TOP);
                }
            }
        }
        canvas.setTextColor(COLORS_CITY);
        canvas._drawTextAligned(X_POSITION_CITY, Y_POSITION_CITY, info.location, H_POSITION_CITY);

        // --- temperature

        canvas.setFont(FONTS_TEMPERATURE);
        canvas.setTextColor(COLORS_TEMPERATURE);
        canvas._drawTextAligned(X_POSITION_TEMPERATURE, Y_POSITION_TEMPERATURE, _getTemperature(info.val.temperature, true), H_POSITION_TEMPERATURE, AdafruitGFXExtension::TOP);

        // --- weather description

        canvas.setFont(FONTS_WEATHER_DESCR);
        canvas.setTextColor(COLORS_WEATHER_DESCR);

        AdafruitGFXExtension::Position_t pos;
        String tmp = info.weather[0].descr;
        if (tmp.length() > 10) {
            auto idx = tmp.indexOf(' ', 7); // wrap after first word thats longer than 7 characters and align to the right
            if (idx != -1) {
                canvas._drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp.substring(0, idx), H_POSITION_WEATHER_DESCR, AdafruitGFXExtension::TOP, &pos);
                canvas._drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR + pos.h + 2, tmp.substring(idx + 1), H_POSITION_WEATHER_DESCR, AdafruitGFXExtension::TOP);
                tmp = String();
            }
        }
        if (tmp.length()) {
            canvas._drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp, H_POSITION_WEATHER_DESCR);
        }

    }
    else {
        canvas.setFont(FONTS_DEFAULT_MEDIUM);
        canvas.setTextColor(COLORS_DEFAULT_TEXT);
        canvas._drawTextAligned(TFT_WIDTH / 2, (Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER), _weatherError, AdafruitGFXExtension::CENTER, AdafruitGFXExtension::MIDDLE);
    }
}


void WSDraw::_drawSunAndMoon()
{
    // _debug_printf_P(PSTR("WSDraw::_drawSunAndMoon()\n"));

    _offsetY = Y_START_POSITION_SUN_MOON;

    double moonDay;
    uint8_t moonPhase;
    char moonPhaseFont;
    calcMoon(time(nullptr), moonDay, moonPhase, moonPhaseFont, FONTS_MOON_PHASE_UPPERCASE);

    _canvas.setFont(FONTS_SUN_AND_MOON);
    _canvas.setTextColor(COLORS_SUN_AND_MOON);
    _canvas._drawTextAligned(X_POSITION_SUN_TITLE, Y_POSITION_SUN_TITLE, F("Sun"), H_POSITION_SUN_TITLE);
    _canvas._drawTextAligned(X_POSITION_MOON_PHASE_NAME, Y_POSITION_MOON_PHASE_NAME, moonPhaseName(moonPhase), H_POSITION_MOON_PHASE_NAME);
    _canvas._drawTextAligned(X_POSITION_MOON_PHASE_DAYS, Y_POSITION_MOON_PHASE_DAYS, PrintString("%dd", (int)round(moonDay)), H_POSITION_MOON_PHASE_NAME);

    auto &info = _weatherApi.getWeatherInfo();
    if (info.hasData()) {
        _canvas.setTextColor(COLORS_SUN_RISE_SET);
        _canvas.drawBitmap(X_POSITION_SUN_RISE_ICON, Y_POSITION_SUN_RISE_ICON, icon_sunrise, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);
        _canvas.drawBitmap(X_POSITION_SUN_SET_ICON, Y_POSITION_SUN_SET_ICON, icon_sunset, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);

        char buf[8];
        PGM_P timeFormat = PSTR("%H:%M");
        time_t time = info.getSunRiseAsGMT();
        struct tm *tm = gmtime(&time);
        strftime_P(buf, sizeof(buf), timeFormat, tm);
        _canvas._drawTextAligned(X_POSITION_SUN_SET, Y_POSITION_SUN_RISE, buf, H_POSITION_SUN_RISE);
        time = info.getSunSetAsGMT();
        tm = gmtime(&time);
        strftime_P(buf, sizeof(buf), timeFormat, tm);
        _canvas._drawTextAligned(X_POSITION_SUN_SET, Y_POSITION_SUN_SET, buf, H_POSITION_SUN_SET);
    }

    _canvas.setFont(FONTS_MOON_PHASE);
    _canvas.setTextColor(COLORS_MOON_PHASE);
    _canvas._drawTextAligned(X_POSITION_MOON_PHASE, Y_POSITION_MOON_PHASE, String(moonPhaseFont), H_POSITION_MOON_PHASE);
}

void WSDraw::_drawScreenMain()
{
    _drawTime();
    _drawWeather();
    _drawSunAndMoon();
    _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
}

void WSDraw::_drawScreenIndoor()
{
    _drawTime();
    _drawIndoor();
    _drawSunAndMoon();
    _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
}

void WSDraw::_drawScreenForecast()
{
    _drawTime();
    _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
}

void WSDraw::_doScroll()
{
    _debug_println();
    ScrollCanvas::create(this, TFT_WIDTH, Y_END_POSITION_WEATHER - Y_START_POSITION_WEATHER);
    _scrollPosition = 0;
}

void WSDraw::_scrollTimer(WSDraw &draw)
{
    if (_scrollCanvas) {
        // _debug_printf_P(PSTR("WSDraw::_scrollTimer(): _scrollPosition=%u\n"), _scrollPosition);
        if (_scrollPosition < TFT_WIDTH) {
            SpeedBooster speedBooster;
            static const uint8_t numScroll = 8;
            uint8_t height = (uint8_t)_scrollCanvas->getCanvas().height();
            for (uint8_t y = 0; y < height; y++) {
                auto &canvasLine = _canvas.getLine(y + Y_START_POSITION_WEATHER);
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

bool WSDraw::_isScrolling() const
{
    return _scrollCanvas != nullptr;
}

void WSDraw::_displayMessage(const String &title, const String &message, uint16_t titleColor, uint16_t messageColor, uint32_t timeout)
{
    ScrollCanvas::destroy(this);
    _displayMessageTimer.add(timeout * 1000, false, [this](EventScheduler::TimerPtr timer) {
        _draw();
    });

    uint16_t _top = 16;

    _canvas.fillScreen(COLORS_BACKGROUND);
    _canvas.fillScreenPartial(0, _top, ST77XX_ORANGE);

    _top += 4;
    // TODO wordwrap
    if (title.length()) {

        _top += _canvas.getFontHeight(FONTS_MESSAGE_TITLE);
        _canvas.setCursor(0, _top);
        _canvas.setTextColor(titleColor);
        _canvas.setFont(FONTS_MESSAGE_TITLE);
        _canvas.println(title);

        _top += 5;
    }

    _top += _canvas.getFontHeight(FONTS_MESSAGE_TEXT);
    _canvas.setCursor(0, _top);
    _canvas.setTextColor(messageColor);
    _canvas.setFont(FONTS_MESSAGE_TEXT);
    _canvas.println(message);

    _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
}

void WSDraw::_updateTime()
{
    if (_displayMessageTimer.active()) {
        return;
    }
    SpeedBooster speedBooster;
    _statsBegin();

    _canvas.fillScreenPartial(Y_START_POSITION_TIME, Y_END_POSITION_TIME - Y_START_POSITION_TIME, COLORS_BACKGROUND);
    _offsetX = 0;
    _offsetY = 0;
    _drawTime();

    _displayScreen(0, Y_START_POSITION_TIME, TFT_WIDTH, Y_END_POSITION_TIME - Y_START_POSITION_TIME);

    _statsEnd();
}

void WSDraw::_drawText(const String &text, const GFXfont *font, uint16_t color, bool clear)
{
    SpeedBooster speedBooster;
    ScrollCanvas::destroy(this);
    _canvas.fillScreen(COLORS_BACKGROUND);
    _canvas.setTextColor(color);
    _canvas.setFont(font);
    GFXCanvasCompressed::Position_t pos;
    if (clear) {
        _canvas._drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, GFXCanvasCompressed::CENTER, GFXCanvasCompressed::MIDDLE, &pos);
        _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
    }
    else {
        _canvas._drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, GFXCanvasCompressed::CENTER, GFXCanvasCompressed::MIDDLE, &pos);
        _displayScreen(0, pos.y, TFT_WIDTH, pos.h);
    }
}

void WSDraw::_draw()
{
    if (_displayMessageTimer.active()) {
        return;
    }
    if (_isScrolling()) {
        return;
    }
    SpeedBooster speedBooster;
    _statsBegin();

    _canvas.fillScreen(COLORS_BACKGROUND);

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
        _canvas.fillScreenPartial(y, 1, color);
        n++;
    }
#endif

    _offsetX = 0;
    _offsetY = 0;
    switch(_currentScreen)
    {
    case MAIN:
        _drawScreenMain();
        break;
    case INDOOR:
        _drawScreenIndoor();
        break;
    case FORECAST:
        _drawScreenForecast();
        break;
    case TEXT_CLEAR:
    case TEXT_UPDATE:
        _drawText(_text, _textFont, COLORS_DEFAULT_TEXT, true);
        _currentScreen = TEXT;
#ifndef _WIN32
        Scheduler.addTimer(150, true, [this](EventScheduler::TimerPtr timer) {
            if (_currentScreen != TEXT) {
                timer->detach();
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

    _statsEnd();
}

void WSDraw::_displayScreen(int16_t x, int16_t y, int16_t w, int16_t h)
{
    _tft.startWrite();
    _tft.setAddrWindow(x, y, w, h);
    _canvas.drawInto(x, y, w, h, [this](uint16_t x, uint16_t y, uint16_t w, uint16_t* pcolors) {
        _tft.writePixels(pcolors, w);
    });
    _tft.endWrite();
    _broadcastCanvas(x, y, w, h);
}

void WSDraw::_broadcastCanvas(int16_t x, int16_t y, int16_t w, int16_t h)
{
    // auto bitmap = getCanvas().getRLEStream(x, y, w, h);
    // int count = 0;
    // while (bitmap.available()) {
    //    bitmap.read();
    //    count++;
    // }
    // auto file = SPIFFS.open("test.bmp", fs::FileOpenMode::write);
    // auto stream = getCanvas().getBitmap(x, y, w, h);
    // while (stream.available()) {
    //     file.write(stream.read());
    // }
    // file.close();

}

void WSDraw::_statsBegin()
{
#if DEBUG_GFXCANVASCOMPRESSED_STATS
    _canvas.clearStats();
#endif
    _statsTimer.start();
}

void WSDraw::_statsEnd()
{

#if 1
    PrintString str(F("redraw %.2fms - "), _statsTimer.getTime() / 1000.0);
    str += _canvas.getDetails();
    str.replace(F("\n"), F(" "));
    debug_println(str);
#endif
}

String WSDraw::_getTemperature(float value, bool kelvin)
{
    if (_config.is_metric) {
        return String(kelvin ? OpenWeatherMapAPI::kelvinToC(value) : value, 1) + F("\xb0""C");
    }
    else {
        return String(kelvin ? OpenWeatherMapAPI::kelvinToF(value) : (value * 1.8f + 32.0f), 1) + F("\xb0""F");
    }
}

void WSDraw::_getIndoorValues(float *data)
{
    // dummy data
    data[0] = 23.38;
    data[1] = 49.13;
    data[2] = 1022.17;
}
