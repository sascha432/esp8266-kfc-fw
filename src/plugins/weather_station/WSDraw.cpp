/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <LoopFunctions.h>
#include <stl_ext/memory.h>
#include <GFXCanvasConfig.h>
#include <locale.h>
#if DEBUG_GFXCANVAS_STATS
#include <GFXCanvasStats.h>
#endif
#include "WSDraw.h"
#include "moon_phase.h"
#include "format_time.h"

#include <TJpg_Decoder.h>
#include <../src/plugins/weather_station/WeatherStationBase.h>

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

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
#include "moon/moon_font.h"

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

namespace WSDraw {

    Base::Base() :
        _tft(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST),
        _canvas(new CanvasType(_tft.width(), _tft.height())),
        _tftOutputMaxHeight(_tft.height()),
        _tftOverlayCanvas(nullptr),
        _scrollCanvas(nullptr),
        _textFont(nullptr),
        _lastTime(0),
        _scrollPosition(0),
        _currentScreen(ScreenType::MAIN),
        _redrawFlag(false),
        _locked(false)
    {
    }

    Base::~Base()
    {
        // _screenTimer.remove();
        // delete _screen;
        // _displayMessageTimer.remove();
        lock();
        ScrollCanvas::destroy(this);
        stdex::reset(_canvas);
        stdex::reset(_tftOverlayCanvas);
    }

    void Base::_drawSunAndMoon()
    {
        constexpr int16_t _offsetY = Y_START_POSITION_SUN_MOON;

        auto moon = calcMoon(getUnixtimeForCalcMoon());

        _canvas->setFont(FONTS_SUN_AND_MOON);
        _canvas->setTextColor(COLORS_SUN_AND_MOON);
        _canvas->drawTextAligned(X_POSITION_SUN_TITLE, Y_POSITION_SUN_TITLE, F("Sun"), H_POSITION_SUN_TITLE);
        _canvas->drawTextAligned(X_POSITION_MOON_PHASE_NAME, Y_POSITION_MOON_PHASE_NAME, moon.moonPhaseName(), H_POSITION_MOON_PHASE_NAME);
        _canvas->drawTextAligned(X_POSITION_MOON_PHASE_DAYS, Y_POSITION_MOON_PHASE_DAYS, PrintString(F("%.1fd"), moon.moonDayDouble()), H_POSITION_MOON_PHASE_NAME);
        _canvas->drawTextAligned(X_POSITION_MOON_PHASE_DAYS, Y_POSITION_MOON_PHASE_PCT, PrintString(F("%.1f%%"), moon.moonPhaseIlluminationPct()), H_POSITION_MOON_PHASE_NAME);


        auto &info = _weatherApi.getWeatherInfo();
        if (info.hasData()) {
            _canvas->setTextColor(COLORS_SUN_RISE_SET);
            _canvas->drawBitmap(X_POSITION_SUN_RISE_ICON, Y_POSITION_SUN_RISE_ICON, icon_sunrise, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);
            _canvas->drawBitmap(X_POSITION_SUN_SET_ICON, Y_POSITION_SUN_SET_ICON, icon_sunset, 7, 9, COLORS_BACKGROUND, COLORS_SUN_RISE_SET);

            char buf[8];
            auto timeFormat = PSTR("%H:%M");
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
        _canvas->drawTextAligned(X_POSITION_MOON_PHASE, Y_POSITION_MOON_PHASE, String(moon.moonPhaseFont), H_POSITION_MOON_PHASE);
    }

    void Base::_drawIndoorClimate()
    {
        int16_t _offsetY = Y_START_POSITION_INDOOR + 5;

        _canvas->drawBitmap(6, _offsetY + 10, icon_house, 36, 37, 0xD6DA, COLORS_BACKGROUND);

        auto data = _getIndoorValues();

        _canvas->setFont(FONTS_TEMPERATURE);
        _canvas->setTextColor(COLORS_WHITE);
        _canvas->drawTextAligned(TFT_WIDTH - 2, _offsetY, _getTemperature(data.getTemperature()), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
        _offsetY += _canvas->getFontHeight(FONTS_TEMPERATURE);

        _canvas->setTextColor(COLORS_CYAN);
        _canvas->drawTextAligned(TFT_WIDTH - 2, _offsetY, PrintString(F("%.1f%%"), data.getHumidity()), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
        _offsetY += _canvas->getFontHeight(FONTS_TEMPERATURE);

        _canvas->setTextColor(COLORS_WHITE);
        _canvas->setFont(FONTS_PRESSURE);
        _canvas->drawTextAligned(TFT_WIDTH - 2, _offsetY, PrintString(F("%.1fhPa"), data.getPressure()), AdafruitGFXExtension::RIGHT, AdafruitGFXExtension::TOP);
    }

    static const uint16_t palette[] PROGMEM = { COLORS_LOCATION };

    void Base::_drawLocalWeather()
    {
        constexpr int16_t _offsetY = Y_START_POSITION_WEATHER;
        auto &info = _weatherApi.getWeatherInfo();
        if (info.hasData()) {

            // --- location
            _canvas->drawBitmap(X_POSITION_WEATHER_ICON, Y_POSITION_WEATHER_ICON, getMiniIconFromProgmem(info.weather[0].icon), palette);

            // create kind of shadow effect in case the text is drawn over the icon
            _canvas->setFont(FONTS_CITY);
            _canvas->setTextColor(COLORS_BACKGROUND);
            for(int8_t x = -2; x <= 2; x++) {
                for(int8_t y = 0; y < 2; y++) {
                    if (!(y == 0 && x == 0)) {
                        _canvas->drawTextAligned(X_POSITION_CITY + x, Y_POSITION_CITY + y, info.location, H_POSITION_CITY);
                    }
                }
            }
            _canvas->setTextColor(COLORS_CITY);
            _canvas->drawTextAligned(X_POSITION_CITY, Y_POSITION_CITY, info.location, H_POSITION_CITY);

            // --- temperature

            _canvas->setFont(FONTS_TEMPERATURE);
            _canvas->setTextColor(COLORS_TEMPERATURE);
            _canvas->drawTextAligned(X_POSITION_TEMPERATURE, Y_POSITION_TEMPERATURE, _getTemperature(info.val.temperature, true), H_POSITION_TEMPERATURE);

            // --- weather description

            _canvas->setFont(FONTS_WEATHER_DESCR);
            _canvas->setTextColor(COLORS_WEATHER_DESCR);

            AdafruitGFXExtension::Position_t pos;
            String tmp = info.weather[0].descr;
            if (tmp.length() > 10) {
                auto idx = tmp.indexOf(' ', 7); // wrap after first word thats longer than 7 characters and align to the right
                if (idx != -1) {
                    _canvas->drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp.substring(0, idx), H_POSITION_WEATHER_DESCR, AdafruitGFXExtension::TOP, &pos);
                    _canvas->drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR + pos.h + 2, tmp.substring(idx + 1), H_POSITION_WEATHER_DESCR);
                    tmp = String();
                }
            }
            if (tmp.length()) {
                _canvas->drawTextAligned(X_POSITION_WEATHER_DESCR, Y_POSITION_WEATHER_DESCR, tmp, H_POSITION_WEATHER_DESCR);
            }
        }
        else {
            auto error = _weatherApi.getWeatherInfo().getError();
            if (error.length()) {
                _canvas->setFont(FONTS_DEFAULT_SMALL);
                _canvas->setTextColor(COLORS_RED);
                _canvas->drawTextAligned(TFT_WIDTH / 2, Y_START_POSITION_FORECAST + 15, F("Loading Weather Failed"), AdafruitGFXExtension::CENTER);
                _canvas->drawTextAligned(TFT_WIDTH / 2, Y_START_POSITION_FORECAST + 30, error, AdafruitGFXExtension::CENTER);
            }
            else {
                _canvas->setFont(FONTS_DEFAULT_MEDIUM);
                _canvas->setTextColor(COLORS_ORANGE);
                _canvas->drawTextAligned(TFT_WIDTH / 2, Y_START_POSITION_FORECAST + 15, F("Weather\nLoading..."), AdafruitGFXExtension::CENTER);
            }
        }
    }

    void Base::_drawIndoorClimateBottom()
    {
        constexpr int16_t _offsetY = Y_START_POSITION_INDOOR_BOTTOM;
        auto data = _getIndoorValues();

        _canvas->setFont(FONTS_WEATHER_INDOOR);
        _canvas->setTextColor(COLORS_WHITE);
        _canvas->drawTextAligned(X_POSITION_WEATHER_INDOOR_TEMP, Y_POSITION_WEATHER_INDOOR_TEMP, _getTemperature(data.getTemperature()), AdafruitGFXExtension::LEFT);

        _canvas->setTextColor(COLORS_CYAN);
        _canvas->drawTextAligned(X_POSITION_WEATHER_INDOOR_HUMIDITY, Y_POSITION_WEATHER_INDOOR_HUMIDITY, PrintString(F("%.1f%%"), data.getHumidity()), AdafruitGFXExtension::CENTER);

        _canvas->setTextColor(COLORS_WHITE);
        _canvas->drawTextAligned(X_POSITION_WEATHER_INDOOR_PRESSURE, Y_POSITION_WEATHER_INDOOR_PRESSURE, PrintString(F("%.0fhPa"), data.getPressure()), AdafruitGFXExtension::RIGHT);
    }

    void Base::_updateIndoorClimateBottom()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_INDOOR_BOTTOM, Y_END_POSITION_INDOOR_BOTTOM) {
            _drawIndoorClimateBottom();
        }
    }

    void Base::_drawForecast()
    {
        _canvas->setFont(FONTS_DEFAULT_BIG);
        _canvas->setTextColor(COLORS_RED);
        _canvas->drawTextAligned(TFT_WIDTH / 2, Y_START_POSITION_FORECAST + 5, F("FORECAST\nN/A"), AdafruitGFXExtension::CENTER);
    }

    using WeatherStation = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation;

    void Base::_drawWorldClocks(int16_t _offsetY)
    {
        auto cfg = WeatherStation::getConfig();
        uint8_t count = 0;
        for(uint8_t num = 0; num < WEATHER_STATION_MAX_CLOCKS; num++) {
            if (cfg.additionalClocks[num].isEnabled()) {
                count++;
            }
        }

        String prevTimezone = getenv(CStrP(F("TZ")));
        String tzStr;
        String timeStr;

        for(uint8_t num = 0; num < WEATHER_STATION_MAX_CLOCKS; num++) {
            __LDBG_printf("%02u(%u): %s, %s", num, cfg.additionalClocks[num].isEnabled() ? 1 : 0, WeatherStation::getTZ(num).c_str(), WeatherStation::getName(num).c_str());
            if (cfg.additionalClocks[num].isEnabled()) {

                // do not allow interrupts while changing the TZ
                {
                    InterruptLock lock;

                    safeSetTZ(WeatherStation::getTZ(num));
                    auto tm = localtime(&_lastTime);
                    tzStr = FormatTime::getTimeTZStr(_config.time_format_24h, tm);
                    timeStr = PrintString(F("%a %b %d %Y"), tm);
                    safeSetTZ(prevTimezone);
                }

                uint8_t yDistance = 5;
                _canvas->setTextColor(COLORS_WORLD_CLOCK_CITY);
                switch(count) {
                    case 1:
                        _canvas->setFont(FONTS_DEFAULT_BIG);
                        yDistance = 14;
                        break;
                    case 2:
                        _canvas->setFont(FONTS_DEFAULT_MEDIUM);
                        yDistance = 12;
                        break;
                    case 3:
                        _canvas->setFont(FONTS_DEFAULT_MEDIUM);
                        yDistance = 8;
                        break;
                    default:
                        _canvas->setFont(FONTS_DEFAULT_SMALL);
                        break;
                }
                _offsetY += _canvas->drawTextAligned(TFT_WIDTH / 2, _offsetY, WeatherStation::getName(num), AdafruitGFXExtension::CENTER) + 3;

                _canvas->setTextColor(COLORS_WORLD_CLOCK_TIME);
                _canvas->setFont(FONTS_DEFAULT_SMALL);
                _offsetY += _canvas->drawTextAligned(TFT_WIDTH / 2, _offsetY, timeStr, AdafruitGFXExtension::CENTER) + 1;

                _canvas->setTextColor(COLORS_WORLD_CLOCK_TIMEZONE);
                _offsetY += _canvas->drawTextAligned(TFT_WIDTH / 2, _offsetY, tzStr, AdafruitGFXExtension::CENTER) + yDistance;
            }

        }
    }

    void Base::_drawWorldClock()
    {
        int16_t offsetY;
        auto cfg = WeatherStation::getConfig();
        if (cfg.show_regular_clock_on_world_clocks) {
            _drawTime(false);
            offsetY = _canvas->getCursorY() + 8;
        }
        else {
            offsetY = Y_START_POSITION_TIME;
        }
        _drawWorldClocks(offsetY);
    }

    void Base::_drawInfo()
    {
        int16_t y = Y_START_POSITION_INFO + 5;
        constexpr uint8_t kIncr = 3;

        _canvas->setTextColor(COLORS_INFO_TITLE);
        _canvas->setFont(FONTS_DEFAULT_SMALL);
        y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, F("Hostname"), AdafruitGFXExtension::CENTER) + kIncr;
        _canvas->setTextColor(COLORS_INFO_DETAILS);
        _canvas->setFont(FONTS_DEFAULT_MEDIUM);
        y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, KFCConfigurationClasses::System::Device::getName(), AdafruitGFXExtension::CENTER) + 2 + kIncr;

        _canvas->setTextColor(COLORS_INFO_TITLE);
        _canvas->setFont(FONTS_DEFAULT_SMALL);
        y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, F("WiFi"), AdafruitGFXExtension::CENTER) + kIncr;
        _canvas->setTextColor(COLORS_INFO_DETAILS);
        _canvas->setFont(FONTS_DEFAULT_MEDIUM);
        if (WiFi.isConnected()) {
            auto ssid = WiFi.SSID();
            if (ssid.length() > 12) {
                _canvas->setFont(FONTS_DEFAULT_SMALL);
            }
            y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, ssid, AdafruitGFXExtension::CENTER) + kIncr;
            _canvas->setFont(FONTS_DEFAULT_MEDIUM);
            y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, WiFi.localIP().toString(), AdafruitGFXExtension::CENTER) + kIncr;
            y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, String(F("GW ")) + WiFi.gatewayIP().toString(), AdafruitGFXExtension::CENTER) + kIncr;
            y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, String(F("DNS ")) + WiFi.dnsIP(0).toString(), AdafruitGFXExtension::CENTER) + kIncr;
            _canvas->drawTextAligned(TFT_WIDTH / 2, y, WiFi.dnsIP(1).toString(), AdafruitGFXExtension::CENTER);
        }
        else {
            _canvas->setFont(FONTS_DEFAULT_SMALL);
            y += _canvas->drawTextAligned(TFT_WIDTH / 2, y, F("Trying to connect to"), AdafruitGFXExtension::CENTER);
            _canvas->drawTextAligned(TFT_WIDTH / 2, y, WiFi.SSID(), AdafruitGFXExtension::CENTER);
        }
    }

    // SCREEN METHODS

    void Base::_drawScreenMain()
    {
        _drawTime();
        _drawLocalWeather();
        _drawIndoorClimateBottom();
        _drawSunAndMoon();
    }

    void Base::_updateScreenMain()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_WEATHER, Y_END_POSITION_WEATHER) {
            _drawLocalWeather();
            _drawIndoorClimateBottom();
        }
    }

    void Base::_drawScreenIndoorClimate()
    {
        _drawTime();
        _drawIndoorClimate();
        _drawSunAndMoon();
    }

    void Base::_updateScreenIndoorClimate()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_INDOOR, Y_END_POSITION_INDOOR) {
            _drawIndoorClimate();
        }
    }

    void Base::_drawScreenForecast()
    {
        _drawTime();
        _drawForecast();
    }

    void Base::_updateScreenForecast()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_FORECAST, Y_END_POSITION_FORECAST) {
            _drawForecast();
        }
    }

    void Base::_drawScreenWorldClock()
    {
        _drawWorldClock();
    }

    void Base::_updateScreenWorldClock()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_TIME, Y_END_POSITION_WORLD_CLOCK) {
            _drawWorldClock();
        }
    }

    void Base::_drawScreenInfo()
    {
        _drawTime();
        _drawInfo();
    }

    void Base::_updateScreenInfo()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_INFO, Y_END_POSITION_INFO) {
            _drawInfo();
        }
    }

    void Base::_drawMoonPhase()
    {
        int16_t _offsetY = Y_START_POSITION_MOON_PHASE;

        auto moon = calcMoon(getUnixtimeForCalcMoon());
        time_t time = ::time(nullptr);

        // moon image
        _canvas->setFont(FONTS_MOON_PHASE);
        _canvas->setTextColor(COLORS_MOON_PHASE);
        auto y = _offsetY;
        _offsetY += _canvas->drawTextAligned(0, _offsetY, String(moon.moonPhaseFont), AdafruitGFXExtension::LEFT);
        auto x = _canvas->getCursorX() + 5;
        _offsetY += 15;

        // moon phase name and % position
        _canvas->setFont(&Dialog_6pt8b);
        _canvas->setTextColor(COLORS_MOON_PHASE_TOP);
        auto phaseY = _canvas->drawTextAligned(x, y, moon.moonPhaseName(), AdafruitGFXExtension::LEFT);
        y += phaseY;
        phaseY = y;
        y *= 2;
        y += 4;

        // time and timezone
        _canvas->setTextColor(COLORS_MOON_PHASE_TIME);
        PrintString timeStr = FormatTime::getTimeStr(_config.time_format_24h, time);
        timeStr += ' ';
        _canvas->drawTextAligned(x, y, timeStr, AdafruitGFXExtension::LEFT);

        _canvas->setTextColor(COLORS_MOON_PHASE_TIMEZONE);
        y += _canvas->drawTextAligned(_canvas->getCursorX(), y, FormatTime::getTimezoneStr(_config.time_format_24h, time), AdafruitGFXExtension::LEFT);

        // the previous, current and next 2 moon phases
        auto phases = calcMoonPhases(time);
        auto lastPhase = moon.pPhase - 0.25;

        for(auto phase: phases._timestamps) {
            auto moon = calcMoon(phase);

            // _canvas->setFont(&Dialog_6pt8b);

            if (isnan(lastPhase) || lastPhase >= moon.pPhase) {
                _canvas->setTextColor(COLORS_MOON_PHASE_NAME);
            }
            else {
                lastPhase = NAN;
                _canvas->setTextColor(COLORS_MOON_PHASE_ACTIVE);
            }

            _offsetY += _canvas->drawTextAligned(TFT_WIDTH / 2, _offsetY, moon.moonPhaseName(), AdafruitGFXExtension::CENTER);
            _offsetY += 2;

            // _canvas->setFont(&DejaVuSans_5pt8b);
            _canvas->setTextColor(COLORS_MOON_PHASE_DATETIME);
            _offsetY += _canvas->drawTextAligned(TFT_WIDTH / 2, _offsetY, FormatTime::getDateTimeStr(_config.time_format_24h, phase), AdafruitGFXExtension::CENTER);
            _offsetY += 4;
        }

        // moon phase name and %
        _canvas->setFont(&Dialog_6pt8b);
        _canvas->setTextColor(COLORS_MOON_PHASE_TOP);
        _canvas->drawTextAligned(x, phaseY + 2, PrintString(F("%.1f%% %.1f %s"), moon.moonPhaseIlluminationPct(), moon.moonDayDouble(), moon.moonDayDescr()), AdafruitGFXExtension::LEFT);

    }

    void Base::_updateMoonPhase()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_MOON_PHASE, Y_END_POSITION_MOON_PHASE) {
            _drawMoonPhase();
        }
    }

    void Base::_drawScreenPictures()
    {
        if (_galleryFile) {
            _galleryFile.seek(0);
            _drawJpegPicture(_galleryFile, _tft.height());
        }
    }

    void Base::_updateScreenPictures()
    {
        if (_galleryFile) {
            _galleryFile.seek(0);
            _drawJpegPicture(_galleryFile, 35);
        }
    }


    #if DEBUG_IOT_WEATHER_STATION

        void Base::printDebugInfo(Print &output)
        {
            _canvas->printf_P(PSTR("Unixtime " TIME_T_FMT "\nUptime %us\n"), time(nullptr), getSystemUptime());
            _canvas->printf_P(PSTR("Free Heap %u\n"), ESP.getFreeHeap(), getSystemUptime());
            _canvas->printf_P(PSTR("WiFi IP %s\nSSID %s (%d dBm)\n"), WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), WiFi.RSSI());
            _canvas->printf_P(PSTR("Avg. FPS %.3f\nPixels %.0f/s\nFrame time %.0fus\n"), 1000000.0 / _debugFPS, _debugPPS, _debugDrawTime);
        }

        void Base::_drawDebugInfo()
        {
            _canvas->setFont(FONTS_DEFAULT_SMALL);
            _canvas->setTextColor(COLORS_WHITE);
            _canvas->setCursor(0, 0);
            printDebugInfo(*_canvas);

            switch(_mode) {
                case DebugMode::ICON:
                    _icon = getIconFromProgmem(rand() % 9);
                    _pos = 0;
                    _mode = DebugMode::LEFT;
                    break;
                case DebugMode::LEFT:
                    if (++_pos >= TFT_WIDTH - GFXCanvas::getBitmapWidth(_icon) - 1) {
                        _mode = DebugMode::RIGHT;
                    }
                    break;
                case DebugMode::RIGHT:
                    if (--_pos == 0) {
                        _mode = DebugMode::ICON;
                    }
                    break;
            }

            _canvas->drawBitmap(_pos, _canvas->getCursorY(), _icon, palette);
        }

        void Base::_drawScreenDebug()
        {
            _drawDebugInfo();
        }

        void Base::_updateScreenDebug()
        {
            CLEAR_AND_DISPLAY(Y_START_POSITION_DEBUG, Y_END_POSITION_DEBUG) {
                _drawDebugInfo();
            };
        }

    #endif

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
                    canvasLine.setDirty(true);
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
        ScrollCanvas::destroy(this);
        if (clear) {
            _canvas->fillScreen(COLORS_BACKGROUND);
        }
        _canvas->setTextColor(color);
        _canvas->setFont(font);
        GFXCanvasCompressed::Position_t pos;
        if (clear) {
            _canvas->drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, GFXCanvasCompressed::CENTER, GFXCanvasCompressed::MIDDLE);
            pos.y = 0;
            pos.h = TFT_HEIGHT;
        }
        else {
            _canvas->drawTextAligned(TFT_WIDTH / 2, TFT_HEIGHT / 2, text, GFXCanvasCompressed::CENTER, GFXCanvasCompressed::MIDDLE, &pos);
        }
        if (!isLocked()) {
            _displayScreen(0, pos.y, TFT_WIDTH, pos.h);
        }
    }

    void Base::_draw()
    {
        if (_displayMessageTimer) {
            // __LDBG_printf("display_msg_timer=%u repeat=%u", _displayMessageTimer->getShortInterval(), _displayMessageTimer->_repeat);
            return;
        }
        if (_isScrolling()) {
            // __LDBG_printf("is_scrolling");
            return;
        }
        // __LDBG_printf("screen=%s num=%u", getScreenName(_currentScreen), _currentScreen);

        #if DEBUG_GFXCANVAS_STATS
            uint32_t start = micros();
        #endif

        if (_currentScreen == ScreenType::PICTURES) {
            _drawScreenPictures();
        }
        else {

            _canvas->fillScreen(COLORS_BACKGROUND);

            switch(_currentScreen) {
                case ScreenType::MAIN:
                    _drawScreenMain();
                    break;
                case ScreenType::INDOOR:
                    _drawScreenIndoorClimate();
                    break;
                case ScreenType::FORECAST:
                    _drawScreenForecast();
                    break;
                case ScreenType::INFO:
                    _drawScreenInfo();
                    break;
                case ScreenType::WORLD_CLOCK:
                    _drawWorldClock();
                    break;
                case ScreenType::MOON_PHASE:
                    _drawMoonPhase();
                    break;
                #if DEBUG_IOT_WEATHER_STATION
                    case ScreenType::DEBUG_INFO:
                        _drawScreenDebug();
                        break;
                #endif
                case ScreenType::TEXT_CLEAR:
                case ScreenType::TEXT_UPDATE:
                    _drawText(_text, _textFont, COLORS_DEFAULT_TEXT, true);
                    _currentScreen = ScreenType::TEXT;
                    break;
                case ScreenType::TEXT:
                    break;
                default:
                    break;
            }

            _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
        }

        #if DEBUG_GFXCANVAS_STATS
            uint32_t dur = micros() - start;
            static uint32_t time = 0;
            if (millis() - time > 5000) {
                time = millis();
                Serial.printf_P(PSTR("redraw %.3fms "), dur / 1000.0);
                _canvas->getDetails(Serial);
                stats.clear();
            }
        #endif
    }

    void Base::_displayScreen(int16_t x, int16_t y, int16_t w, int16_t h)
    {
        #if DEBUG_IOT_WEATHER_STATION
            if (_debugLastUpdate) {
                auto diff = micros() - _debugLastUpdate;
                float multiplier = diff > 0 ? ((1000U * 1000U) / diff) : 0;
                // _debugFPS = time in microseconds per frame
                _debugFPS = (_debugFPS * multiplier) + diff;
                _debugPPS = (_debugPPS * multiplier) + ((w - x) * (h - y));
                multiplier++;
                _debugFPS /= (multiplier);
                _debugPPS /= (multiplier);
            }
            uint32_t start = micros();
        #endif
        // __DBG_printf("x=%d y=%d w=%d h=%d", x, y, w, h);

        // copy canvas into tft memory
        _tft.startWrite();
        _tft.setAddrWindow(x, y, w, h);
        _canvas->drawInto(_tft, x, y, w, h);
        _tft.endWrite();

        #if DEBUG_IOT_WEATHER_STATION
            uint32_t dur = micros() -  start;
            _debugDrawTime = (_debugDrawTime + dur) / 2.0;
            _debugLastUpdate = micros();
        #endif

        #if WEATHER_STATION_HAVE_WEBUI_PREVIEW
            // promote full or partial update
            canvasUpdatedEvent(x, y, w, h);
        #endif
    }

    String Base::_getTemperature(float value, bool kelvin)
    {
        if (_config.is_metric) {
            return String(kelvin ? OpenWeatherMapAPI::kelvinToC(value) : value, 1) + '\xb0' + 'C';
        }
        else {
            return String(kelvin ? OpenWeatherMapAPI::kelvinToF(value) : (value * 1.8f + 32.0f), 1) + '\xb0' + 'F';
        }
    }

    // draws the date, time and timezone at the top
    // the area must be cleared before
    void Base::_drawTime(bool displayTZ)
    {
        constexpr int16_t _offsetY = 0;

        _lastTime = time(nullptr);
        auto tm = localtime(&_lastTime);

        _canvas->setFont(FONTS_DATE);
        _canvas->setTextColor(COLORS_DATE);
        PrintString timeStr;
        timeStr.strftime(F("%a %b %d %Y"), tm);
        _canvas->drawTextAligned(X_POSITION_DATE, Y_POSITION_DATE, timeStr.c_str(), H_POSITION_DATE);

        _canvas->setFont(displayTZ ? FONTS_TIME : FONTS_DEFAULT_MEDIUM);
        _canvas->setTextColor(COLORS_TIME);

        _canvas->drawTextAligned(X_POSITION_TIME + (displayTZ ? 0 : (_config.time_format_24h ? -17 : -28)), Y_POSITION_TIME, FormatTime::getTimeStr(_config.time_format_24h, tm).c_str(), H_POSITION_TIME);

        _canvas->setTextColor(COLORS_TIMEZONE);
        auto timeZoneStr = FormatTime::getTimezoneStr(_config.time_format_24h, tm);
        if (displayTZ) {
            _canvas->setFont(FONTS_TIMEZONE);
            _canvas->drawTextAligned(X_POSITION_TIMEZONE, Y_POSITION_TIMEZONE, timeZoneStr.c_str(), H_POSITION_TIMEZONE);
        }
        else {
            _canvas->drawTextAligned(_canvas->getCursorX() + 4, Y_POSITION_TIME, timeZoneStr.c_str(), AdafruitGFXExtension::LEFT);
        }
    }

    bool Base::tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
    {
        return WeatherStationBase::_getInstance()._tftOutput(x, y, w, h, bitmap);
    }

    bool Base::_tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
    {
        if (y >= _tftOutputMaxHeight) {
            return false;
        }
        auto tmp = GFXcanvas16(w, h);
        memcpy(tmp.getBuffer(), bitmap, w * h * 2);

        constexpr int16_t kWidth = 120;
        constexpr int16_t kHeight = 35;
        constexpr int16_t kTop = 5;
        constexpr int16_t kLeft = 2;

        if (!_tftOverlayCanvas) {
            _lastTime = time(nullptr);
            auto tm = localtime(&_lastTime);
            PrintString timeStr;
            timeStr.strftime(F("%a, %b %d"), tm);

            _tftOverlayCanvas = new GFXcanvas1(kWidth, kHeight);
            _tftOverlayCanvas->setFont(FONTS_DATE);
            _tftOverlayCanvas->setCursor(kLeft + 3, kTop * 2);
            _tftOverlayCanvas->print(timeStr);
            _tftOverlayCanvas->setFont(FONTS_DEFAULT_MEDIUM);
            _tftOverlayCanvas->setCursor(kLeft + 70, kTop + 25);
            _tftOverlayCanvas->print(_getTemperature(_getIndoorValues().getTemperature()));
            _tftOverlayCanvas->setFont(FONTS_TIME);
            _tftOverlayCanvas->setCursor(kLeft, kTop + 26);
            _tftOverlayCanvas->print(FormatTime::getShortTimeStr(_config.time_format_24h, tm));
        }

        if (x < kWidth && y < kHeight) {
            auto nw = std::min<uint16_t>(kWidth - x, w);
            auto nh = std::min<uint16_t>(kHeight - y, h);
            auto tmp2 = GFXcanvas1(nw, nh);
            auto dst = tmp2.getBuffer();
            auto src = &_tftOverlayCanvas->getBuffer()[(x / 8) + (y * ((kWidth + 7) / 8))];
            for(int16_t i = 0; i < nh; i++) {
                memcpy(dst, src, (nw + 7) / 8);
                dst += (nw + 7) / 8;
                src += (kWidth + 7) / 8;
            }
            tmp.drawBitmap(2, 2, tmp2.getBuffer(), nw, nh, COLORS_BLACK);
            tmp.drawBitmap(1, 1, tmp2.getBuffer(), nw, nh, COLORS_BLACK);
            tmp.drawBitmap(0, 0, tmp2.getBuffer(), nw, nh, COLORS_WHITE);
        }

        _tft.drawRGBBitmap(x, y, tmp.getBuffer(), w, h);
        return true;
    }

    void Base::_drawJpegPicture(File filename, uint16_t height)
    {
        _tftOutputMaxHeight = height ? height : _tft.height();
        auto TJpgDecPtr = new TJpg_Decoder(tft_output);
        if (TJpgDecPtr) {
            TJpgDecPtr->drawFsJpg(0, 0, filename);
            delete TJpgDecPtr;
            stdex::reset(_tftOverlayCanvas);
        }
        else {
            __DBG_printf("out of memory");
        }
    }

    void Base::_updateScreenTime()
    {
        CLEAR_AND_DISPLAY(Y_START_POSITION_TIME, Y_END_POSITION_TIME) {
            _drawTime();
        }
    }

    const __FlashStringHelper *Base::getScreenName(ScreenType screen)
    {
        switch(screen) {
            case ScreenType::MAIN:
                return F("Local Weather");
            case ScreenType::INDOOR:
                return F("Indoor Climate");
            case ScreenType::FORECAST:
                return F("Weather Forecast");
            case ScreenType::WORLD_CLOCK:
                return F("World Clock");
            case ScreenType::MOON_PHASE:
                return F("Moon Phase");
            case ScreenType::INFO:
                return F("System Info");
            case ScreenType::PICTURES:
                return F("Gallery");
            #if DEBUG_IOT_WEATHER_STATION
                case ScreenType::DEBUG_INFO:
                    return F("Debug Info");
                case ScreenType::NUM_SCREENS:
                case ScreenType::TEXT_CLEAR:
                case ScreenType::TEXT_UPDATE:
                case ScreenType::TEXT:
                    __DBG_panic("Invalid screen %u", screen);
                    break;
            #else
                default:
                    break;
            #endif
        }
        return nullptr;
    }

}
