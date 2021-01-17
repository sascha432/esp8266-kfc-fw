/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#if IOT_LED_MATRIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "LM"
#else
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "CLOCK"
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKPX, "PX", "<led>,<r>,<g>,<b>", "Set level of a single pixel");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKP, "P", "<00[:.]00[:.]00>", "Display strings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKC, "C", "<r>,<g>,<b>", "Set color");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKM, "M", "<value>[,<incr>,<min>,<max>]", "Set rainbow animation multiplier");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKT, "T", "<value>", "Overide temperature");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKTS, "TS", "<num>,<segment>", "Set segment for digit <num>");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CLOCKA, "A", "<num>[,<arguments>,...]", "Set animation", "Display available animations");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKD, "D", "Dump pixel addresses and other information");

ATModeCommandHelpArrayPtr ClockPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKPX),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKP),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKC),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKM),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKT),
        // PROGMEM_AT_MODE_HELP_COMMAND(CLOCKTS),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKA),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKD)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool ClockPlugin::atModeHandler(AtModeArgs &args)
{
    // if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKTS))) {
    //     if (args.size() == 2) {
    //         enable(false);
    //         uint8_t digit = args.toInt(0);
    //         uint8_t segment = args.toInt(1);
    //         args.printf_P(PSTR("digit=%u segment=%c color=#%06x"), digit, _display.getSegmentChar(segment), _color.get());
    //         _display.setSegment(digit, segment, _color);
    //         _display.show();
    //     }
    //     return true;
    // }
    // else
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKP))) {
        enable(false);
        if (args.size() < 1) {
            args.print(F("clear"));
            _display.clear();
            _display.show();
        }
        else {
            auto text = args.get(0);
            args.printf_P(PSTR("'%s'"), text);
            _display.print(text, _color);
            _display.show();
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKM))) {
        _config.rainbow.multiplier.value = args.toFloatMinMax(0, 0.1f, 100.0f, _config.rainbow.multiplier.value);
        args.printf_P(PSTR("Rainbow multiplier=%f increment=%f min=%f max=%f"), _config.rainbow.multiplier.value, _config.rainbow.multiplier.incr, _config.rainbow.multiplier.min, _config.rainbow.multiplier.max);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKA))) {
        if (args.isQueryMode()) {
            args.printf_P(PSTR("%u - rainbow animation (+CLOCKA=%u,<speed>,<multiplier>,<r>,<g>,<b-factor>)"), AnimationType::RAINBOW, AnimationType::RAINBOW);
            args.printf_P(PSTR("%u - flashing"), AnimationType::FLASHING);
            args.printf_P(PSTR("%u - fade to color (+CLOCKA=%u,<r>,<g>,<b>)"), AnimationType::FADING, AnimationType::FADING);
#if !IOT_LED_MATRIX
            args.printf_P(PSTR("%u - blink colon speed"), (int)AnimationType::MAX);
#endif
            args.print(F("100 = disable clock"));
            args.print(F("101 = enable clock"));
            args.print(F("102 = all pixels on"));
            args.print(F("103 = test pixel animation order"));
#if !IOT_LED_MATRIX
            args.print(F("200 = display ambient light sensor value (+CLOCKA=200,<0|1>)"));
#endif
            args.print(F("300 = temperature protection animation"));
        }
        else if (args.size() >= 1) {
            int value = args.toInt(0);
#if !IOT_LED_MATRIX
            if (value == (int)AnimationType::MAX) {
                setBlinkColon(args.toIntMinMax(1, 50U, 0xffffU, 1000U));
            }
            else
#endif
            if (value == 100) {
                enable(false);
                _display.reset();
            }
            else if (value == 101) {
                _display.reset();
                enable(true);
            }
            else if (value == 102) {
                enable(false);
                _display.reset();
                _display.setColor(0xffffff);
                _display.show();
            }
            else if (value == 103) {
                int interval = args.toInt(1, 500);
                enable(false);
                size_t num = 0;
                _Scheduler.add(interval, true, [num, this](Event::CallbackTimerPtr timer) mutable {
                    __LDBG_printf("pixel=%u", num);
#if IOT_LED_MATRIX
                    if (num == _display.getTotalPixels()) {
#else
                    if (num == _pixelOrder.size()) {
#endif
                        _display.clear();
                        _display.show();
                        timer->disarm();
                    }
                    else {
#if IOT_LED_MATRIX
                        if (num) {
                            _display._pixels[num - 1] = 0;
                        }
                        _display._pixels[num++] = 0x22;
#else
                        if (num) {
                            _display._pixels[_pixelOrder[num - 1]] = 0;
                        }
                        _display._pixels[_pixelOrder[num++]] = 0x22;
#endif
                    }
                });
            }
#if !IOT_LED_MATRIX
            else if (value == 200) {
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
                if (args.isTrue(1)) {
                    if (_displaySensorValue == 0) {
                        _autoBrightness = kAutoBrightnessOff;
                        _displaySensorValue = 1;
                        _display.clear();
                        _display.show();
                        setUpdateRate(500);
                        args.print(F("displaying sensor value"));
                    }
                }
                else if (_displaySensorValue != 0) {
                    _displaySensorValue = 0;
                    _autoBrightness = _config.auto_brightness;
                    if (_animation) {
                        setUpdateRate(_animation->getUpdateRate());
                    }
                    else {
                        setAnimation(AnimationType::NONE);
                    }
                    args.print(F("displaying time"));
                }
#else
                args.print(F("sensor not supported"));
#endif
            }
#endif
            else if (value == 300) {
                _startTempProtectionAnimation();
            }
            else if (value == 400) {
                _updateRate = args.toInt(1);
            }
            else if (value >= 0 && value < (int)AnimationType::MAX) {
                switch(static_cast<AnimationType>(value)) {
                    case AnimationType::RAINBOW:
                        _config.rainbow.speed = args.toIntMinMax(1, ClockPlugin::kMinRainbowSpeed, (uint16_t)0xfffe, _config.rainbow.speed);
                        _config.rainbow.multiplier.value = args.toFloatMinMax(2, 0.1f, 100.0f, _config.rainbow.multiplier.value);
                        _config.rainbow.color.factor.red = (uint8_t)args.toInt(3, _config.rainbow.color.factor.red);
                        _config.rainbow.color.factor.green = (uint8_t)args.toInt(4, _config.rainbow.color.factor.green);
                        _config.rainbow.color.factor.blue = (uint8_t)args.toInt(5, _config.rainbow.color.factor.blue);
                        _setAnimation(__LDBG_new(Clock::RainbowAnimation, *this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.color));
                        break;
                    default:
                        setAnimation(static_cast<AnimationType>(value));
                        break;
                }
                // if (value == AnimationType::FADE) {
                //     _animationData.fade.speed = args.toIntMinMax(1, 5U, 1000U, 10U);
                //     if (args.size() >= 5) {
                //         _animationData.fade.toColor = Color(args.toInt(2, rand()), args.toInt(3, rand()), args.toInt(4, rand()));
                //     }
                //     else {
                //         _animationData.fade.toColor = ~_color & 0xffffff;
                //     }
                // }
                // else if (value == AnimationType::RAINBOW) {
                //     if (args.size() >= 2) {
                //         _config.rainbow.multiplier = args.toFloatMinMax(1, 0.01f, 100.0f);
                //         if (args.size() >= 3) {
                //             _config.rainbow.speed = args.toIntMinMax(2, 5, 255);
                //         }
                //     }
                // }
                // else if (value == AnimationType::FLASHING) {
                // }
                // setAnimation(static_cast<AnimationType>(value));
            }
        }
#if !IOT_LED_MATRIX
        else {
            setBlinkColon(0);
        }
#endif
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKC))) {
        if (args.size() == 1) {
            _color = Color::fromBGR(args.toNumber(0, 0xff0000));
        }
        else if (args.requireArgs(3, 3)) {
            _color = Color(args.toNumber(0, 0), args.toNumber(1, 0), args.toNumber(2, 0x80));
        }
        args.printf_P(PSTR("color=#%06x"), _color.get());
        for(size_t i = 0; i  < SevenSegmentDisplay::getTotalPixels(); i++) {
            if (_display._pixels[i]) {
                _display._pixels[i] = _color;
            }
        }
        _display.show();
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKPX))) {
        if (args.requireArgs(4, 4)) {
            enable(false);

            int num = args.toInt(0);
            Color color(args.toIntT<uint8_t>(1), args.toIntT<uint8_t>(2), args.toIntT<uint8_t>(3));
            if (num < 0) {
                args.printf_P(PSTR("pixel=0-%u, color=#%06x"), _display.getTotalPixels(), color.get());
                _display.setColor(color);
            }
            else {
                args.printf_P(PSTR("pixel=%u, color=#%06x"), num, color.get());
                _display._pixels[num] = color;
            }
            _display.show();
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKT))) {
        // if (args.size() > 0) {
            _tempOverride = args.toIntMinMax<uint8_t>(0, kMinimumTemperatureThreshold, 255, 0);
        // }
        // else {
            // _tempOverride = 0;
        // }
        args.printf_P(PSTR("temp override %u"), _tempOverride);
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKD))) {
        _display.dump(args.getStream());
        __DBGTM(
            args.print(F("animation loop times (microseconds):"));
            PrintString str;
            uint8_t n = 0;
            for(const auto time: _anmationTime) {
                str.printf_P(PSTR("%05u "), time);
                if (++n % 5 == 0) {
                    str.print('\n');
                }
            }
            str.rtrim();
            args.print(str);

            args.print(F("animation render times (microseconds):"));
            str = PrintString();
            n = 0;
            for(const auto time: _animationRenderTime) {
                str.printf_P(PSTR("%05u "), time);
                if (++n % 5 == 0) {
                    str.print('\n');
                }
            }
            str.rtrim();
            args.print(str);

            args.print(F("display times (microseconds):"));
            str = PrintString();
            n = 0;
            for(const auto time: _displayTime) {

                str.printf_P(PSTR("%05u "), time);
                if (++n % 5 == 0) {
                    str.print('\n');
                }
            }
            str.rtrim();
            args.print(str);

            args.printf_P(PSTR("timer update rate %u (milliseconds):"), _updateRate);
            str = PrintString();
            n = 0;
            for(const auto time: _timerDiff) {
                str.printf_P(PSTR("%05u "), time);
                if (++n % 5 == 0) {
                    str.print('\n');
                }
            }
            str.rtrim();
            args.print(str);
        );
        return true;
    }
    return false;
}

#endif
