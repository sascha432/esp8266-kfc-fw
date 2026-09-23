/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <web_socket.h>
#include <NeoPixelEx.h>
#include "../src/plugins/plugins.h"
#include "animation.h"
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LMC, "LMC", "<command|help>[,<options>]", "Run command");

bool ClockPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LMC))) {
        // vis[ualizer],<type>
        if (args.startsWithIgnoreCase(0, F("vis"))) {
            int visTxtType = -1;
            auto newType = Clock::AnimationType::RAINBOW_FASTLED;
            auto newTypeCStr = args.get(1);
            if (*newTypeCStr) {
                auto newTypeStr = String(newTypeCStr);
                _config.normalizeSlug(newTypeStr);
                if (newTypeStr.trim().length()) {
                    for(uint8_t i = 0; i < static_cast<uint8_t>(AnimationType::LAST); i++) {
                        auto name = String(_getAnimationNameSlug(static_cast<AnimationType>(i)));
                        if (_config.normalizeSlug(name) == newTypeStr) {
                            visTxtType = i;
                            break;
                        }
                    }
                }
            }
            if (visTxtType == -1) {
                newType = static_cast<Clock::AnimationType>(args.toIntMinMax(1, static_cast<int>(Clock::AnimationType::MIN), static_cast<int>(Clock::AnimationType::LAST) - 1, static_cast<int>(Clock::AnimationType::RAINBOW_FASTLED)));
            }
            else {
                newType = static_cast<Clock::AnimationType>(visTxtType);
            }
            setAnimation(newType, 0);
            args.printf_P(PSTR("Visualizer=%u (%s)"), _config.animation, _getAnimationName(static_cast<Clock::AnimationType>(_config.animation)));
        }
        // br[ightness],<level>
        else if (args.startsWithIgnoreCase(0, F("br"))) {
            if (args.requireArgs(2, 3)) {
                auto brightness = args.toIntMinMax<uint16_t>(1, 0, Clock::kMaxBrightness);
                auto time = args.toMillis(2, 0, 60000, 10000);
                setBrightness(brightness, time);
                args.printf_P("fading brightness to %.2f%% (%u) in %.3f seconds", brightness / (float)Clock::kMaxBrightness * 100.0, brightness, time / 1000.0);
            }
        }
        // co[lor],<#RGB|r,b,g>
        else if (args.startsWithIgnoreCase(0, F("co"))) {
            if (args.size() == 1) {
                setColor(Color::fromString(args.toString(1)));
            }
            else if (args.size() == 4) {
                setColor(Color(args.toNumber(1, 0), args.toNumber(2, 0), args.toNumber(3, 0x80)));
            }
            args.printf_P(PSTR("set color %s"), getColor().toString().c_str());
        }
        // met[hod][,<fastled|neoex|neo|none|toggle>]
        // +lmc=method,tog
        else if (args.startsWithIgnoreCase(0, F("met"))) {
            if (args.startsWithIgnoreCase(1, F("fast"))) {
                ClockPlugin::setShowMethod(Clock::ShowMethodType::FASTLED);
            }
            #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
                else if (args.startsWithIgnoreCase(1, F("neoex"))) {
                    ClockPlugin::setShowMethod(Clock::ShowMethodType::NEOPIXEL_EX);
                }
            #endif
            #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                else if (args.startsWithIgnoreCase(1, F("neo"))) {
                    ClockPlugin::setShowMethod(Clock::ShowMethodType::AF_NEOPIXEL);
                }
            #endif
            else if (args.startsWithIgnoreCase(1, F("none"))) {
                ClockPlugin::setShowMethod(Clock::ShowMethodType::NONE);
            }
            else {
                ClockPlugin::toggleShowMethod();
            }
            args.print(F("show method: %s (%u)"), ClockPlugin::getShowMethodStr(), ClockPlugin::getShowMethod());
        }
        // ani[mation][,<animation>][,blend_time=4000ms]
        else if (args.startsWithIgnoreCase(0, F("ani"))) {
            if (args.size() <= 1) {
                for(uint8_t i = 0; i < static_cast<uint8_t>(AnimationType::LAST); i++) {
                    auto slug = String(_config.getAnimationNameSlug(static_cast<AnimationType>(i)));
                    args.print(F("%s animation (+LMC=vis,%s)"), _config.getAnimationName(static_cast<AnimationType>(i)), _config.normalizeSlug(slug).c_str());
                }
            }
            else {
                auto animation = args.toString(1);
                animation.replace(' ', '_');
                auto blendTime = args.toMillis(2, 0, 30000, 4000);
                for(uint8_t i = 0; i < static_cast<uint8_t>(AnimationType::LAST); i++) {
                    auto name = String(_config.getAnimationName(static_cast<AnimationType>(i)));
                    name.replace(' ', '_');
                    if (animation.equalsIgnoreCase(name)) {
                        setAnimation(static_cast<AnimationType>(i), blendTime);
                        break;
                    }
                }
            }
        }
        // dit[her],<on|off>
        else if (args.startsWithIgnoreCase(0, F("dit"))) {
            bool state = args.isTrue(1);
            _display.setDither(state);
            args.print(F("dithering %s"), state ? PSTR("enabled") : PSTR("disabled"));
        }
        // out[put],<on|off>
        else if (args.startsWithIgnoreCase(0, F("out"))) {
            bool state = args.isTrue(1);
            if (state) {
                _enable();
            }
            else {
                _disable();
            }
            args.print(F("LED output %s"), state ? PSTR("enabled") : PSTR("disabled"));
        }
        // map,<rows>,<cols>,<reverse_rows>,<reverse_columns>,<rotate>,<interleaved>,<offset>
        else if (args.startsWithIgnoreCase(0, F("map"))) {
            // the mapping is used by the renderer, it must not change while the loop task draws
            IF_NOT_LOOP_TASK(args.print(F("not available from this context, use the serial console")); return true);
            if (args.size() >= 6) {
                if (!_display.setParams(
                    args.toInt(1, _display.getRows()),
                    args.toInt(2, _display.getCols()),
                    args.isTrue(3, _display.isRowsReversed()),
                    args.isTrue(4, _display.isColsReversed()),
                    args.isTrue(5, _display.isRotated()),
                    args.isTrue(6, _display.isInterleaved())
                )) {
                    args.print(F("failed to set parameters"));
                }
            }
            args.print(F("+lmc=map,%u,%u,%u,%u,%u,%u"), _display.getRows(), _display.getCols(), _display.isRowsReversed(), _display.isColsReversed(), _display.isRotated(), _display.isInterleaved());
        }
        // fr[ames][,rst]
        else if (args.startsWithIgnoreCase(0, F("fr"))) {
            auto &stats = NeoPixelEx::getStats();
            args.print(F("Internal: aborted frames=%u/%u fps=%u"), stats.getAbortedFrames(), stats.getFrames(), stats.getFps());
            args.print(F("FastLED: fps=%u _fps=%.1f"), FastLED.getFPS(), _fps);
            if (args.size() > 1) {
                FastLED.countFPS();
                stats.clear();
                args.print(F("stats reset"));
            }
        }
        // res[et][,<pixels>]
        else if (args.startsWithIgnoreCase(0, F("res"))) {
            _resetDisplay();
            args.print(F("display reset"));
        }
        // cl[ear]
        else if (args.startsWithIgnoreCase(0, F("cl"))) {
            enableLoop(false);
            _clear();
            _show();
            delay(1);
            _show();
            args.print(F("display cleared"));
        }
        // pr[int],<display=00:00:00>
        else if (args.startsWithIgnoreCase(0, F("pr"))) {
            #if IOT_LED_MATRIX == 0
                enableLoop(false);
                if (args.size() < 1) {
                    _display.clear();
                    _display.show();
                    args.print(F("display cleared"));
                }
                else {
                    auto text = args.get(1);
                    args.print(F("display '%s'"), text);
                    _display.clear();
                    _display.fill(0x000020);
                    _display.setBrightness(255);
                    _display.print(text);
                    _display.show();
                }
            #else
                args.print(F("print not supported"));
            #endif
        }
        // lo[op],<enable|disable>
        else if (args.startsWithIgnoreCase(0, F("lo"))) {
            _clear();
            _show();
            delay(1);
            _show();
            auto value = args.isTrue(1);
            enableLoop(value);
            args.print(F("loop %s"), value ? PSTR("enabled") : PSTR("disabled"));
        }
        // st[ate]
        else if (args.startsWithIgnoreCase(0, F("st"))) {
            auto config = Plugins::Clock::getConfig();
            auto initialState = PSTR("INVALID");
            switch(_config.getInitialState()) {
                case InitialStateType::ON:
                    initialState = PSTR("ON");
                    break;
                case InitialStateType::OFF:
                    initialState = PSTR("OFF");
                    break;
                case InitialStateType::RESTORE:
                    initialState = PSTR("RESTORE");
                    break;
                default:
                    break;
            }
            #if IOT_LED_MATRIX_STANDBY_PIN != -1
                auto state = digitalRead(IOT_LED_MATRIX_STANDBY_PIN) == IOT_LED_MATRIX_STANDBY_PIN_STATE(true);
                args.print(F("enable pin=%u initial state=%s"), state, initialState);
            #else
                args.print(F("initial state=%s"), initialState);
            #endif
            #if !IOT_SENSOR_HAVE_MOTION_SENSOR
            int _motionAutoOff = 0;
            #endif
            args.print(F("state: _isEnabled=%u _isRunning=%u _targetBrightness=%u _autoOff=%u temp. protection=%u"), _isEnabled, _isRunning, _targetBrightness, _motionAutoOff, isTempProtectionActive());
            args.print(F("current config: enabled=%u brightness=%u animation=%s"), _config.enabled, _config.brightness, _config.getAnimationName(_config._get_enum_animation()));
            args.print(F("stored config: enabled=%u brightness=%u animation=%s"), config.enabled, config.brightness, config.getAnimationName(config._get_enum_animation()));
        }
        // temp,<value>
        else if (args.startsWithIgnoreCase(0, F("tem"))) {
            _tempOverride = args.toIntMinMax<uint8_t>(0, kMinimumTemperatureThreshold, 255, 0);
            if (_tempOverride) {
                args.printf_P(PSTR("temperature override %u%s"), _tempOverride, SPGM(UTF8_degreeC));
            }
            else {
                args.print(F("temperature override disabled"));
            }
        }
        // +lmc=set,0-16,#120000;+lmc=get
        // +lmc=set,0-16,0x23,0,0;+lmc=get
        // +lmc=get
        // +lmc=cl;+lmc=get
        // get,[<any|*>[,<offset>,<length>]
        else if (args.equalsIgnoreCase(0, F("get")) || args.equalsIgnoreCase(0, F("set"))) {
            Color color;
            auto &stream = args.getStream();
            auto range = args.toRange(1, 0, _display.size() - 1, PrintString(F("0,1")));

            if (_display.getRows() > 1) {
                args.print(F("Matrix %ux%u, segments=%u"), _display.getCols(), _display.getRows(), _display.getNumSegments());
            }
            else {
                args.print(F("Strip, %u pixels, segments=%u"), _display.size(), _display.getNumSegments());
            }
            // <set>,[<any|*>[,<range>,<#color>]
            if (args.equalsIgnoreCase(0, F("set"))) {
                if (args.toString(2).trim() == F("0")) {
                    color = 0;
                }
                else if (args.size() > 4) {
                    color = Color(args.toNumber(2, 0), args.toNumber(3, 0), args.toNumber(4, 0x80));
                }
                else {
                    color = Color::fromString(args.toString(2));
                }
                auto point = _display.getPoint(range.offset);
                auto address = _display.getAddress(point);
                args.print(F("pixel=%u color=%s x=%u y=%u seq=%u show=%s pin=%u"), range.size - range.offset + 1, color.toString().c_str(), point.col(), point.row(), address, getNeopixelShowMethodStr(), IOT_LED_MATRIX_OUTPUT_PIN);
                _display.dump(args.getStream());
                for(uint16_t i = range.offset; i < range.offset + range.size; i++) {
                    _display.setPixel(i, color);
                    _display.setPixelState(i, true);
                }
                _show();
                delay(1);
                _show();
            }
            else {
                args.getStream().printf_P(PSTR("show=%s pin=%u "), getNeopixelShowMethodStr(), IOT_LED_MATRIX_OUTPUT_PIN);
                _display.dump(args.getStream());
                auto ofs = range.offset;
                for(uint16_t y = 0; y < _display.getRows(); y++) {
                    for(uint16_t x = 0; x < _display.getCols(); x++) {
                        if (ofs > 0) {
                            ofs--;
                            if (ofs == 0) {
                                stream.printf_P(PSTR("...[%u] "), range.offset);
                            }
                            continue;
                        }
                        auto address = _display.getAddress(y, x);
                        auto state = _display.getPixelState(address);
                        auto color = Color(_display._pixels[address]);
                        stream.printf_P(PSTR("%c%06x "), state ? '#' : '!', color.get());
                        if ((_display.getRows() == 1) && ((x % 10) == 9)) {
                            stream.println();
                        }
                        if (ofs >= range.size) {
                            y = 0xffff;
                        }
                    }
                    stream.println();
                }
            }
        }
        else {
            auto subCommand = args.get(0);
            args.print(F("Invalid command: %s"), subCommand ? subCommand : PSTR("arguments required"));
        }
        return true;
    }
    return false;
}

#endif
