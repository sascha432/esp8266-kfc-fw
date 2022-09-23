/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <chrono>
#include <ReadADC.h>
#include <KFCForms.h>
#include <WebUISocket.h>
#include <async_web_response.h>
#include "clock.h"
#include "web_server.h"
#include "blink_led_timer.h"
#include <stl_ext/algorithm.h>
#include "../src/plugins/plugins.h"
#include "../src/plugins/sensor/sensor.h"
#include "../src/plugins/mqtt/mqtt_json.h"

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#if DEBUG_MEASURE_ANIMATION
Clock::AnimationStats Clock::animationStats;
#endif

ClockPlugin ClockPlugin_plugin;

#if IOT_LED_MATRIX

#    define PLUGIN_OPTIONS_NAME          "led_matrix"
#    define PLUGIN_OPTIONS_FRIENDLY_NAME "LED Matrix"
#    define PLUGIN_OPTIONS_WEB_TEMPLATES ""

#else

#    define PLUGIN_OPTIONS_NAME          "clock"
#    define PLUGIN_OPTIONS_FRIENDLY_NAME "Clock"

#endif

#define PLUGIN_OPTIONS_CONFIG_FORMS                     "settings,animations,protection,matrix,ani-*"

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    ClockPlugin,
    PLUGIN_OPTIONS_NAME,
    PLUGIN_OPTIONS_FRIENDLY_NAME,
    "",
    PLUGIN_OPTIONS_CONFIG_FORMS,
    "",
    PluginComponent::PriorityType::MIN,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

ClockPlugin::ClockPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(ClockPlugin)),
    MQTTComponent(ComponentType::LIGHT),
    _schedulePublishState(false),
    _isFading(false),
    _isEnabled(false),
    _forceUpdate(false),
    _isRunning(false),
    __color(0, 0, 0xff),
    _lastUpdateTime(0),
    _tempOverride(0),
    _tempBrightness(1.0),
    _fps(0),
    _timerCounter(0),
    _savedBrightness(0),
    _targetBrightness(0),
    _animation(nullptr),
    _blendAnimation(nullptr),
    _method(Clock::ShowMethodType::FASTLED)
{
    REGISTER_PLUGIN(this, "ClockPlugin");
}

uint8_t getNeopixelShowMethodInt()
{
    return static_cast<uint8_t>(ClockPlugin::getInstance().getShowMethod());
}

void ClockPlugin::createMenu()
{
    #if IOT_LED_MATRIX
        #define MENU_URI_PREFIX "led-matrix/"
    #else
        #define MENU_URI_PREFIX "clock/"
    #endif

    auto configMenu = bootstrapMenu.getMenuItem(navMenu.config);
    auto subMenu = configMenu.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("Settings"), F(MENU_URI_PREFIX "settings.html"));
    subMenu.addMenuItem(F("Animations"), F(MENU_URI_PREFIX "animations.html"));
    #if IOT_LED_MATRIX_CONFIGURABLE
        subMenu.addMenuItem(F("Matrix"), F(MENU_URI_PREFIX "matrix.html"));
    #endif
    subMenu.addMenuItem(F("Protection"), F(MENU_URI_PREFIX "protection.html"));
}

void ClockPlugin::_setupTimer()
{
    _Timer(_timer).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr timer) {

        _timerCounter++;

        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
            _calcPowerLevel();
            // __LDBG_printf("current power %.3fW", _powerLevelAvg / 1000.0);
            // update power level sensor of the webui
            if (_timerCounter % IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE == 0) {
                _updatePowerLevelWebUI();
            }
        #endif

        #if IOT_CLOCK_TEMPERATURE_PROTECTION
            // temperature protection
            if (_timerCounter % kCheckTemperatureInterval == 0) {
                float tempSensor = 0;
                for(auto sensor: SensorPlugin::getInstance().getSensors()) {
                    if (sensor->getType() == MQTT::SensorType::LM75A) {
                        auto &lm75a = *reinterpret_cast<Sensor_LM75A *>(sensor);
                        tempSensor = std::max(tempSensor, lm75a.readSensor()
                        #if IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS != 255
                            - (lm75a.getAddress() == IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS ? _config.protection.regulator_margin : 0)
                        #endif
                        );
                    }
                }
                if (_tempOverride) {
                    tempSensor = _tempOverride;
                }

                if (tempSensor) {

                    // #if DEBUG_IOT_CLOCK
                    //     if (_timerCounter % 60 == 0) {
                    //         __LDBG_printf("temp=%.1f prot=%u range=%u-%u max=%u", tempSensor, isTempProtectionActive(), _config.protection.temperature_reduce_range.min, _config.protection.temperature_reduce_range.max, _config.protection.max_temperature);
                    //         __LDBG_printf("reduce=%u val=%f br=%f", _config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min, _config.protection.temperature_reduce_range.min - tempSensor, std::clamp<float>(1.0 - ((tempSensor - _config.protection.temperature_reduce_range.min) * 0.75 / (float)(_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min)), 0.25, 1.0));
                    //     }
                    // #endif

                    if (isTempProtectionActive()) {
                        return;
                    }

                    if (tempSensor > _config.protection.max_temperature) {

                        PrintString str;
                        PrintString str2 = F("<span style=\"font-size:1.65rem\">OVERHEATED<br>");

                        for(auto sensor: SensorPlugin::getInstance().getSensors()) {
                            if (sensor->getType() == MQTT::SensorType::LM75A) {
                                auto &lm75a = *reinterpret_cast<Sensor_LM75A *>(sensor);
                                str.printf_P(PSTR("%s %.2f%s "), lm75a.getSensorName().c_str(), lm75a.readSensor(), SPGM(UTF8_degreeC));
                                str2.printf_P(PSTR("%.1f%s"), lm75a.readSensor(), SPGM(UTF8_degreeC));
                            }
                        }
                        _overheatedInfo = std::move(str2);

                        auto message = PrintString(F("Over temperature protection activated: %.1f%s &gt; %u%s"), tempSensor, SPGM(UTF8_degreeC), _config.protection.max_temperature, SPGM(UTF8_degreeC));
                        message += F("<br>");
                        message += str;
                        _setBrightness(0);
                        enableLoop(false);
                        _tempBrightness = -1.0;
                        #if IOT_LED_MATRIX_FAN_CONTROL
                            _setFanSpeed(255);
                        #endif
                        #if IOT_CLOCK_HAVE_OVERHEATED_PIN != -1
                            digitalWrite(IOT_CLOCK_HAVE_OVERHEATED_PIN, LOW);
                            #if IOT_LED_MATRIX_STANDBY_PIN != -1
                                digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
                            #endif
                        #endif

                        Logger_error(message);
                        _schedulePublishState = true;
                    }
                    else {
                        // get current brightness
                        uint8_t oldBrightness = _getBrightness();
                        uint8_t newBrightness = oldBrightness;
                        if (tempSensor > _config.protection.temperature_reduce_range.min) {
                            // reduce brightness to 25-100% or 50% if the range is invalid
                            _tempBrightness = (_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min > 0) ?
                                std::clamp<float>(1.0 - ((tempSensor - _config.protection.temperature_reduce_range.min) * 0.75 / static_cast<float>(_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min)), 0.25, 1.0) :
                                0.5;
                        }
                        else {
                            _tempBrightness = 1.0;
                        }
                        // check if brightness has changed
                        newBrightness = _getBrightness();
                        if (newBrightness != oldBrightness) {
                            __LDBG_printf("temp. brightness factor=%.2f%% brightness=%u->%u", _tempBrightness * 100.0, oldBrightness, newBrightness);
                            // update the temperature protection sensor
                            _schedulePublishState = true;
                        }
                    }
                }
            }
        #endif

        // mqtt update / webui update
        if (_schedulePublishState || (_timerCounter % kUpdateMQTTInterval == 0)) {
            _schedulePublishState = false;
            _publishState();
        }

    });
}

void ClockPlugin::preSetup(SetupModeType mode)
{
    if (mode == SetupModeType::DEFAULT) {
        auto sensorPlugin = PluginComponent::getPlugin<SensorPlugin>(F("sensor"), false);
        if (sensorPlugin) {
            sensorPlugin->getInstance().setAddCustomSensorsCallback(webUIHook);
        }
    }
}

void ClockPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    #if defined(HAVE_IOEXPANDER)
        auto &_PCF8574 = IOExpander::config._device;
        _PCF8574.DDR = 0b00111111;
        _PCF8574.PORT = 0xff;
    #endif

    #if IOT_LED_MATRIX_HAVE_SSD1306
        ssd1306Begin();
    #endif

    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(false));
        pinMode(IOT_LED_MATRIX_ENABLE_PIN, OUTPUT);
    #endif

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
        pinMode(IOT_LED_MATRIX_STANDBY_PIN, OUTPUT);
    #endif

    _disable();

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        WsClient::addClientCallback(webSocketCallback, this);
    #endif

    readConfig(true);
    _targetBrightness = 0;

    #if IOT_CLOCK_BUTTON_PIN != -1

        __LDBG_printf("button at pin %u", IOT_CLOCK_BUTTON_PIN);
        pinMonitor.attach<Clock::Button>(IOT_CLOCK_BUTTON_PIN, 0, *this);

        #if IOT_CLOCK_TOUCH_PIN != -1
            __LDBG_printf("touch sensor at pin %u", IOT_CLOCK_TOUCH_PIN);
            pinMonitor.attach<Clock::TouchButton>(IOT_CLOCK_TOUCH_PIN, 1, *this);
        #endif

        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            __LDBG_printf("rotary encoder at pin %u,%u active_low=%u", IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB, PIN_MONITOR_ACTIVE_STATE == ActiveStateType::ACTIVE_LOW);
            auto encoder = new Clock::RotaryEncoder(PIN_MONITOR_ACTIVE_STATE);
            encoder->attachPins(IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB);
        #endif

        pinMonitor.begin();

    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR || IOT_SENSOR_HAVE_MOTION_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            switch(sensor->getType()) {
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        case SensorPlugin::SensorType::AMBIENT_LIGHT: {
                auto lightSensor = reinterpret_cast<Sensor_AmbientLight *>(sensor);
                if (lightSensor->getId() == 0) {
                    #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR == 1
                        auto config = Sensor_AmbientLight::SensorInputConfig(Sensor_AmbientLight::SensorType::INTERNAL_ADC);
                        config.adc.inverted = IOT_CLOCK_AMBIENT_LIGHT_SENSOR_INVERTED;
                    #elif IOT_CLOCK_AMBIENT_LIGHT_SENSOR == 2
                        auto config = Sensor_AmbientLight::SensorInputConfig(Sensor_AmbientLight::SensorType::TINYPWM);
                        config.tinyPWM = Sensor_AmbientLight::SensorInputConfig::TinyPWM(TINYPWM_I2C_ADDRESS, IOT_CLOCK_AMBIENT_LIGHT_SENSOR_INVERTED);
                    #endif
                    lightSensor->begin(this, config);
                }
            }
            break;
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        case SensorPlugin::SensorType::MOTION:
            reinterpret_cast<Sensor_Motion *>(sensor)->begin(this, IOT_CLOCK_MOTION_SENSOR_PIN, IOT_CLOCK_MOTION_SENSOR_PIN_INVERTED);
            break;
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR || IOT_SENSOR_HAVE_MOTION_SENSOR
                default:
                    break;
            }
        }
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(_config.fan_speed);
    #endif

    MQTT::Client::registerComponent(this);

    enableLoopNoClear(true);

    #if IOT_ALARM_PLUGIN_ENABLED
        AlarmPlugin::setCallback(alarmCallback);
    #endif

    #if IOT_CLOCK_SAVE_STATE
        // set initial state after reset
        switch(_config.getInitialState()) {
            case InitialStateType::OFF:
                // keep device off after reset
                _savedBrightness = _config.getBrightness();
                _config.enabled = false;
                _setBrightness(0);
                break;
            case InitialStateType::ON:
                // turn device on after reset
                _savedBrightness = std::max<uint8_t>(15, _config.getBrightness()); // restore last brightness but at least ~6%
                _config.enabled = true;
                setBrightness(_savedBrightness);
                break;
            case InitialStateType::RESTORE:
                // restore last settings
                _savedBrightness = _config.getBrightness();
                if (_config.enabled) {
                    setBrightness(_savedBrightness);
                }
                else {
                    _setBrightness(0);
                }
                break;
            default:
                break;
        }
    #endif

    _setupTimer();
    _isRunning = true;

    #if IOT_LED_MATRIX_SHOW_UPDATE_PROGRESS

        // show update status during OTA updates
        int progressValue = -1;
        WebServer::Plugin::getInstance().setUpdateFirmwareCallback([this, progressValue](size_t position, size_t size) mutable {
            int progress = position * 100 / size;
            if (progressValue != progress) {
                if (progressValue == -1) {

                    // enable LEDs and clear them to show status
                    #if IOT_LED_MATRIX_ENABLE_PIN != -1
                        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(true));
                    #endif
                    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, OUTPUT);

                    LoopFunctions::remove(standbyLoop);
                    LoopFunctions::remove(loop);
                    _fps = NAN;
                }

                uint8_t color2 = (progress * progress * progress * progress) / 1000000;
                uint8_t kBrightnessDivider = 4; // use fill to reduce the brightness to avoid show() to adjust brightness for each pixel
                _display.fill((((100 - progress) / kBrightnessDivider) << 16) | ((color2 / kBrightnessDivider) << 8));
                NeoPixelEx::StaticStrip::externalShow<IOT_LED_MATRIX_OUTPUT_PIN, NeoPixelEx::DefaultTimings, NeoPixelEx::CRGB>(reinterpret_cast<uint8_t *>(_display.begin()), _display.size() * sizeof(NeoPixelEx::CRGB), 255, NeoPixelEx::Context::validate(nullptr));

                progressValue = progress;
            }
        });

    #endif

}

void ClockPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    _isRunning = false;
    #if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
        _removeDisplayLedTimer();
    #endif
    if (source.startsWith(F("ani-"))) {
        // do not reset just apply new config
        _config.enabled = false;
        _isEnabled = false;
    }
    else {
        // reset entire state and all pixels
        _disable();
    }
    readConfig(false);

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(_config.fan_speed);
    #endif
    _schedulePublishState = true;
    if (_targetBrightness) {
        _enable();
    }
    _isRunning = true;
    #if IOT_CLOCK_SAVE_STATE
        _saveState();
    #endif
}

void ClockPlugin::shutdown()
{
    _isRunning = false;

    #if PIN_MONITOR
        PinMonitor::GPIOInterruptsDisable();
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR || IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            switch(sensor->getType()) {
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        case SensorPlugin::SensorType::MOTION:
            reinterpret_cast<Sensor_Motion *>(sensor)->end();
            break;
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        case SensorPlugin::SensorType::AMBIENT_LIGHT:
            if (reinterpret_cast<Sensor_AmbientLight *>(sensor)->getId() == 0) {
                reinterpret_cast<Sensor_AmbientLight *>(sensor)->end();
            }
            break;
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR || IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                default:
                    break;
            }
        }
    #endif


    #if IOT_CLOCK_SAVE_STATE
        if (_saveTimer) {
            _saveTimer.remove();
            _saveState();
        }
    #endif

    #if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
        if (_displayLedTimer) {
            _removeDisplayLedTimer();
            if (!_targetBrightness || !_config.enabled) {
                _display.delay(250);
            }
        }
    #endif

    #if defined(HAVE_IOEXPANDER)
        auto &_PCF8574 = IOExpander::config._device;
        _PCF8574.DDR = 0b00111111;
        _PCF8574.PORT = 0xff;
    #endif

    #if IOT_CLOCK_HAVE_ROTARY_ENCODER
        _rotaryActionTimer.remove();
    #endif

    #if IOT_CLOCK_BUTTON_PIN != -1
        // pinMonitor.detach(this);
        pinMonitor.end();
    #endif

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        WsClient::removeClientCallback(this);
    #endif

    _timer.remove();

    #if IOT_ALARM_PLUGIN_ENABLED
        _resetAlarm();
        AlarmPlugin::setCallback(nullptr);
    #endif

    if (_targetBrightness && _config.enabled) {
        // limit fade time to 1000ms
        uint16_t max_delay = 1000;
        setBrightness(0, -1, max_delay);
        max_delay += 75;
        while(_targetBrightness && max_delay--) {
            loop();
            _display.delay(10);
        }
    }

    #if IOT_LED_MATRIX_HAVE_SSD1306
        ssd1306End();
    #endif

    LoopFunctions::remove(loop);

    // turn all LEDs off
    _disable();

    if (_animation) {
        delete _animation;
        _animation = nullptr;
    }
    if (_blendAnimation) {
        delete _blendAnimation;
        _blendAnimation = nullptr;
    }
}

void ClockPlugin::getStatus(Print &output)
{
    #if IOT_LED_MATRIX
        output.print(F("LED Matrix Plugin" HTML_S(br)));
    #else
        output.print(F("Clock Plugin" HTML_S(br)));
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        output.print(F("Fan control, "));
    #endif

    if (_config.protection.max_temperature > 25 && _config.protection.max_temperature < 85) {
        output.print(F("Over-temperature protection enabled"));
    }

    #if IOT_LED_MATRIX
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u"), _display.size());
    #else
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u, digits pixels %u"), Clock::DisplayType::kNumPixels, Clock::SevenSegment::kNumPixelsDigits);
    #endif
    switch(static_cast<Clock::ShowMethodType>(getNeopixelShowMethodInt())) {
        case Clock::ShowMethodType::FASTLED:
            output.printf_P(PSTR(", FastLED %.1ffps"), _fps);
            #if FASTLED_DEBUG_COUNT_FRAME_RETRIES
                extern uint32_t _frame_cnt;
                extern uint32_t _retry_cnt;
                output.printf_P(PSTR(", aborted frames %u/%u (%.2f%%)"), _retry_cnt, _frame_cnt, !_frame_cnt ? NAN : (_retry_cnt * 100 / static_cast<float>(_frame_cnt)));
            #endif
            break;
        case Clock::ShowMethodType::NEOPIXEL:
        case Clock::ShowMethodType::NEOPIXEL_REPEAT: {
            #if NEOPIXEL_HAVE_STATS
                    auto &stats = NeoPixelEx::Context::validate(nullptr).getStats();
                    output.printf_P(PSTR(", NeoPixel %dfps, aborted frames %u/%u (%.2f%%)"), stats.getFps(), stats.getAbortedFrames(), stats.getFrames(), stats.getFrames() ? (stats.getAbortedFrames() * 100.0 / stats.getFrames()) : NAN);
                    if (stats.getTime() > 10000) {
                        stats.clear();
                    }
                #else
                    output.printf_P(PSTR(", NeoPixel"));
                #endif
            } break;
        default:
            break;
    }

    #if IOT_CLOCK_BUTTON_PIN != -1
        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            #if IOT_CLOCK_TOUCH_PIN
                output.printf_P(PSTR(HTML_S(br) "Rotary encoder with capacitive touch sensor and button"));
            #else
                output.printf_P(PSTR(HTML_S(br) "Rotary encoder with button"));
            #endif
        #else
            output.printf_P(PSTR(HTML_S(br) "Single button"));
        #endif
    #endif

    if (isTempProtectionActive()) {
        output.printf_P(PSTR(HTML_SA(h5, HTML_A("class", "text-danger")) "The temperature exceeded %u" HTML_E(h5)), _config.protection.max_temperature);
    }
}

void ClockPlugin::setBrightness(uint8_t brightness, int ms, uint32_t maxTime)
{
    if (ms < 0) {
        ms = _config.getFadingTimeMillis();
    }
    __LDBG_printf("brightness=%u fading=%u time=%d", brightness, _isFading, ms);
    if (ms == 0 || maxTime == 0) {
        // use _setBrightness
        _setBrightness(brightness);
    }
    else {
        // _setBrightness is called once the fading is complete
        // enable LEDs now if the brightness is not 0, but wait until fading is complete before disabling them
        if (brightness != 0) {
            _enable();
        }
        _startBrightness = _getFadingBrightness();
        _fadingBrightness = ~_getBrightness();
        _targetBrightness = brightness;
        int16_t diff = (int16_t)_startBrightness - (int16_t)_targetBrightness;
        if (diff < 0) {
            diff = -diff;
        }
        __LDBG_printf("from=%u to=%u time=%d partial=%d", _startBrightness, _targetBrightness, ms, ms * diff / Clock::kMaxBrightness);
        // calculate time relative to the level change
        _fadeTimer.set(std::min<uint32_t>(maxTime, ms * diff / Clock::kMaxBrightness));
        _isFading = true;
        _updateBrightnessSettings();
        // publish new state immediatelly
        _schedulePublishState = true;
    }
}

void ClockPlugin::setAnimation(AnimationType animation, uint16_t blendTime)
{
    __LDBG_printf("animation=%d blend_time=%u", animation, blendTime);
    _blendTime = blendTime;
    #if IOT_LED_MATRIX == 0
        switch(animation) {
            case AnimationType::COLON_SOLID:
                setBlinkColon(0);
                return;
            case AnimationType::COLON_BLINK_SLOWLY:
                setBlinkColon(1000);
                return;
            case AnimationType::COLON_BLINK_FAST:
                setBlinkColon(0);
                return;
            default:
                break;
        }
    #endif
    _config.animation = static_cast<uint8_t>(animation);
    switch(animation) {
        case AnimationType::FADING:
            _setAnimation(new Clock::FadingAnimation(*this, _getColor(), Color().rnd(), _config.fading.speed, _config.fading.delay * 1000, _config.fading.factor.value));
            break;
        case AnimationType::RAINBOW:
            _setAnimation(new Clock::RainbowAnimation(*this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.color));
            break;
        case AnimationType::RAINBOW_FASTLED:
            _setAnimation(new Clock::RainbowAnimationFastLED(*this, _config.rainbow.bpm, _config.rainbow.hue));
            break;
        case AnimationType::FLASHING:
            _setAnimation(new Clock::FlashingAnimation(*this, _getColor(), _config.flashing_speed));
            break;
        case AnimationType::FIRE:
            _setAnimation(new Clock::FireAnimation(*this, _config.fire));
            break;
        case AnimationType::INTERLEAVED:
            _setAnimation(new Clock::InterleavedAnimation(*this, _getColor(), _config.interleaved.rows, _config.interleaved.cols, _config.interleaved.time));
            break;
        case AnimationType::GRADIENT:
            _setAnimation(new Clock::GradientAnimation(*this, _config.gradient));
            break;
        #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
        case  AnimationType::VISUALIZER:
            _setAnimation(new Clock::VisualizerAnimation(*this, _config.visualizer));
            break;
        #endif
        case AnimationType::SOLID:
        default:
            _config.animation = static_cast<uint8_t>(AnimationType::SOLID);
            _setColor(_config.solid_color);
            _setAnimation(new Clock::SolidColorAnimation(*this, _getColor()));
            break;
    }
    _schedulePublishState = true;
}

void ClockPlugin::readConfig(bool setup)
{
    // read config
    _config = Plugins::Clock::getConfig();
    #if IOT_CLOCK_SAVE_STATE
        if (setup) {
            // check if a config state is stored
            auto state = _getState();
            if (state.hasValidData()) {
                _config = state.getConfig();
            }
        }
    #endif
    _config.protection.max_temperature = std::max<uint8_t>(kMinimumTemperatureThreshold, _config.protection.max_temperature);

    _display.setDither(_config.dithering);

    if (!_display.setParams(_config.matrix.rows, _config.matrix.cols, _config.matrix.reverse_rows, _config.matrix.reverse_cols, _config.matrix.rotate, _config.matrix.interleaved, _config.matrix.offset)) {
        __DBG_printf("_display.setParams() failed");
    }

    #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        __LDBG_printf("limit=%u/%u r/g/b/idle=%u/%u/%u/%u", _config.power_limit, _getPowerLevelLimit(_config.power_limit), _config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
        FastLED.setMaxPowerInMilliWatts(_getPowerLevelLimit(_config.power_limit) IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(, &calcPowerFunction));
        FastLED.setPowerConsumptionInMilliwattsPer256(_config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
    #endif

    // reset temperature protection
    _tempBrightness = 1.0;
    #if IOT_CLOCK_HAVE_OVERHEATED_PIN
        digitalWrite(IOT_CLOCK_HAVE_OVERHEATED_PIN, HIGH);
    #endif

    _setColor(_config.solid_color.value);
    _setBrightness(_config.getBrightness(), false);
    setAnimation(_config.getAnimation());
}

void ClockPlugin::_setBrightness(uint8_t brightness, bool useEnable)
{
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        #if IOT_ALARM_PLUGIN_ENABLED
            AlarmPlugin::resetAlarm();
        #endif
        return;
    }
    __LDBG_printf("brightness=%u fading=%u", brightness, _isFading);
    if (useEnable) {
        if (brightness) {
            _enable();
        }
        else {
            _disable();
        }
    }
    _updateBrightnessSettings();
    _targetBrightness = brightness;
    _fadeTimer.disable();
    _forceUpdate = true;
    _isFading = false;
    _schedulePublishState = true;
    #if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
        if (_displayLedTimer) {
            _displayLedTimer->print(PrintString(F("+LED_MATRIX_BRIGHTNESS=%u"), _getBrightness()));
        }
    #endif
}

void ClockPlugin::_enable()
{
    if (_isEnabled) {
        // __LDBG_printf("enable LED pin %u state %u (is_enabled=%u, config=%u) SKIPPED", IOT_LED_MATRIX_ENABLE_PIN, enablePinState(true), _isEnabled, _config.enabled);
        return;
    }
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        return;
    }

    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(true));
        __LDBG_printf("enable LED pin %u state %u (is_enabled=%u, config=%u)", IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(true), _isEnabled, _config.enabled);
    #endif

    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, OUTPUT);

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        if (_config.standby_led) {
            digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
        }
    #endif
    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(_config.fan_speed);
    #endif
    enableLoopNoClear(true);

    _config.enabled = true;
    _isEnabled = true;
    _fps = 0;
}

void ClockPlugin::_disable()
{
    __LDBG_printf("disable LED pin %u state %u (is_enabled=%u, config=%u)", IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(false), _isEnabled, _config.enabled);

    // turn all leds off and set brightness to 0
    _display.setBrightness(0);
    _display.clear();
    _display.show();

    _config.enabled = false;
    _isEnabled = false;
    _fps = NAN;

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        _powerLevelCurrentmW = 0;
        _powerLevelUpdateTimer = 0;
        _powerLevelAvg = 0;
    #endif

    // avoid leakage through the level shifter
    // this will stop the LEDs from getting powered through data line
    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, OUTPUT);
    digitalWrite(IOT_LED_MATRIX_OUTPUT_PIN, LOW);

    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, kEnablePinState(false));
    #endif

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        if (_config.standby_led) {
            digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(true));
        }
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(0);
    #endif

    LoopFunctions::remove(loop);
    LoopFunctions::add(standbyLoop);
}

#if IOT_ALARM_PLUGIN_ENABLED

void ClockPlugin::_alarmCallback(ModeType mode, uint16_t maxDuration)
{
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        _resetAlarm();
        return;
    }
    if (maxDuration == Alarm::STOP_ALARM) {
        _resetAlarm();
        return;
    }

    // alarm reset function already set?
    if (!_resetAlarmFunc) {

        if (_isFading) {
            // stop fading
            // the target brightness will be set after the alarm has been disabled
            _setBrightness(_targetBrightness);
        }

        struct {
            AnimationType animation;
            uint8_t targetBrightness;
            bool autoBrightness;
        } saved = {
            _config.getAnimation(),
            _targetBrightness,
            #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                isAutoBrightnessEnabled()
            #else
                false
            #endif
        };

        __LDBG_printf("storing parameters brightness=%u auto=%d color=%s animation=%u", saved.targetBrightness, saved.autoBrightness, getColor().toString().c_str(), saved.animation);
        _resetAlarmFunc = [this, saved](Event::CallbackTimerPtr timer) {
            #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                setAutoBrightness(saved.autoBrightness);
            #endif
            _targetBrightness = saved.targetBrightness;
            setAnimation(saved.animation);
            __LDBG_printf("restored parameters brightness=%u auto=%d color=%s animation=%u timer=%u", saved.targetBrightness, saved.autoBrightness, getColor().toString().c_str(), saved.animation, (bool)timer);
            timer->disarm();
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer) {
        __LDBG_printf("alarm brightness=%u color=%s", _targetBrightness, getColor().toString().c_str());
        #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
            setAutoBrightness(false);
        #endif
        _targetBrightness = kMaxBrightness;
        _setAnimation(new Clock::FlashingAnimation(*this, _config.alarm.color.value, _config.alarm.speed));
    }

    if (maxDuration == 0) {
        maxDuration = Alarm::DEFAULT_MAX_DURATION;
    }
    // reset time if alarms overlap
    __LDBG_printf("alarm duration %u", maxDuration);
    _Timer(_alarmTimer).add(Event::seconds(maxDuration), false, _resetAlarmFunc);
}

#endif

void ClockPlugin::_loop()
{
    LoopOptionsType options(*this);
    _display.setBrightness(_getBrightness());

    uint16_t frames = std::clamp<uint16_t>(_fps * 10, 100, 65535);
    auto fps = FastLED.getFPS();
    if (fps) {
        _fps = ((_fps * frames) + fps) / (frames + 1.0);
    }

    if (_animation) {
        #if DEBUG_MEASURE_ANIMATION
            auto start = __clock_cycles();
        #endif
        _animation->loop(options.getMillis());
        #if DEBUG_MEASURE_ANIMATION
            auto diff = __clock_cycles() - start;
            Clock::animationStats.loop.add(diff);
        #endif
    }
    if (_blendAnimation) {
        _blendAnimation->loop(options.getMillis());
    }

    if (options.doUpdate()) {

        // start update process
        _lastUpdateTime = millis();

        if (options.doRedraw()) {

            #if IOT_LED_MATRIX == 0
                uint8_t displayColon = true;
                // does the animation allow blinking and is it set?
                if ((!_animation || (_animation && !_animation->isBlinkColonDisabled())) && (_config.blink_colon_speed >= kMinBlinkColonSpeed)) {
                    // set on/off depending on the update rate
                    displayColon = ((_lastUpdateTime / _config.blink_colon_speed) % 2 == 0);
                }

                auto &tm = options.getLocalTime();

                _display.setDigit(0, tm.tm_hour_format() / 10);
                _display.setDigit(1, tm.tm_hour_format() % 10);
                _display.setDigit(2, tm.tm_min / 10);
                _display.setDigit(3, tm.tm_min % 10);
                #if IOT_CLOCK_NUM_DIGITS == 6
                    _display.setDigit(4, tm.tm_sec / 10);
                    _display.setDigit(5, tm.tm_sec % 10);
                #endif

                _display.setColons(displayColon ? Clock::SevenSegment::ColonType::BOTH : Clock::SevenSegment::ColonType::NONE);
            #endif

            if (_blendAnimation) {
                _animation->nextFrame(options.getMillis());
                _blendAnimation->nextFrame(options.getMillis());

                if (!_blendAnimation->blend(_display, options.getMillis())) {
                    __LDBG_printf("blending done");
                    delete _blendAnimation;
                    _blendAnimation = nullptr;
                }
            }
            else if (_animation) {
                #if DEBUG_MEASURE_ANIMATION
                    auto start = __clock_cycles();
                #endif
                _animation->nextFrame(options.getMillis());
                #if DEBUG_MEASURE_ANIMATION
                    auto diff = __clock_cycles() - start;
                    Clock::animationStats.nextFrame.add(diff);
                #endif

                #if DEBUG_MEASURE_ANIMATION
                    start = __clock_cycles();
                #endif
                _animation->copyTo(_display, options.getMillis());
                #if DEBUG_MEASURE_ANIMATION
                    diff = __clock_cycles() - start;
                    Clock::animationStats.copyTo.add(diff);
                #endif
            }
        }
    }

    _display.show();
    if (WebServer::Plugin::getRunningRequests() || WebServer::Plugin::getRunningResponses()) {
        // give system time to process the requests
        // if the cpu is overloaded, the animation will get very choppy and the web server will be slow
        uint32_t count = WebServer::Plugin::getRunningRequestsAndResponses();
        delay(count > 2 ? 50 : count > 1 ? 25 : 5);

        #if DEBUG_IOT_CLOCK
            static uint32_t lastValue = 0;
            uint32_t currentValue;
            if ((currentValue = WebServer::Plugin::getRunningRequestsAndResponsesUint32()) != lastValue) {
                lastValue = currentValue;
                __DBG_printf("requests=%u responses=%u sum=%u _fps=%.1f fps=%u", WebServer::Plugin::getRunningRequests(), WebServer::Plugin::getRunningResponses(), count, _fps, FastLED.getFPS());
            }
        #endif
    }
}

void ClockPluginClearPixels()
{
    ClockPlugin::getInstance().clear();
}
