/**
 * Author: sascha_lammers@gmx.de
 */

 #if AT_MODE_SUPPORTED

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
                if (StrWrapper(newTypeStr).trim().length()) {
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
            _syncVisualizerType(newType);
            _setAnimation(newType, 0);
            args.printf_P(PSTR("Visualizer=%u (%s)"), _config.animation, _getAnimationName(static_cast<Clock::AnimationType>(_config.animation)));
        }
        // br[ightness],<level>
        else if (args.startsWithIgnoreCase(0, F("br"))) {
            if (args.requireArgs(2, 3)) {
                auto brightness = args.toIntMinMax<uint16_t>(1, 0, Clock::kMaxBrightness);
                auto time = args.toMillis(2, 0, 60000, 10000);
                _setBrightness(brightness, time);
                args.printf_P("fading brightness to %.2f%% (%u) in %.3f seconds", brightness / (float)Clock::kMaxBrightness * 100.0, brightness, time / 1000.0);
            }
        }
        // co[lor],<#RGB|r,g,b>
        else if (args.startsWithIgnoreCase(0, F("co"))) {
            if (args.size() == 2) {
                setColor(Color::fromString(args.toString(1)));
            }
            else if (args.size() == 4) {
                setColor(Color(args.toNumber(1, 0), args.toNumber(2, 0), args.toNumber(3, 0x80)));
            }
            args.printf_P(PSTR("set color %s"), getColor().toString().c_str());
        }
        // met[hod][,<fastled|nrmt|ni2s|neoex|none|toggle>]
        // +lmc=method,tog
        else if (args.startsWithIgnoreCase(0, F("met"))) {
            #if HAVE_NEOPIXELBUS
                // the NeoPixelBus build has no FastLED transport
                if (args.startsWithIgnoreCase(1, F("nrmt"))) {
                    ClockPlugin::setShowMethod(Clock::ShowMethodType::NEOBUS_RMT);
                }
                else if (args.startsWithIgnoreCase(1, F("ni2s"))) {
                    ClockPlugin::setShowMethod(Clock::ShowMethodType::NEOBUS_I2S);
                }
            #else
                if (args.startsWithIgnoreCase(1, F("fast"))) {
                    ClockPlugin::setShowMethod(Clock::ShowMethodType::FASTLED);
                }
            #endif
            #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
                else if (args.startsWithIgnoreCase(1, F("neoex"))) {
                    ClockPlugin::setShowMethod(Clock::ShowMethodType::NEOPIXEL_EX);
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
                StrWrapper(animation).slugify('_');
                auto blendTime = args.toMillis(2, 0, 30000, 4000);
                for(uint8_t i = 0; i < static_cast<uint8_t>(AnimationType::LAST); i++) {
                    auto name = String(_config.getAnimationName(static_cast<AnimationType>(i)));
                    StrWrapper(name).slugify('_');
                    if (animation.equalsIgnoreCase(name)) {
                        _syncVisualizerType(static_cast<AnimationType>(i));
                        _setAnimation(static_cast<AnimationType>(i), blendTime);
                        break;
                    }
                }
            }
        }
        // dit[her],<on|off>
        // NeoPixelBus has no temporal dithering
        #if !HAVE_NEOPIXELBUS
            else if (args.startsWithIgnoreCase(0, F("dit"))) {
                bool state = args.isTrue(1);
                _display.setDither(state);
                args.print(F("dithering %s"), state ? PSTR("enabled") : PSTR("disabled"));
            }
        #endif
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
        // map,<rows>,<cols>,<reverse_rows>,<reverse_columns>,<rotate>,<interleaved>
        else if (args.startsWithIgnoreCase(0, F("map"))) {
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
        // transport diagnostics for developers, the status page shows a summary for users. Only the
        // transports that are compiled in are listed
        else if (args.startsWithIgnoreCase(0, F("fr"))) {
            args.print(F("show method=%s (%u) fps=%.1f segments=%u pixels=%u"), getNeopixelShowMethodStr(), getShowMethod(), _fps, _display.getNumSegments(), _display.size());
            #if HAVE_NEOPIXELBUS
                // the counters are sticky since boot
                args.print(F("NeoPixelBus: fps=%.1f rmt blocks=%u rmt pins=%u i2s pins=%u wire=%uus"), _display.getFps(), gNeoPixelBusRmtMemBlocks, _display.getRmtStrips(), _display.getI2sStrips(), _display.getWireMicros());
                args.print(F("NeoPixelBus: transmissions=%u over=%u max. overrun=%uus max. write=%uus tx timeouts=%u"), _display.getTransmissions(), _display.getTransmissionsOver(), _display.getMaxOverrunMicros(), _display.getMaxWriteMicros(), _display.getTxTimeouts());
                args.print(F("frame time: anim=%uus show=%uus"), _animMicros, _showMicros);
            #else
                args.print(F("FastLED %u.%u.%u: fps=%u dithering=%u"), FASTLED_VERSION / 1000000, (FASTLED_VERSION / 1000) % 1000, FASTLED_VERSION % 1000, FastLED.getFPS(), _display.getDither());
                #if FASTLED_DEBUG_COUNT_FRAME_RETRIES
                    extern uint32_t _frame_cnt;
                    extern uint32_t _retry_cnt;
                    args.print(F("FastLED: aborted frames=%u/%u"), _retry_cnt, _frame_cnt);
                #endif
                #if FASTLED_VERSION == 3004000 && (IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION)
                    args.print(F("FastLED: power limit scale=%.1f%%"), FastLED.getPowerLimitScale() * 100.0f);
                #endif
            #endif
            #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
                auto &stats = NeoPixelEx::getStats();
                args.print(F("NeoPixelEx: fps=%u aborted frames=%u/%u"), stats.getFps(), stats.getAbortedFrames(), stats.getFrames());
            #endif
            if (args.size() > 1) {
                #if !HAVE_NEOPIXELBUS
                    FastLED.countFPS();
                #endif
                #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
                    stats.clear();
                #endif
                #if HAVE_NEOPIXELBUS
                    args.print(F("stats reset, the NeoPixelBus counters cannot be reset"));
                #else
                    args.print(F("stats reset"));
                #endif
            }
        }
        // cl[ear], stops the animation loop and blanks the pixels
        else if (args.startsWithIgnoreCase(0, F("cl"))) {
            enableLoop(false);
            _clear();
            _show();
            args.print(F("display cleared"));
        }
        // pr[int],<display=00:00:00>
        else if (args.startsWithIgnoreCase(0, F("pr"))) {
            #if IOT_LED_MATRIX == 0
                enableLoop(false);
                if (args.size() < 1) {
                    _display.clear();
                    _show();
                    args.print(F("display cleared"));
                }
                else {
                    auto text = args.get(1);
                    args.print(F("display '%s'"), text);
                    _display.clear();
                    _display.fill(0x000020);
                    _display.setBrightness(255);
                    _display.print(text);
                    _show();
                }
            #else
                args.print(F("print not supported"));
            #endif
        }
        // lo[op],<enable|disable>
        else if (args.startsWithIgnoreCase(0, F("lo"))) {
            _clear();
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
                args.print(F("enable pin=%u initial state=%s"), Clock::LedPower::isOn(), initialState);
            #else
                args.print(F("initial state=%s"), initialState);
            #endif
            #if !IOT_SENSOR_HAVE_MOTION_SENSOR
            int _motionAutoOff = 0;
            #endif
            args.print(F("state: _isEnabled=%u _isRunning=%u _targetBrightness=%u _autoOff=%u temp. protection=%u"), _isEnabled, _isRunning, _targetBrightness, _motionAutoOff, isTempProtectionActive());
            args.print(F("current config: enabled=%u brightness=%u animation=%s"), _config.enabled, _config.brightness, _config.getAnimationName(_config._get_enum_animation()));
            args.print(F("stored config: enabled=%u brightness=%u animation=%s"), config.enabled, config.brightness, config.getAnimationName(config._get_enum_animation()));
            #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
                args.print(F("power: %.2fW limit=%uW"), _getPowerLevel(), _config.power_limit);
            #endif
            #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
                #if ESP32
                    auto irReceiver = PSTR("software NEC receiver");
                #else
                    auto irReceiver = PSTR("IRremoteESP8266 receiver");
                #endif
                args.print(F("IR remote: pin=%u %s last code=%08x frames=%u repeats=%u"), IOT_LED_MATRIX_IR_REMOTE_PIN, irReceiver, _irLastCode, _irFrameCount, _irRepeatCount);
            #endif
            #if DEBUG_TASK_QUEUE
                args.print(F("deferred tasks: %u queued (%u peak of %u), %u processed, %u dropped"), static_cast<unsigned>(_tasks.size()), static_cast<unsigned>(_tasks.peakSize()), static_cast<unsigned>(_tasks.capacity()), static_cast<unsigned>(_tasks.processed()), static_cast<unsigned>(_tasks.dropped()));
            #endif
            // compile time switches of the diagnostics, 0 = not compiled in. DEBUG_IOT_CLOCK contains defined(),
            // which is only valid in #if
            #if DEBUG_IOT_CLOCK
                constexpr int kDebugClock = 1;
            #else
                constexpr int kDebugClock = 0;
            #endif
            #ifndef FASTLED_DEBUG_COUNT_FRAME_RETRIES
                constexpr int kFastLedRetries = 0;
            #else
                constexpr int kFastLedRetries = FASTLED_DEBUG_COUNT_FRAME_RETRIES;
            #endif
            #ifndef NEOPIXEL_HAVE_STATS
                constexpr int kNeoPixelStats = 0;
            #else
                constexpr int kNeoPixelStats = NEOPIXEL_HAVE_STATS;
            #endif
            #ifndef DEBUG_TASK_QUEUE
                constexpr int kTaskQueue = 0;
            #else
                constexpr int kTaskQueue = DEBUG_TASK_QUEUE;
            #endif
            args.print(F("debug: DEBUG_IOT_CLOCK=%u FASTLED_DEBUG_COUNT_FRAME_RETRIES=%u NEOPIXEL_HAVE_STATS=%u DEBUG_TASK_QUEUE=%u"), kDebugClock, kFastLedRetries, kNeoPixelStats, kTaskQueue);
        }
        // tem[perature][,<value>], 0 or no value disables the override
        #if IOT_CLOCK_TEMPERATURE_PROTECTION
            else if (args.startsWithIgnoreCase(0, F("tem"))) {
                const auto value = args.toIntMinMax<uint8_t>(1, 0, 255, 0);
                _tempOverride = value ? std::max<uint8_t>(value, kMinimumTemperatureThreshold) : 0;
                if (_tempOverride) {
                    args.printf_P(PSTR("temperature override %u%s"), _tempOverride, SPGM(UTF8_degreeC));
                }
                else {
                    args.print(F("temperature override disabled"));
                }
            }
        #endif
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
                // the wrapper needs a named String, args.toString() returns a temporary
                auto colorStr = args.toString(2);
                if (StrWrapper(colorStr).trim() == F("0")) {
                    color = 0;
                }
                else if (args.size() > 4) {
                    color = Color(args.toNumber(2, 0), args.toNumber(3, 0), args.toNumber(4, 0x80));
                }
                else {
                    color = Color::fromString(colorStr);
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
