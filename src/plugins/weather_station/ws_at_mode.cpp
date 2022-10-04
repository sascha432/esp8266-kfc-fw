/**
 * Author: sascha_lammers@gmx.de
 */

#if AT_MODE_SUPPORTED

#include "weather_station.h"
#include "moon_phase.h"
#include "format_time.h"
#include "at_mode.h"
#include "moon/moontool.h"


#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSSET, "WSSET", "<touchpad|timeformat24h|metric|tft|scroll|stats|lock|unlock|screen|screens>,<on|off|options>", "Enable/disable function");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSBL, "WSBL", "<level=0-1023>", "Set backlight level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSU, "WSU", "<i|f>", "Update weather info/forecast");
#if DEBUG_MOON_PHASE
    PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSM, "WSM", "<date YYYY-MM-DD>[,<days>]", "Show Moon Phase for given Date");
#endif

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr WeatherStationPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray PROGMEM tmp = {
        PROGMEM_AT_MODE_HELP_COMMAND(WSSET),
        PROGMEM_AT_MODE_HELP_COMMAND(WSBL),
        PROGMEM_AT_MODE_HELP_COMMAND(WSU)
        #if DEBUG_MOON_PHASE
            , PROGMEM_AT_MODE_HELP_COMMAND(WSM)
        #endif
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool WeatherStationPlugin::atModeHandler(AtModeArgs &args)
{
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSSET))) {
        if (args.requireArgs(1)) {
            bool state = !args.isFalse(1);
            #if HAVE_GFX_READLINE
            if (args.equalsIgnoreCase(0, F("rwtest"))) {//wsset rwtest
                LoopFunctions::callOnce([this]() {
                    _canvas->writeLine(0, 0, TFT_WIDTH, 0, COLORS_RED);
                    _canvas->writeLine(0, 1, TFT_WIDTH, 1, COLORS_BLUE);
                    _displayScreen(0, 0, TFT_WIDTH, TFT_HEIGHT);
                    uint16_t buf[TFT_WIDTH];
                    std::fill(std::begin(buf), std::end(buf), 0xcf);
                    uint32_t start = micros();
                    _canvas->readLine(0, buf);
                    uint32_t dur = micros() - start;
                    Serial.printf_P(PSTR("readLine %uus\n"), dur);
                    Serial.flush();
                    delay(10);
                    for(uint16_t len = 0; len < TFT_WIDTH; len++) {
                        Serial.printf_P(PSTR("%02x "), buf[len]);
                        delay(1);
                    }
                    Serial.println();
                    _canvas->readLine(1, buf);
                    for(uint16_t len = 0; len < TFT_WIDTH; len++) {
                        Serial.printf_P(PSTR("%02x "), buf[len]);
                        delay(1);
                    }
                    Serial.println();
                });
            }
            else
            #endif
            #if IOT_WEATHER_STATION_TOUCHPAD_DEBUG
                if (args.equalsIgnoreCase(0, F("touchpad"))) {
                    _touchpadDebug = state;
                    args.printf_P(PSTR("touchpad debug=%u"), state);
                }
                else
            #endif
            if (args.equalsIgnoreCase(0, F("timeformat24h"))) {
                _config.time_format_24h = state;
                redraw();
                args.printf_P(PSTR("time format 24h=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, F("metric"))) {
                _config.is_metric = state;
                redraw();
                args.printf_P(PSTR("metric=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, F("screen"))) {
                auto timeout = args.toIntMinMax(2, -1, 300, -1);
                _setScreen(args.toIntMinMax<uint8_t>(1, 0, kNumScreens - 1,_getCurrentScreen() + 1), timeout);
                args.printf_P(PSTR("screen=%u timeout=%ds"), _currentScreen, timeout);
            }
            else if (args.equalsIgnoreCase(0, F("screens"))) {
                for(uint8_t n = 0; n < kNumScreens; n++) {
                    args.print(F("screen=%u name=%s timeout=%us valid=%u screen=%u prev=%u next=%u"),
                        n, getScreenName(n), _config.screenTimer[n], _config.screenTimer[n] != 255,
                        _getScreen(n, true),
                        _getPrevScreen(n, true),
                        _getNextScreen(n, true)
                    );
                }
            }
            else if (args.equalsIgnoreCase(0, F("tft"))) {
                _tft.initR(INITR_BLACKTAB);
                _tft.fillScreen(state ? COLORS_YELLOW : COLORS_RED);
                _tft.setRotation(0);
                args.printf_P(PSTR("set color"));
            }
            // else if (args.equalsIgnoreCase(0, F("scroll"))) {
            //     if (!_isScrolling()) {
            //         _doScroll();
            //         _drawEnvironmentalSensor(_scrollCanvas->getCanvas(), 0);
            //         _Scheduler.add(20, true, [this](Event::CallbackTimerPtr timer) {
            //             _scrollTimer(*this);
            //         });
            //     }
            // }
            else if (args.equalsIgnoreCase(0, F("text"))) {
                setText(args.get(1), FONTS_DEFAULT_MEDIUM);
                _setScreen(ScreenType::TEXT_CLEAR);
            }

            // else if (args.equalsIgnoreCase(0, F("dump"))) {
            //     BufferPool::getInstance().dump(args.getStream());
            // }
            else if (args.equalsIgnoreCase(0, F("lock"))) {
                int mem = ESP.getFreeHeap();
                bool state = lock();
                args.print(F("locked=%d %s heap=%d"), isLocked(), state ? PSTR("locked") : PSTR("locking failed"), mem - (int)ESP.getFreeHeap());
            }
            else if (args.equalsIgnoreCase(0, F("unlock"))) {
                int mem = ESP.getFreeHeap();
                if (isLocked()) {
                    unlock();
                }
                args.print(F("locked=%d detached heap=%d"), isLocked(), mem - (int)ESP.getFreeHeap());
            }
            else {
                args.print("Invalid type");
            }
        }
        return true;
    }
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSBL))) {
        if (args.requireArgs(1, 1)) {
            uint16_t level = args.toIntMinMax<uint16_t>(0, 0, PWMRANGE);
            _fadeBacklight(_backlightLevel, level, 1);
            _backlightLevel = level;
            args.printf_P(PSTR("backlight=%u/%u"), level, PWMRANGE);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSU))) {
        auto stream = &args.getStream();
        if (args.equals(0, 'f')) {
            args.print(F("Updating forecast..."));
            _getWeatherForecast([this, stream](bool status, KFCRestAPI::HttpRequest &request) {
                auto msg = request.getMessage();
                LoopFunctions::callOnce([this, status, msg, stream]() {
                    stream->printf_P(PSTR("+WSU status=%u msg=%s\n"), status, msg.c_str());
                    redraw();
                });
            });
        }
        else {
            args.print(F("Updating info..."));
            _getWeatherInfo([this, stream](bool status, KFCRestAPI::HttpRequest &request) {
                auto msg = request.getMessage();
                LoopFunctions::callOnce([this, status, msg, stream]() {
                    stream->printf_P(PSTR("+WSU status=%u msg=%s\n"), status, msg.c_str());
                    redraw();
                });
            });
        }
        return true;
    }
    #if DEBUG_MOON_PHASE
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSM))) {
            time_t unixtime = 0;
            uint32_t days = args.toInt(1, 1);
            uint32_t incr = args.toInt(2, 86400);
            if (args.startsWithIgnoreCase(0, F("list"))) {
                auto &stream = args.getStream();
                unixtime = time(nullptr);

                stream.println();
                stream.println(F("date,unixtime,phase"));

                time_t lastPhase = 0;
                for(float i = 0; i < days; i += MoonPhaseType::kMoonDay) {
                    auto phases = calcMoonPhases(unixtime + (i * incr));
                    for(auto phase: phases._timestamps) {
                        if (phase <= lastPhase) {
                            continue;
                        }
                        lastPhase = phase;
                        auto moon = calcMoon(phase);
                        PrintString dateStr;
                        dateStr.strftime_P(PSTR("%Y-%m-%d %H:%M"), phase);
                        stream.printf_P(PSTR("%s," TIME_T_FMT ",%s\n"), dateStr.c_str(), phase, moonPhaseName(moon.pPhase));
                    }
                }
                stream.println();
                return true;
            }
            else if (args.startsWithIgnoreCase(0, F("ph"))) {
                unixtime = -1;
                auto results = calcMoonPhases(time(nullptr));
                for(uint8_t i = 0; i < 5; i++) {
                    args.print(F("%u %s %s"), (uint32_t)results._timestamps[i], FormatTime::getDateTimeStr(true, results._timestamps[i]).c_str(), moonPhaseName(static_cast<uint8_t>((i * 2) % 8)));
                }
            }
            else if (args.startsWithIgnoreCase(0, F("now"))) {
                unixtime = getUnixtimeForCalcMoon();
            }
            else {
                StringVector items;
                explode_P(args.toString(0).c_str(), PSTR("-./"), items, 3);
                if (items.size() == 3) {
                    struct tm tm = {};
                    tm.tm_year = items[0].toInt() - 1900;
                    tm.tm_mon = items[1].toInt() - 1;
                    tm.tm_mday = items[2].toInt();
                    tm.tm_hour = 11;
                    tm.tm_min = 59;
                    tm.tm_sec = 59;
                    unixtime = mktime(&tm);
                }
            }
            if (unixtime == -1) {
                // dont show
            }
            else if (unixtime) {
                while(days--) {
                    auto moon = calcMoon(unixtime);

                    args.print(F("%s mAge=%f pPhase=%f(%s) il=%.3f(%.2f%%) mFont=%c"),
                        FormatTime::getDateTimeStr(true, moon.unixTime).c_str(),
                        moon.mAge,
                        moon.pPhase,
                        moon.moonPhaseName(),
                        moon.moonPhaseIllumination(),
                        moon.moonPhaseIlluminationPct(),
                        moon.moonPhaseFont
                    );
                    unixtime += incr;
                }
            }
            else {
                args.print(F("Invalid date, the format is YYYY-MM-DD"));
            }
            return true;
        }
    #endif
    return false;
}

#endif
