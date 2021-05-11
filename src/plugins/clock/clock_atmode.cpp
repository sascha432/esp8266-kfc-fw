/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <web_socket.h>
#include "../src/plugins/plugins.h"
#include "animation.h"

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

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKTP, "TP", "<#color>[,<time=500ms>]", "Test peak values");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKBR, "BR", "Set brightness (0-65535). 0 disables the LEDs, > 0 enables them");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKPX, "PX", "[<number|-1=all>,<#RGB>|<r>,<g>,<b>]", "Set level of a single pixel. No arguments turns all off");
#if !IOT_LED_MATRIX
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKP, "P", "<00[:.]00[:.]00>", "Display strings");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKV, "V", "<1-4>", "Set visualizer mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKC, "C", "<#RGB>|<r>,<g>,<b>", "Set color");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKM, "M", "<value>[,<incr>,<min>,<max>]", "Set rainbow animation multiplier");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKT, "T", "<value>", "Override temperature");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKTS, "TS", "<num>,<segment>", "Set segment for digit <num>");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CLOCKA, "A", "<num>[,<arguments>,...]", "Set animation", "Display available animations");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKD, "D", "Dump pixel addresses and other information");
#if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "LM"
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LMVIEW, "VIEW", "<interval in ms|0=disable>,<client_id>", "Display Leds over http2serial");
#endif

#if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL

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
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKTP),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKBR),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKPX),
#if !IOT_LED_MATRIX
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKP),
#endif
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKC),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKM),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKT),
        // PROGMEM_AT_MODE_HELP_COMMAND(CLOCKTS),
        PROGMEM_AT_MODE_HELP_COMMAND(CLOCKA),
        // PROGMEM_AT_MODE_HELP_COMMAND(CLOCKD),
#if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
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
#if !IOT_LED_MATRIX
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
            _display.print(text);
            _display.show();
        }
        return true;
    }
    else
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKM))) {
        _config.rainbow.multiplier.value = args.toFloatMinMax(0, 0.1f, 100.0f, _config.rainbow.multiplier.value);
        args.printf_P(PSTR("Rainbow multiplier=%f increment=%f min=%f max=%f"), _config.rainbow.multiplier.value, _config.rainbow.multiplier.incr, _config.rainbow.multiplier.min, _config.rainbow.multiplier.max);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKV))) {
        Clock::VisualizerAnimation::_visualizerType = args.toIntMinMax(0, 1, 4, 2);
        args.printf_P(PSTR("Visualizer=%u"), Clock::VisualizerAnimation::_visualizerType);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKA))) {
        if (args.isQueryMode()) {
            for(uint8_t i = 0; i < static_cast<int>(AnimationType::MAX); i++) {
                auto name = String(_config.getAnimationName(static_cast<AnimationType>(i)));
                name.toLowerCase();
                args.printf_P(PSTR("%s - %s animation (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=%s[,<options>])"), name.c_str(), _config.getAnimationName(static_cast<AnimationType>(i)), _config.getAnimationName(static_cast<AnimationType>(i)));
            }
            args.printf_P(PSTR("%u - rainbow animation (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=%u,<speed>,<multiplier>,<r>,<g>,<b-factor>)"), AnimationType::RAINBOW, AnimationType::RAINBOW);
            args.printf_P(PSTR("%u - fade to color (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=%u,<r>,<g>,<b>)"), AnimationType::FADING, AnimationType::FADING);
#if !IOT_LED_MATRIX
            args.printf_P(PSTR("%u - blink colon speed"), (int)AnimationType::MAX);
            args.print(F("100 = disable clock"));
            args.print(F("101 = enable clock"));
            args.print(F("200 = display ambient light sensor value (+" PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "A=200,<0|1>)"));
#endif
            args.print(F("300 = test pixel order"));
        }
        else if (args.size() >= 1) {
            auto animation = _getAnimationType(args.toString(0));
            int value = static_cast<int>(animation);
            if (animation == AnimationType::MAX) {
                value = args.toInt(0);
            }

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
                if (interval < 50) {
                    interval = 50;
                }
                _Scheduler.add(interval, true, [num, this](Event::CallbackTimerPtr timer) mutable {
                    __LDBG_printf("pixel=%u", num);
                    if (num == _display.kNumPixels) {
                        _display.clear();
                        _display.show();
                        timer->disarm();
                    }
                    else {
                        if (num) {
                            _display.setPixel(num - 1, 0);
                        }
                        _display.setPixel(num++, 0x000022);
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
                        args.print(F("displaying sensor value"));
                    }
                }
                else if (_displaySensor == DisplaySensorType::SHOW) {
                    _displaySensor = DisplaySensorType::OFF;
                    _autoBrightness = _config.auto_brightness;
                    args.print(F("displaying time"));
                }
            }
#endif
            else if (value >= 0 && value < (int)AnimationType::MAX) {
                switch(static_cast<AnimationType>(value)) {
                    case AnimationType::RAINBOW:
                        _config.rainbow.speed = args.toIntMinMax(1, ClockPlugin::kMinRainbowSpeed, (uint16_t)0xfffe, _config.rainbow.speed);
                        _config.rainbow.multiplier.value = args.toFloatMinMax(2, 0.1f, 100.0f, _config.rainbow.multiplier.value);
                        _config.rainbow.color.factor.red = (uint8_t)args.toInt(3, _config.rainbow.color.factor.red);
                        _config.rainbow.color.factor.green = (uint8_t)args.toInt(4, _config.rainbow.color.factor.green);
                        _config.rainbow.color.factor.blue = (uint8_t)args.toInt(5, _config.rainbow.color.factor.blue);
                    default:
                        break;
                }
                setAnimation(static_cast<AnimationType>(value));
            }
        }
#if !IOT_LED_MATRIX
        else {
            setBlinkColon(0);
        }
#endif
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKTP))) {
        if (args.requireArgs(1, 2)) {

            Sensor_INA219 *ina219 = nullptr;
            for(auto sensor: SensorPlugin::getSensors()) {
                if (sensor->getType() == SensorPlugin::SensorType::INA219) {
                    ina219 = reinterpret_cast<Sensor_INA219 *>(sensor);
                }
            }

            setAnimation(AnimationType::SOLID, 0);
            _setBrightness(0);
            if (ina219) {
                _Scheduler.add(Event::milliseconds(250), false, [this, ina219](Event::CallbackTimerPtr) {
                    ina219->resetPeak();
                });
            }

            setColor(Color::fromString(args.toString(0)));
            _setBrightness(255);

            auto &stream = args.getStream();
            _Scheduler.add(Event::milliseconds(args.toIntMinMax(1, 500, 5000, 500)), false, [this, ina219, &stream](Event::CallbackTimerPtr) {
                _setBrightness(0);
                if (ina219) {
                    stream.printf_P(PSTR("U=%f I=%f P=%f"), ina219->getVoltage(), ina219->getPeakCurrent(), ina219->getPeakPower());
                }
            });
        }
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
                args.printf_P(PSTR("pixel=0-%u, color=%s"), _display.kNumPixels, color.toString().c_str());
                _display.fill(color.get());
                _display.show();
            }
            else if (num >= 0) {
                args.printf_P(PSTR("pixel=%u, color=%s"), num, color.toString().c_str());
                _display.setPixel(num, color.get());
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
#if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LMVIEW))) {
        auto interval = static_cast<uint32_t>(args.toInt(0));
        if (interval != 0) {
            interval = std::max(5U, interval);
        }
        auto serverStr = args.toString(1);
        if (serverStr.startsWith(F("udp")) && interval) {
            if (_displayLedTimer) {
                _displayLedTimer->timer->setInterval(Event::milliseconds(interval));
                args.printf_P(PSTR("changed interval to %ums"), interval);
            }
            else {
                StringVector parts;
                explode(serverStr.c_str(), ':', parts, 4);
                if (parts.size() >= 4) {
                    _displayLedTimer = new LedMatrixDisplayTimer(nullptr, 5000 / interval);
                    _displayLedTimer->host = parts[1];
                    _displayLedTimer->udpPort = parts[2].toInt();
                    _displayLedTimer->wsPort = parts[3].toInt();
                    auto &output = args.getStream();
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
                                    for(uint16_t i = IOT_LED_MATRIX_START_ADDR; i < (IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS) + IOT_LED_MATRIX_START_ADDR; i++) {
                                        auto color = _display._pixels[i];
                                        // 888 to 565
                                        tmp = ((color.red & 0b11111000) << 8) | ((color.green & 0b11111100) << 3) | (color.blue >> 3);
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
                                        delay(50);
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

                                    for(uint16_t i = IOT_LED_MATRIX_START_ADDR; i < (IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS) + IOT_LED_MATRIX_START_ADDR; i++) {
                                        auto color = _display._pixels[i];
                                        // 888 to 565
                                        *buf++ = ((color.red & 0b11111000) << 8) | ((color.green & 0b11111100) << 3) | (color.blue >> 3);
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
                                    delay(50);
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
#endif
    // else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKD))) {
    //     _display.dump(args.getStream());
    //     return true;
    // }
    return false;
}

#endif
