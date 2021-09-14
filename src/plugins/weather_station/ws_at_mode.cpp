/**
 * Author: sascha_lammers@gmx.de
 */

#if AT_MODE_SUPPORTED

#include "weather_station.h"
#include "at_mode.h"

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSSET, "WSSET", "<touchpad|timeformat24h|metrics|tft|scroll|stats|lock|unlock>,<on|off|options>", "Enable/disable function");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSBL, "WSBL", "<level=0-1023>", "Set backlight level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSU, "WSU", "<i|f>", "Update weather info/forecast");

ATModeCommandHelpArrayPtr WeatherStationPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray PROGMEM tmp = {
        PROGMEM_AT_MODE_HELP_COMMAND(WSSET),
        PROGMEM_AT_MODE_HELP_COMMAND(WSBL),
        PROGMEM_AT_MODE_HELP_COMMAND(WSU)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool WeatherStationPlugin::atModeHandler(AtModeArgs &args)
{
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSSET))) {
        if (args.requireArgs(1, 2)) {
            bool state = !args.isFalse(1);
            if (args.equalsIgnoreCase(0, F("touchpad"))) {
                _touchpadDebug = state;
                args.printf_P(PSTR("touchpad debug=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, F("timeformat24h"))) {
                _config.time_format_24h = state;
                redraw();
                args.printf_P(PSTR("time format 24h=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, FSPGM(metrics))) {
                _config.is_metric = state;
                redraw();
                args.printf_P(PSTR("metrics=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, F("screen"))) {
                _setScreen((_getCurrentScreen() + 1) % kNumScreens);
                redraw();
                args.printf_P(PSTR("screen=%u"), _currentScreen);
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
            else if (args.equalsIgnoreCase(0, F("dump"))) {
                BufferPool::getInstance().dump(args.getStream());
            }
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
                args.print(F("locked=%d deattached heap=%d"), isLocked(), mem - (int)ESP.getFreeHeap());
            }
            #if WSDRAW_STATS
                else if (args.equalsIgnoreCase(0, F("stats"))) {
                    if (args.equalsIgnoreCase(1, F("print"))) {
                        for(const auto &item: _stats) {
                            PrintString str;
                            str.printf_P(PSTR("%s: "), item.first.c_str());
                            auto iterator = item.second.begin();
                            while(iterator != item.second.end()) {
                                str.printf_P(PSTR("%.2f "), *iterator);
                                ++iterator;
                            }
                            args.print(str.c_str());
                        }
                    }
                    else {
                        _debug_stats = state;
                    }
                }
            #endif
            else {
                args.print("Invalid type");
            }
        }
        return true;
    }
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSBL))) {
        if (args.requireArgs(1, 1)) {
            uint16_t level = args.toIntMinMax<uint16_t>(0, 0, 1023);
            _fadeBacklight(_backlightLevel, level);
            _backlightLevel = level;
            args.printf_P(PSTR("backlight=%d"), level);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSU))) {
        if (args.equals(0, 'f')) {
            // args.print(F("Updating forecast..."));
            // _getWeatherForecast([this, args](bool status) mutable {
            //     args.printf_P(SPGM(status__u), status);
            //     redraw();
            // });
        }
        else {
            // args.print(F("Updating info..."));
            // _getWeatherInfo([this, args](bool status) mutable {
            //     args.printf_P(SPGM(status__u), status);
            //     redraw();
            // });
        }
        return true;
    }
    return false;
}

#endif
