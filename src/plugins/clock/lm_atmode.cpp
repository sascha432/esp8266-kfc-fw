/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <web_socket.h>
#include <NeoPixelEx.h>
#include "../src/plugins/plugins.h"
#include "animation.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#if IOT_SENSOR_HAVE_INA219
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LMTESTP, "LMTESTP", "<#color>[,<time=500ms>]", "Test peak values. WARNING! This command will bypass all potections and limits");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LMC, "LMC", "<command|help>[,<options>]", "Run command");
#if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LMVIEW, "LMVIEW", "<interval in ms|0=disable>,<client_id>", "Display Leds over http2serial");
#endif

#if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL

void ClockPlugin::_removeDisplayLedTimer()
{
    if (_displayLedTimer) {
        _displayLedTimer->print(F("+LED_MATRIX=0"));
        delete _displayLedTimer;
        _displayLedTimer = nullptr;
    }
}

#endif

ATModeCommandHelpArrayPtr ClockPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
#if IOT_SENSOR_HAVE_INA219
        PROGMEM_AT_MODE_HELP_COMMAND(LMTESTP),
#endif
        PROGMEM_AT_MODE_HELP_COMMAND(LMC),
#if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
        PROGMEM_AT_MODE_HELP_COMMAND(LMVIEW)
#endif
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
    // if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKM))) {
    //     _config.rainbow.multiplier.value = args.toFloatMinMax(0, 0.1f, 100.0f, 5.23f);
    //     _config.rainbow.multiplier.incr = args.toFloatMinMax(1, 0.0f, 1.0f, _config.rainbow.multiplier.incr);
    //     _config.rainbow.multiplier.min = args.toFloatMinMax(2, 0.1f, 100.0f, _config.rainbow.multiplier.min);
    //     _config.rainbow.multiplier.max = args.toFloatMinMax(3, 0.1f, 100.0f, _config.rainbow.multiplier.max);
    //     _config.rainbow.color.red_incr = args.toFloatMinMax(4, 0.0f, 1.0f, _config.rainbow.color.red_incr);
    //     _config.rainbow.color.green_incr = args.toFloatMinMax(5, 0.0f, 1.0f, _config.rainbow.color.green_incr);
    //     _config.rainbow.color.blue_incr = args.toFloatMinMax(6, 0.0f, 1.0f, _config.rainbow.color.blue_incr);
    //     args.printf_P(PSTR("Rainbow multiplier=%f increment=%f min=%f max=%f color incr r=%f g=%f b=%f"),
    //         _config.rainbow.multiplier.value, _config.rainbow.multiplier.incr, _config.rainbow.multiplier.min, _config.rainbow.multiplier.max,
    //         _config.rainbow.color.red_incr, _config.rainbow.color.green_incr, _config.rainbow.color.blue_incr
    //     );
    //     return true;
    // }
//     else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKA))) {
//         if (args.isQueryMode()) {
//             for(uint8_t i = 0; i < static_cast<int>(AnimationType::MAX); i++) {
//                 auto name = String(_config.getAnimationName(static_cast<AnimationType>(i)));
//                 name.toLowerCase();
//                 args.printf_P(PSTR("%s - %s animation (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=%s[,<options>])"), name.c_str(), _config.getAnimationName(static_cast<AnimationType>(i)), _config.getAnimationName(static_cast<AnimationType>(i)));
//             }
//             args.printf_P(PSTR("%u - rainbow animation (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=%u,<speed>,<multiplier>,<r>,<g>,<b-factor>)"), AnimationType::RAINBOW, AnimationType::RAINBOW);
//             args.printf_P(PSTR("%u - fade to color (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=%u,<r>,<g>,<b>)"), AnimationType::FADING, AnimationType::FADING);
//             IF_IOT_CLOCK_MODE(
//                 args.printf_P(PSTR("%u - blink colon speed"), (int)AnimationType::MAX);
//                 args.print(F("100 = disable clock"));
//                 args.print(F("101 = enable clock"));
//                 args.print(F("200 = display ambient light sensor value (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=200,<0|1>)"));
//             )
//             args.print(F("300 = test pixel order"));
//         }
//         else if (args.size() >= 1) {
//             auto animation = _getAnimationType(args.toString(0));
//             int value = static_cast<int>(animation);
//             if (animation == AnimationType::MAX) {
//                 value = args.toInt(0);
//             }

//             IF_IOT_CLOCK_MODE(
//                 if (value == (int)AnimationType::MAX) {
//                     setBlinkColon(args.toIntMinMax(1, 50U, 0xffffU, 1000U));
//                 }
//                 else
//             )
//             if (value == 100) {
//                 enableLoop(false);
//                 _display.reset();
//             }
//             else if (value == 101) {
//                 enableLoop(true);
//                 _display.reset();
//             }
//             else if (value == 300) {
//                 int interval = args.toInt(1, 500);
//                 enableLoop(false);
//                 size_t num = 0;
//                 if (interval < 50) {
//                     interval = 50;
//                 }
//                 _Scheduler.add(interval, true, [num, this](Event::CallbackTimerPtr timer) mutable {
//                     __LDBG_printf("pixel=%u", num);
//                     if (num == _display.kNumPixels) {
//                         _display.clear();
//                         _display.show();
//                         timer->disarm();
//                     }
//                     else {
//                         if (num) {
//                             _display.setPixel(num - 1, 0);
//                         }
//                         _display.setPixel(num++, 0x000022);
//                     }
//                 });
//             }
// #if !IOT_LED_MATRIX && IOT_CLOCK_AMBIENT_LIGHT_SENSOR
//             else if (value == 200) {
//                 if (args.isTrue(1)) {
//                     if (_displaySensor == DisplaySensorType::OFF) {
//                         _autoBrightness = kAutoBrightnessOff;
//                         _displaySensor = DisplaySensorType::SHOW;
//                         _display.clear();
//                         _display.show();
//                         args.print(F("displaying sensor value"));
//                     }
//                 }
//                 else if (_displaySensor == DisplaySensorType::SHOW) {
//                     _displaySensor = DisplaySensorType::OFF;
//                     _autoBrightness = _config.auto_brightness;
//                     args.print(F("displaying time"));
//                 }
//             }
// #endif
//             else if (value >= 0 && value < (int)AnimationType::MAX) {
//                 switch(static_cast<AnimationType>(value)) {
//                     case AnimationType::RAINBOW:
//                         _config.rainbow.speed = args.toIntMinMax(1, ClockPlugin::kMinRainbowSpeed, (uint16_t)0xfffe, _config.rainbow.speed);
//                         _config.rainbow.multiplier.value = args.toFloatMinMax(2, 0.1f, 100.0f, _config.rainbow.multiplier.value);
//                         _config.rainbow.color.factor.red = (uint8_t)args.toInt(3, _config.rainbow.color.factor.red);
//                         _config.rainbow.color.factor.green = (uint8_t)args.toInt(4, _config.rainbow.color.factor.green);
//                         _config.rainbow.color.factor.blue = (uint8_t)args.toInt(5, _config.rainbow.color.factor.blue);
//                     default:
//                         break;
//                 }
//                 setAnimation(static_cast<AnimationType>(value));
//             }
//         }
//         IF_IOT_CLOCK_MODE(
//             else {
//                 setBlinkColon(0);
//             }
//         )
//         return true;
//     }
//     IF_IOT_SENSOR_HAVE_INA219(
//         else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LMTESTPEAK))) {
//             if (args.requireArgs(1, 2)) {

//                 Sensor_INA219 *ina219 = nullptr;
//                 for(auto sensor: SensorPlugin::getSensors()) {
//                     if (sensor->getType() == SensorPlugin::SensorType::INA219) {
//                         ina219 = reinterpret_cast<Sensor_INA219 *>(sensor);
//                     }
//                 }

//                 uint32_t color = Color::fromString(args.toString(0));
//                 auto duration = args.toIntMinMax(1, 100, 5000, 100);
//                 args.print(F("testing color=%06x brightness 255 duration %u mspower_limit=off. debug mode enabled, reset device to disable"), color, duration);

//                 _debug = true;
//                 FastLED.setMaxPowerInMilliWatts(0, nullptr);

//                 _setBrightness(5);
//                 // if (ina219) {
//                 //     _Scheduler.add(Event::milliseconds(250), false, [this, ina219](Event::CallbackTimerPtr) {
//                 //         ina219->resetPeak();
//                 //     });
//                 // }

//                 _display.setDither(false);
//                 _display.setBrightness(0);
//                 _display.clear();
//                 _display.show();
//                 delay(250);

//                 if (ina219) {
//                     ina219->resetPeak();
//                 }

//                 _display.setBrightness(255);
//                 _display.fill(color);
//                 _display.show();

//                 auto &stream = args.getStream();
//                 _Scheduler.add(Event::milliseconds(), false, [this, ina219, &stream](Event::CallbackTimerPtr) {
//                     // ramp down slowly to keep the 5V rail within is limits
//                     for(uint8_t i = 255 - 31; i >= 0; i -= 32) {
//                         _display.setBrightness(i);
//                         _display.show();
//                         ::delay(10);
//                     }
//                     _display.setBrightness(0);
//                     _display.clear();
//                     _display.show();

//                     if (ina219) {
//                         stream.printf_P(PSTR("U=%f I=%f P=%f"), ina219->getVoltage(), ina219->getPeakCurrent(), ina219->getPeakPower());
//                     }
//                 });
//             }
//             return true;
//         }
//     )
//     else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LMC))) {
//         if (args.size() == 1) {
//             setColor(Color::fromString(args.toString(0)));
//         }
//         else if (args.requireArgs(3, 3)) {
//             setColor(Color(args.toNumber(0, 0), args.toNumber(1, 0), args.toNumber(2, 0x80)));
//         }
//         args.printf_P(PSTR("set color %s"), getColor().toString().c_str());
//         return true;
//     }
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LMC))) {
        if (args.startsWithIgnoreCase(0, F("vis"))) {
            IF_IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER(
                _config.visualizer.type = args.toIntMinMax(1, 1, 4, 2);
                args.printf_P(PSTR("Visualizer=%u"), _config.visualizer.type);
            )
        }
        else if (args.startsWithIgnoreCase(0, F("br"))) {
            if (args.requireArgs(2, 3)) {
                auto brightness = args.toIntMinMax<uint16_t>(1, 0, Clock::kMaxBrightness);
                auto time = args.toMillis(2, 0, 60000, 10000);
                setBrightness(brightness, time);
                args.printf_P("fading brightness to %.2f%% (%u) in %.3f seconds", brightness / (float)Clock::kMaxBrightness * 100.0, brightness, time / 1000.0);
            }
        }
        // color,<#RGB|r,b,g>
        else if (args.startsWithIgnoreCase(0, F("co"))) {
            if (args.size() == 1) {
                setColor(Color::fromString(args.toString(1)));
            }
            else if (args.size() == 4) {
                setColor(Color(args.toNumber(1, 0), args.toNumber(2, 0), args.toNumber(3, 0x80)));
            }
            args.printf_P(PSTR("set color %s"), getColor().toString().c_str());
        }
        // met[hod][,<fastled|interenal|toggle>]
        // +lmc=method,tog
        else if (args.startsWithIgnoreCase(0, F("met"))) {
            if (args.startsWithIgnoreCase(1, F("fast"))) {
                ClockPlugin::setShowMethod(Clock::ShowMethodType::FASTLED);
            }
            else if (args.startsWithIgnoreCase(1, F("int"))) {
                ClockPlugin::setShowMethod(Clock::ShowMethodType::NEOPIXEL);
            }
            else {
                ClockPlugin::toggleShowMethod();
            }
            args.print(F("show method: %s (%u)"), ClockPlugin::getShowMethod() == Clock::ShowMethodType::FASTLED ? PSTR("FastLED") : PSTR("internal"), ClockPlugin::getShowMethod());
        }
        else if (args.startsWithIgnoreCase(0, F("ani"))) {
            enableLoop(false);
            uint16_t x;
            uint16_t y;
            uint16_t n;
            _display.setBrightness(32);
            _display.clear();
            auto displayPtr = &_display;
            switch(args.toInt(1)) {
                case 1:
                    _Scheduler.add(Event::milliseconds(100), true, [n, displayPtr](Event::CallbackTimerPtr timer) mutable {
                        displayPtr->fill(0);
                        displayPtr->setPixel(n, Color(0x300030));
                        displayPtr->show();
                        n++;
                        if (n >= displayPtr->kCols * displayPtr->kRows) {
                            displayPtr->clear();
                            displayPtr->show();
                            timer->disarm();
                        }
                    });
                    break;
                case 2:
                    _Scheduler.add(Event::milliseconds(100), true, [n, displayPtr](Event::CallbackTimerPtr timer) mutable {
                        displayPtr->fill(0x300030);
                        displayPtr->hideAll();
                        displayPtr->setPixelState(n, true);
                        displayPtr->show();
                        n++;
                        if (n >= displayPtr->kCols * displayPtr->kRows) {
                            displayPtr->clear();
                            displayPtr->show();
                            timer->disarm();
                        }
                    });
                    break;
                default:
                    _Scheduler.add(Event::milliseconds(100), true, [x, y, displayPtr](Event::CallbackTimerPtr timer) mutable {
                        displayPtr->fill(0);
                        displayPtr->setPixel(y, x, Color(0x300030));
                        displayPtr->show();
                        x++;
                        if (x >= displayPtr->kCols) {
                            y++;
                            if (y >= displayPtr->kRows) {
                                displayPtr->fill(0);
                                displayPtr->show();
                                timer->disarm();
                            }
                        }
                    });
                    break;
            }
        }
        // dither,<on|off|1|0>
        else if (args.startsWithIgnoreCase(0, F("dit"))) {
            bool state = args.isTrue(1);
            _display.setDither(state);
            args.print(F("dithering %s"), state ? PSTR("enabled") : PSTR("disabled"));
        }
        // frames[,rst]
        else if (args.startsWithIgnoreCase(0, F("fr"))) {
            auto &stats = NeoPixelEx::getStats();
            args.print(F("Internal: aborted frames=%u/%u fps=%u"), stats.getAbortedFrames(), stats.getFrames(), stats.getFps());
            args.print(F("FastLED: fps=%u"), FastLED.getFPS());
            if (args.size() > 1) {
                stats.clear();
                args.print(F("reset"));
            }
        }
        // +lmc=res,1
        else if (args.startsWithIgnoreCase(0, F("res"))) {
            enableLoop(true);
            if (args.isTrue(1)) {
                _display.clear();
                pinMode(IOT_CLOCK_WS2812_OUTPUT, OUTPUT);
                NeoPixel_espShow(IOT_CLOCK_WS2812_OUTPUT, nullptr, IOT_CLOCK_NUM_PIXELS * 3, 0);
            }
            else {
                _display.setBrightness(32);
                _display.clear();
                _display.show();
            }
            args.print(F("display reset"));
        }
        else if (args.startsWithIgnoreCase(0, F("cl"))) {
            enableLoop(false);
            _display.clear();
            _display.show();
            args.print(F("display cleared"));
        }
        // pr[int],<display=00:00:00>
        else if (args.startsWithIgnoreCase(0, F("pr"))) {
            IF_IOT_CLOCK_MODE(
                enableLoop(false);
                if (args.size() < 1) {
                    _display.clear();
                    _display.show();
                    args.print(F("display cleared"));
                }
                else {
                    auto text = args.get(0);
                    args.printf_P(PSTR("display '%s'"), text);
                    _display.clear();
                    _display.fill(0x000020);
                    _display.setBrightness(255);
                    _display.print(text);
                    _display.show();
                }
            )
        }
        // lo[op],<enable|disable>
        else if (args.startsWithIgnoreCase(0, F("lo"))) {
            _display.clear();
            _display.show();
            auto value = args.isTrue(1);
            enableLoop(value);
            args.print(F("loop %s"), value ? PSTR("enabled") : PSTR("disabled"));
        }
        // <temp>,<value>
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
        // <get>,[<any|*>[,<offset>,<length>]
        else if (args.equalsIgnoreCase(0, F("get")) || args.equalsIgnoreCase(0, F("set"))) {
            Color color;
            auto &stream = args.getStream();
            auto range = args.toRange(1, 0, _display.kNumPixels - 1, PrintString(F("0,1")));

            if (_display.kRows > 1) {
                args.print(F("Matrix %ux%u"), _display.kCols, _display.kRows);
            }
            else {
                args.print(F("No shape, %u pixels"), _display.kCols * _display.kRows);
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
                args.print(F("pixel=%u color=%s x=%u y=%u seq=%u show=%s pin=%u"), range.size - range.offset + 1, color.toString().c_str(), point.col(), point.row(), address, getNeopixelShowMethodStr(), IOT_CLOCK_WS2812_OUTPUT);
                _display.dump(args.getStream());
                for(uint16_t i = range.offset; i < range.offset + range.size; i++) {
                    _display.setPixel(i, color);
                    _display.setPixelState(i, true);
                }
                _display.show();
            }
            else {
                args.getStream().printf_P(PSTR("show=%s pin=%u "), getNeopixelShowMethodStr(), IOT_CLOCK_WS2812_OUTPUT);
                _display.dump(args.getStream());
                auto ofs = range.offset;
                for(uint16_t y = 0; y < _display.kRows; y++) {
                    for(uint16_t x = 0; x < _display.kCols; x++) {
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
                        if ((_display.kRows == 1) && ((x % 10) == 9)) {
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
            args.print(F("usage: +LMC=<lo[op],<enable|disable>|me[thod]<fastled|int>|ani[mation],<name>|di[ther],<on|off>|co[lor],<#color>|br[igthness],<level>|cl[ear]|re[set]|<temp>,<value>|get,<range>|set,<range>,[#color]|pr[int],<display=00:00:00>>"));
        }
        return true;
    }
    IF_IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL(
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LMVIEW))) {
            auto interval = static_cast<uint32_t>(args.toInt(0, 30));
            if (interval != 0) {
                interval = std::max(15U, interval);
            }
            auto serverStr = args.toString(1);
            if (serverStr.startsWith(F("udp")) && interval) {
                auto &output = args.getStream();
                if (_displayLedTimer) {
                    _displayLedTimer->timer->setInterval(Event::milliseconds(interval));
                    args.printf_P(PSTR("changed interval to %ums"), interval);
                    output.printf_P(PSTR("+LED_MATRIX_SERVER=ws://%s:%u,%u,%u,1,0,0,1\n"), _displayLedTimer->host.c_str(), _displayLedTimer->wsPort, IOT_LED_MATRIX_ROWS, IOT_LED_MATRIX_COLS);
                }
                else {
                    StringVector parts;
                    explode(serverStr.c_str(), ':', parts, 4);
                    if (parts.size() >= 4) {
                        _displayLedTimer = new LedMatrixDisplayTimer(nullptr, 5000 / interval);
                        _displayLedTimer->host = parts[1];
                        _displayLedTimer->udpPort = parts[2].toInt();
                        _displayLedTimer->wsPort = parts[3].toInt();
                        output.printf_P(PSTR("+LED_MATRIX_SERVER=ws://%s:%u,%u,%u,1,0,0,1\n"), _displayLedTimer->host.c_str(), _displayLedTimer->wsPort, IOT_LED_MATRIX_ROWS, IOT_LED_MATRIX_COLS);
                        _Timer(_displayLedTimer->timer).add(Event::milliseconds(interval), true, [this, &output](Event::CallbackTimerPtr timer) {
                            WiFiUDP udp;
                            uint16_t tmp;
                            if (udp.beginPacket(_displayLedTimer->host.c_str(), _displayLedTimer->udpPort)) {
                                bool error = true;
                                tmp = (uint16_t)WsClient::BinaryPacketType::LED_MATRIX_DATA;
                                if (udp.write((const uint8_t *)&tmp, sizeof(tmp)) == sizeof(tmp)) {
                                    tmp = _displayLedTimer->errors << 4;
                                    if (udp.write((const uint8_t *)&tmp, sizeof(tmp)) == sizeof(tmp)) {
                                        error = false;
                                        for(uint16_t i = IOT_LED_MATRIX_PIXEL_OFFSET; i < (IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS) + IOT_LED_MATRIX_PIXEL_OFFSET; i++) {
                                            #if !IOT_LED_MATRIX
                                            if (_display.getPixelState(i)) {
                                            #endif
                                                auto color = _display._pixels[i];
                                                // 888 to 565
                                                tmp = ((color.red & 0b11111000) << 8) | ((color.green & 0b11111100) << 3) | (color.blue >> 3);
                                            #if !IOT_LED_MATRIX
                                            }
                                            else {
                                                tmp = 0;
                                            }
                                            #endif
                                            if (udp.write((const uint8_t *)&tmp, sizeof(tmp)) != sizeof(tmp)) {
                                                error = true;
                                                break;
                                            }
                                        }
                                        if (!error) {
                                            error = !udp.endPacket();
                                        }
                                    }
                                }
                                if (error) {
                                    _displayLedTimer->errors++;
                                    if (_displayLedTimer->errors > _displayLedTimer->maxErrors) {
                                        __LDBG_printf("failed to send udp packet");
                                        timer->disarm();
                                        LoopFunctions::callOnce([this]() {
                                            _display.delay(50);
                                            _removeDisplayLedTimer();
                                        });
                                        return;
                                    }
                                }
                                else {
                                    _displayLedTimer->errors = 0;
                                }
                            }
                        });
                    }
                    else {
                        args.printf_P(PSTR("arguments: <interval>,udp:<host>:<udp-port>:<web-socket port>"));
                    }
                }
            }
            else {
                auto clientId = (void *)args.toNumber(1, 0);
                if (clientId && interval) {
                    if (_displayLedTimer) {
                        _displayLedTimer->timer->setInterval(Event::milliseconds(interval));
                        args.printf_P(PSTR("changed interval to %ums"), interval);
                    }
                    else {
                        auto client = Http2Serial::getClientById(clientId);
                        if (client) {
                            // rows,cols,reverse rows,reverse cols,rotate,interleaved
                            client->text(PrintString(F("+LED_MATRIX=%u,%u,1,0,0,1"), IOT_LED_MATRIX_ROWS, IOT_LED_MATRIX_COLS));
                            _displayLedTimer = new LedMatrixDisplayTimer(clientId, 2000 / interval);
                            _Timer(_displayLedTimer->timer).add(Event::milliseconds(interval), true, [this](Event::CallbackTimerPtr timer) {
                                auto client = Http2Serial::getClientById(_displayLedTimer->clientId);
                                if (client) {
                                    auto server = client->server();
                                    if (server->getQueuedMessageCount() < 5 && client->canSend()) {
                                        constexpr size_t frameSize = IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS * sizeof(uint16_t);
                                        auto sBuf = server->makeBuffer(frameSize + sizeof(uint16_t) * 2);
                                        auto buf = reinterpret_cast<uint16_t *>(sBuf->get());
                                        *buf++ = (uint16_t)WsClient::BinaryPacketType::LED_MATRIX_DATA;
                                        // 4 / 12 bit
                                        *buf++ = (server->getQueuedMessageCount() & 0x0f) | ((server->getQueuedMessageSize() >> 1) & 0xfff0);

                                        for(uint16_t i = IOT_LED_MATRIX_PIXEL_OFFSET; i < (IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS) + IOT_LED_MATRIX_PIXEL_OFFSET; i++) {
                                            #if !IOT_LED_MATRIX
                                            if (_display.getPixelState(i)) {
                                            #endif
                                                auto color = _display._pixels[i];
                                                // 888 to 565
                                                *buf++ = ((color.red & 0b11111000) << 8) | ((color.green & 0b11111100) << 3) | (color.blue >> 3);
                                            #if !IOT_LED_MATRIX
                                            }
                                            else {
                                                *buf++ = 0;
                                            }
                                            #endif
                                        }
                                        client->binary(sBuf);
                                        _displayLedTimer->errors = 0;
                                    }
                                    else {
                                        _displayLedTimer->errors++;
                                        if (_displayLedTimer->errors > _displayLedTimer->maxErrors) {
                                            client = nullptr;
                                        }
                                    }
                                }
                                if (!client) {
                                    timer->disarm();
                                    LoopFunctions::callOnce([this]() {
                                        _display.delay(50);
                                        _removeDisplayLedTimer();
                                    });
                                    return;
                                }
                            });
                            args.printf_P(PSTR("sending LED colors every %ums to %p"), interval, clientId);
                        }
                        else {
                            args.printf_P(PSTR("client_id %p not found"), clientId);
                        }
                    }
                }
                else {
                    if (_displayLedTimer) {
                        args.printf_P(PSTR("stopped sending LED colors to %p"), _displayLedTimer->clientId);
                        _removeDisplayLedTimer();
                    }
                    else {
                        args.printf_P(PSTR("not sending LED colors"));
                    }
                }
            }
            return true;
        }
    )
    return false;
}

#endif
