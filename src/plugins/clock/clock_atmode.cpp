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

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKBR, "BR", "Set brightness (0-65535). 0 disables the LEDs, > 0 enables them");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKPX, "PX", "[<number|-1=all>,<#RGB>|<r>,<g>,<b>]", "Set level of a single pixel. No arguments turns all off");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKP, "P", "<00[:.]00[:.]00>", "Display strings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKC, "C", "<#RGB>|<r>,<g>,<b>", "Set color");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKM, "M", "<value>[,<incr>,<min>,<max>]", "Set rainbow animation multiplier");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKT, "T", "<value>", "Override temperature");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKTS, "TS", "<num>,<segment>", "Set segment for digit <num>");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CLOCKA, "A", "<num>[,<arguments>,...]", "Set animation", "Display available animations");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKD, "D", "Dump pixel addresses and other information");

ATModeCommandHelpArrayPtr ClockPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKBR),
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
        enableLoop(false);
        if (args.size() < 1) {
            _display.clear();
            _display.show();
            args.print(F("display cleared"));
        }
        else {
            auto text = args.get(0);
            args.printf_P(PSTR("display '%s'"), text);
            _display.print(text, _getColor());
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
#if IOT_LED_MATRIX
            args.printf_P(PSTR("%u - fire (+CLOCKA=%u)"), AnimationType::FIRE, AnimationType::FIRE);
            args.printf_P(PSTR("%u - fire (+CLOCKA=%u)"), AnimationType::SKIP_ROWS, AnimationType::SKIP_ROWS);
#else
            args.printf_P(PSTR("%u - blink colon speed"), (int)AnimationType::MAX);
            args.print(F("100 = disable clock"));
            args.print(F("101 = enable clock"));
            args.print(F("200 = display ambient light sensor value (+CLOCKA=200,<0|1>)"));
#endif
            args.print(F("300 = test pixel animation order"));
            args.print(F("400 = get/set update rate"));
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
                enableLoop(false);
                _display.reset();
            }
            else if (value == 101) {
                enableLoop(true);
                _display.reset();
            }
            else if (value == 300) {
                int interval = args.toInt(1, 500);
                enableLoop(false);
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
#if !IOT_LED_MATRIX && IOT_CLOCK_AMBIENT_LIGHT_SENSOR
            else if (value == 200) {
                if (args.isTrue(1)) {
                    if (_displaySensor == DisplaySensorType::OFF) {
                        _autoBrightness = kAutoBrightnessOff;
                        _displaySensor = DisplaySensorType::SHOW;
                        _display.clear();
                        _display.show();
                        setUpdateRate(500);
                        args.print(F("displaying sensor value"));
                    }
                }
                else if (_displaySensor == DisplaySensorType::SHOW) {
                    _displaySensor = DisplaySensorType::OFF;
                    _autoBrightness = _config.auto_brightness;
                    if (_animation) {
                        setUpdateRate(_animation->getUpdateRate());
                    }
                    else {
                        setAnimation(AnimationType::NONE);
                    }
                    args.print(F("displaying time"));
                }
            }
#endif
            else if (value == 400) {
                _updateRate = args.toIntMinMax<uint16_t>(0, 5, 1000, _updateRate);
                args.printf_P(PSTR("update rate: %u"), _updateRate);
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
                    case AnimationType::FIRE:
                        _setAnimation(__LDBG_new(Clock::FireAnimation, *this, _config.fire));
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
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKBR))) {
        if (args.requireArgs(1, 2)) {
            auto brightness = args.toIntMinMax<uint16_t>(0, 0, Clock::kMaxBrightness);
            auto time = args.toMillis(1, 0, 60000, 10000);
            setBrightness(brightness, time);
            args.printf_P("fading brightness to %.2f%% (%u) in %.3f seconds", brightness / (float)Clock::kMaxBrightness * 100.0, brightness, time / 1000.0);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKC))) {
        if (args.size() == 1) {
            setColor(Color::fromString(args.toString(0)));
        }
        else if (args.requireArgs(3, 3)) {
            setColor(Color(args.toNumber(0, 0), args.toNumber(1, 0), args.toNumber(2, 0x80)));
        }
        args.printf_P(PSTR("set color %s"), getColor().toString().c_str());
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKPX))) {
        if (args.size() == 0) {
            enableLoop(false);
            _display.clear();
            _display.show();
            args.print(F("display cleared"));
        }
        else if (args.size() == 2 || args.requireArgs(4, 4)) {
            enableLoop(false);
            int num = args.toInt(0);
            Color color;
            if (args.size() == 2) {
                color = Color::fromString(args.toString(1));
            }
            else {
                color = Color(args.toNumber(1, 0), args.toNumber(2, 0), args.toNumber(3, 0x80));
            }
            if (num == -1) {
                args.printf_P(PSTR("pixel=0-%u, color=%s"), _display.getTotalPixels(), color.toString().c_str());
                _display.setColor(color);
                _display.show();
            }
            else if (num >= 0) {
                args.printf_P(PSTR("pixel=%u, color=%s"), num, color.toString().c_str());
                _display._pixels[num] = color;
                _display.show();
            }
            else {
                args.print(F("invalid pixel number"));
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKT))) {
        _tempOverride = args.toIntMinMax<uint8_t>(0, kMinimumTemperatureThreshold, 255, 0);
        if (_tempOverride) {
            args.printf_P(PSTR("temperature override %u°C"), _tempOverride);
        }
        else {
            args.print(F("temperature override disabled"));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKD))) {
        _display.dump(args.getStream());
#if IOT_CLOCK_DEBUG_ANIMATION_TIME
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
#endif
        return true;
    }
    return false;
}

#endif
