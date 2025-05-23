/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <chrono>
#include <ReadADC.h>
#include <stl_ext/algorithm.h>
#include <KFCForms.h>
#include <WebUISocket.h>
#include <async_web_response.h>
#include "clock.h"
#include "web_server.h"
#include "blink_led_timer.h"
#include "../src/plugins/plugins.h"
#include "../src/plugins/sensor/sensor.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "animation.h"

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

ClockPlugin ClockPlugin_plugin;

#if IOT_LED_STRIP

#    define PLUGIN_OPTIONS_NAME          "led_strip"
#    define PLUGIN_OPTIONS_FRIENDLY_NAME "RGB LED Strip"

#elif IOT_LED_MATRIX

#    define PLUGIN_OPTIONS_NAME          "led_matrix"
#    define PLUGIN_OPTIONS_FRIENDLY_NAME "LED Matrix"

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
    _isLocked(false),
    _display(),
    _lastUpdateTime(0),
    _tempOverride(0),
    _tempBrightness(1.0),
    _fps(0),
    _timerCounter(0),
    _savedBrightness(0),
    _targetBrightness(0),
    _animation(nullptr),
    _blendAnimation(nullptr),
    _method(Clock::ShowMethodType::NONE)
{
    REGISTER_PLUGIN(this, "ClockPlugin");
}

uint8_t getNeopixelShowMethodInt()
{
    return static_cast<uint8_t>(ClockPlugin::getInstance().getShowMethod());
}

MQTT::Json::UnnamedObject ClockPlugin::getWLEDJson()
{
    using MQTT::Json::UnnamedObject;
    using MQTT::Json::NamedObject;
    using MQTT::Json::NamedBool;
    using MQTT::Json::NamedUint32;
    using MQTT::Json::NamedString;
    using MQTT::Json::UnnamedString;
    using MQTT::Json::NamedArray;
    using MQTT::Json::NamedStoredString;
    using MQTT::Json::NamedArray;
    using KFCConfigurationClasses::System;

    UnnamedObject json;

    json.append(
        NamedObject(F("state"),
            NamedBool(F("on"), isEnabled()),
            NamedUint32(F("bri"), _targetBrightness)
        )
    );
    auto mac = WiFi.macAddress();
    mac.replace(F(":"), F(""));
    json.append(
        NamedObject(F("info"),
            NamedString(F("ver"), config.getShortFirmwareVersion()),
            NamedStoredString(F("name"), System::Device::getName()),
            #if ESP8266
                NamedString(F("arch"), F("ESP8622")),
            #elif ESP32
                NamedString(F("arch"), F("ESP32")),
            #endif
            NamedString(F("brand"), F("KFCFW")),
            NamedUint32(F("freeheap"), ESP.getFreeHeap()),
            NamedUint32(F("uptime"), getSystemUptime()),
            NamedStoredString(F("mac"), mac)
        )
    );

    auto effects = NamedArray(F("effects"));
    for(auto i = AnimationType::MIN; i < AnimationType::MAX; i = AnimationType(int(i) + 1)) {
        auto name = _getAnimationName(i);
        if (name) {
            effects.append(UnnamedString(name));
        }
    }
    json.append(effects);

    return json;
}

void ClockPlugin::createMenu()
{
    auto configMenu = bootstrapMenu.getMenuItem(navMenu.config);
    auto subMenu = configMenu.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("Settings"), F(LED_MATRIX_MENU_URI_PREFIX "settings.html"));
    #if ESP32
        subMenu.addMenuItem(F("Animations"), F(LED_MATRIX_MENU_URI_PREFIX "animations.html"));
    #else
        for(auto i = AnimationType::MIN; i < AnimationType::LAST; i = AnimationType(int(i) + 1)) {
            auto name = _getAnimationName(i);
            if (name) {
                subMenu.addMenuItem(PrintString(F("Animation - %s"), name), PrintString(F(LED_MATRIX_MENU_URI_PREFIX "ani-%s.html"), _getAnimationNameSlug(i)));
            }
        }
    #endif
    #if IOT_LED_MATRIX_CONFIGURABLE
        subMenu.addMenuItem(F("Matrix"), F(LED_MATRIX_MENU_URI_PREFIX "matrix.html"));
    #endif
    subMenu.addMenuItem(F("Protection"), F(LED_MATRIX_MENU_URI_PREFIX "protection.html"));
}

void ClockPlugin::_setupTimer()
{
    _Timer(_timer).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr timer) {

        _timerCounter++;

        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
            // __LDBG_printf("current power %.3fW", _powerLevel.average / 1000.0);
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
                                pinMode(IOT_LED_MATRIX_STANDBY_PIN, OUTPUT);
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

    #if PIN_MONITOR

        #if IOT_CLOCK_BUTTON_PIN != -1
            __LDBG_printf("button at pin %u", IOT_CLOCK_BUTTON_PIN);
            pinMonitor.attach<Clock::Button>(IOT_CLOCK_BUTTON_PIN, Clock::ButtonType::MAIN, *this);
        #endif

        #if IOT_CLOCK_TOUCH_PIN != -1
            __LDBG_printf("touch sensor at pin %u", IOT_CLOCK_TOUCH_PIN);
            pinMonitor.attach<Clock::TouchButton>(IOT_CLOCK_TOUCH_PIN, ButtonType::TOUCH, *this);
        #endif

        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            __LDBG_printf("rotary encoder at pin %u,%u active_low=%u", IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB, PIN_MONITOR_ACTIVE_STATE == ActiveStateType::ACTIVE_LOW);
            auto encoder = new Clock::RotaryEncoder(PIN_MONITOR_ACTIVE_STATE);
            encoder->attachPins(IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB);
        #endif

        #if IOT_LED_MATRIX_TOGGLE_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_TOGGLE_PIN, Clock::ButtonType::TOGGLE_ON_OFF, *this);
        #endif

        #if IOT_LED_MATRIX_INCREASE_BRIGHTNESS_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_INCREASE_BRIGHTNESS_PIN, Clock::ButtonType::INCREASE_BRIGHTNESS, *this);
        #endif

        #if IOT_LED_MATRIX_DECREASE_BRIGHTNESS_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_DECREASE_BRIGHTNESS_PIN, Clock::ButtonType::DECREASE_BRIGHTNESS, *this);
        #endif

        #if IOT_LED_MATRIX_NEXT_ANIMATION_PIN != -1
            pinMonitor.attach<Clock::Button>(IOT_LED_MATRIX_NEXT_ANIMATION_PIN, Clock::ButtonType::NEXT_ANIMATION, *this);
        #endif

        if (pinMonitor.getPins().size()) {
            __LDBG_printf("pins attached=%u", pinMonitor.getPins().size());
            pinMonitor.begin();
        }

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
                    #if IOT_LED_MATRIX_STANDBY_PIN != -1
                        digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(true));
                        pinMode(IOT_LED_MATRIX_STANDBY_PIN, OUTPUT);
                    #endif
                    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, OUTPUT);

                    LoopFunctions::remove(standbyLoop);
                    _removeLoop();
                    _fps = NAN;
                }

                uint8_t color2 = (progress * progress * progress * progress) / 1000000;
                uint8_t kBrightnessDivider = 4; // use fill to reduce the brightness to avoid show() to adjust brightness for each pixel
                _display.fill((((100 - progress) / kBrightnessDivider) << 16) | ((color2 / kBrightnessDivider) << 8));
                _display._showNeoPixelEx(255);

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
        readConfig(true);
    }
    else {
        // reset entire state and all pixels
        _disable();
        readConfig(false);
    }

    endIRReceiver();
    beginIRReceiver();

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(_config.fan_speed);
    #endif
    _schedulePublishState = true;
    if (_targetBrightness) {
        _enable();
    }
    _isRunning = true;
}

void ClockPlugin::shutdown()
{
    // store pending changed on shutdown
    if (_saveTimer) {
        _saveTimer->_callback(nullptr);
        _Timer(_saveTimer).remove();
    }

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
        _Timer(_rotaryActionTimer).remove();
    #endif

    #if PIN_MONITOR
        pinMonitor.end();
    #endif

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        WsClient::removeClientCallback(this);
    #endif

    _Timer(_timer).remove();

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

    _removeLoop();

    // turn all LEDs off
    _disable();

    endIRReceiver();

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
    #if IOT_LED_STRIP
        output.print(F("LED Strip Plugin" HTML_S(br)));
    #elif IOT_LED_MATRIX
        output.print(F("LED Matrix Plugin" HTML_S(br)));
    #elif IOT_CLOCK
        output.print(F("Clock Plugin" HTML_S(br)));
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        output.print(F("Fan control, "));
    #endif

    #if IOT_CLOCK_TEMPERATURE_PROTECTION
        if (_config.protection.max_temperature > 25 && _config.protection.max_temperature < 85) {
            output.print(F("Over-temperature protection enabled"));
        }
    #endif

    #if IOT_LED_MATRIX
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u"), _display.size());
    #else
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u, digits pixels %u"), Clock::DisplayType::kNumPixels, Clock::SevenSegment::kNumPixelsDigits);
    #endif

    if (_display.getNumSegments() > 1) {
        output.printf_P(PSTR(", segments %u"), _display.getNumSegments());
    }

    switch(Clock::getNeopixelShowMethodType()) {
        case Clock::ShowMethodType::FASTLED:
            output.printf_P(PSTR(", FastLED %u.%u.%u, %.1ffps, dithering %s"), FASTLED_VERSION / 1000000, (FASTLED_VERSION / 1000) % 1000, FASTLED_VERSION % 1000, _fps, _display.getDither() ? PSTR("on") : PSTR("off"));
            #if FASTLED_VERSION == 3004000 && (IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION)
                {
                    auto limit = FastLED.getPowerLimitScale();
                    if (limit != 1.0f) {
                        output.printf_P(PSTR(HTML_S(br) "Power limit %.1f%%"), limit * 100.0f);
                    }
                }
            #endif

            #if FASTLED_DEBUG_COUNT_FRAME_RETRIES
                extern uint32_t _frame_cnt;
                extern uint32_t _retry_cnt;
                if (_retry_cnt) {
                    output.printf_P(PSTR(", aborted frames %u/%u (%.2f%%)"), _retry_cnt, _frame_cnt, !_frame_cnt ? NAN : (_retry_cnt * 100 / static_cast<float>(_frame_cnt)));
                }
            #endif
            break;
        #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
            case Clock::ShowMethodType::NEOPIXEL_EX: {
                #if NEOPIXEL_HAVE_STATS
                        auto &stats = NeoPixelEx::Context::validate(nullptr).getStats();
                        output.printf_P(PSTR(", NeoPixelEx %dfps, aborted frames %u/%u (%.2f%%)"), stats.getFps(), stats.getAbortedFrames(), stats.getFrames(), stats.getFrames() ? (stats.getAbortedFrames() * 100.0 / stats.getFrames()) : NAN);
                        if (stats.getTime() > 10000) {
                            stats.clear();
                        }
                    #else
                        output.printf_P(PSTR(", NeoPixel"));
                    #endif
                } break;
        #endif
        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
            case Clock::ShowMethodType::AF_NEOPIXEL: {
                    output.printf_P(PSTR(", Adafruit NeoPixel"));
                } break;
        #endif
        default:
            break;
    }

    #if IOT_LED_MATRIX_NO_BUTTON
        // nothing to see here move along
    #elif IOT_CLOCK_BUTTON_PIN != -1
        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            #if IOT_CLOCK_TOUCH_PIN
                output.printf_P(PSTR(HTML_S(br) "Rotary encoder with capacitive touch sensor and button"));
            #else
                output.printf_P(PSTR(HTML_S(br) "Rotary encoder with button"));
            #endif
        #else
        #endif
        auto numPins = int(PinMonitor::pinMonitor.getPins().size());
        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            numPins -= 2;
        #endif
        if (numPins > 0) {
            output.printf_P(PSTR(HTML_S(br) "Total buttons: %u"), numPins);
        }
    #endif

    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
        if (_config.animation == uint8_t(Clock::AnimationType::VISUALIZER)) {
            #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
                if (_config.visualizer.input == uint8_t(Clock::VisualizerType::AudioInputType::MICROPHONE)) {
                    output.printf_P(PSTR(HTML_S(br) "I2S Microphone, Loudness Gain %u, Band Gain %u"), _config.visualizer.mic_loudness_gain, _config.visualizer.mic_band_gain);
                }
                else
            #endif
            {
                output.printf_P(PSTR(HTML_S(br) "UDP%s Active Port %u"), _config.visualizer.multicast ? PSTR(" Multicast") : PSTR(""), _config.visualizer.port);
            }
        }
    #endif

    #if IOT_CLOCK_TEMPERATURE_PROTECTION
        if (isTempProtectionActive()) {
            output.printf_P(PSTR(HTML_SA(h5, HTML_A("class", "text-danger")) "The temperature exceeded %u" HTML_E(h5)), _config.protection.max_temperature);
        }
    #endif

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
        __LDBG_printf("from=%u to=%u partial=%d/%d", _startBrightness, _targetBrightness, std::min<uint32_t>(maxTime, ms * diff / Clock::kMaxBrightness), ms);
        // calculate time relative to the level change
        _fadeTimer.set(std::min<uint32_t>(maxTime, ms * diff / Clock::kMaxBrightness));
        _isFading = true;
        _updateBrightnessSettings();
        // publish new state immediately
        _schedulePublishState = true;
    }
}

void ClockPlugin::setAnimation(AnimationType animation, uint16_t blendTime)
{
    __LDBG_printf("animation=%d blend_time=%u", animation, blendTime);
    if (_animation && _animation->hasBlendSupport()) {
        _blendTime = blendTime;
    }
    else {
        _blendTime = 0;
    }
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
            _setAnimation(new Clock::RainbowAnimationFastLED(*this, _config.rainbow.bpm, _config.rainbow.hue, _config.rainbow.invert_direction));
            break;
        case AnimationType::FLASHING:
            _setAnimation(new Clock::FlashingAnimation(*this, _getColor(), _config.flashing_speed));
            break;
        case AnimationType::FIRE:
            _setAnimation(new Clock::FireAnimation(*this, _config.fire));
            break;
        case AnimationType::PLASMA:
            _setAnimation(new Clock::PlasmaAnimation(*this, _getColor(), _config.plasma));
            break;
        case AnimationType::INTERLEAVED:
            _setAnimation(new Clock::InterleavedAnimation(*this, _getColor(), _config.interleaved.rows, _config.interleaved.cols, _config.interleaved.time));
            break;
        case AnimationType::XMAS:
            _setAnimation(new Clock::XmasAnimation(*this, _config.xmas));
            break;
        case AnimationType::GRADIENT:
            _setAnimation(new Clock::GradientAnimation(*this, _config.gradient));
            break;
        #if IOT_LED_MATRIX_ENABLE_VISUALIZER
            case  AnimationType::VISUALIZER:
                _setAnimation(new Clock::VisualizerAnimation(*this, _getColor(), _config.visualizer));
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
    _config.protection.max_temperature = std::max<uint8_t>(kMinimumTemperatureThreshold, _config.protection.max_temperature);

    _setShowMethod(static_cast<Clock::ShowMethodType>(_config.method));

    // set configured segments
    _display.updateSegments(_config.matrix.pixels0, _config.matrix.offset0, _config.matrix.pixels1, _config.matrix.offset1, _config.matrix.pixels2, _config.matrix.offset2, _config.matrix.pixels3, _config.matrix.offset3);

    // update matrix configuration
    if (!_display.setParams(_config.matrix.rows, _config.matrix.cols, _config.matrix.reverse_rows, _config.matrix.reverse_cols, _config.matrix.rotate, _config.matrix.interleaved, _config.matrix.rowOfs, _config.matrix.colOfs)) {
        __DBG_printf("_display.setParams() failed");
    }

    _display.setDither(_config.dithering);

    #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        __LDBG_printf("limit=%u/%u r/g/b/idle=%u/%u/%u/%u", _config.power_limit, _getPowerLevelLimit(_config.power_limit), _config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
            _powerLevel.clear();
        #endif

        #if FASTLED_VERSION >= 3005000
            if (_getPowerLevelLimit(_config.power_limit) == ~0U) {
                FastLED.m_pPowerFunc = nullptr; // if this does not compile, make m_pPowerFunc public in FastLED.h
            }
            else {
                FastLED.setMaxPowerInMilliWatts(_getPowerLevelLimit(_config.power_limit));
                FastLED.m_pPowerFunc = calcPowerFunction;
            }
        #else
            if (_getPowerLevelLimit(_config.power_limit) == ~0U) {
                FastLED.setMaxPowerInMilliWatts(0);
            }
            else {
                FastLED.setPowerConsumptionInMilliWattsPer256(_config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
                FastLED.setMaxPowerInMilliWatts(_getPowerLevelLimit(_config.power_limit), &calcPowerFunction);
            }
        #endif
    #endif

    // reset temperature protection
    _tempBrightness = 1.0;
    #if IOT_CLOCK_HAVE_OVERHEATED_PIN
        digitalWrite(IOT_CLOCK_HAVE_OVERHEATED_PIN, HIGH);
    #endif

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        if (!_config.standby_led) {
            pinMode(IOT_LED_MATRIX_STANDBY_PIN, INPUT);
        }
    #endif

    #if defined(IOT_LED_MATRIX_STANDBY_LED_PIN) && IOT_LED_MATRIX_STANDBY_LED_PIN != -1
        // enable/disable extra LED that is powered by the 5V from the relay/mosfet
        // 1 pulls the LED to GND, 0 leaves it floating
        pinMode(IOT_LED_MATRIX_STANDBY_LED_PIN, OUTPUT);
        digitalWrite(IOT_LED_MATRIX_STANDBY_LED_PIN, IOT_LED_MATRIX_STANDBY_LED_PIN_LEVEL);
    #endif

    beginIRReceiver();

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
    if (_isLocked) {
        _isLocked = false;
        return;
    }
    if (_isEnabled) {
        __LDBG_printf("enable LED pin=%d state=%u (cfg_enable=%u, is_enabled=%u, config=%u) SKIPPED", IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(true), _config.standby_led, _isEnabled, _config.enabled);
        return;
    }
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        return;
    }

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        if (_config.standby_led) {
            digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(true));
        }
        __LDBG_printf("enable LED pin=%u state=%u (cfg_enable=%u, is_enabled=%u, config=%u)", IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(true), _config.standby_led, _isEnabled, _config.enabled);
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(_config.fan_speed);
    #endif

    enableLoopNoClear(true);

    _config.enabled = true;
    _isEnabled = true;

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        _powerLevel.clear();
    #endif

    #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
        NeoPixelEx::Context::validate(nullptr).getStats().clear();
    #endif
    #if FASTLED_DEBUG_COUNT_FRAME_RETRIES
        extern uint32_t _frame_cnt;
        extern uint32_t _retry_cnt;
        _frame_cnt = 0;
        _retry_cnt = 0;
    #endif
}

void ClockPlugin::_disable()
{
    __LDBG_printf("disable LED pin=%d state=%u (cfg_enabled=%u, is_enabled=%u, config=%u)", IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false), _config.standby_led, _isEnabled, _config.enabled);

    // turn all leds off and set brightness to 0
    _display.setBrightness(0);
    _display.clear();
    _display_show();

    _config.enabled = false;
    _isEnabled = false;

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        _calcPowerFunction(0, 0);
    #endif

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        if (_config.standby_led) {
            digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
        }
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(0);
    #endif

    _removeLoop();
    LOOP_FUNCTION_ADD(standbyLoop);
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

#if IOT_CLOCK_OPTIMIZE_LOOP
#    pragma GCC push_options
#    pragma GCC optimize ("O3")
#endif

void IRAM_ATTR ClockPlugin::_loop()
{
    LoopOptionsType options(*this);
    _display.setBrightness(_getBrightness());

    #if IOT_LED_MATRIX_FASTLED_ONLY == 0
        if (_method == Clock::ShowMethodType::FASTLED)
    #endif
    {
        auto frames = std::clamp<uint32_t>(_fps * 10, 100, 0xffffff);
        auto fps = FastLED.getFPS();
        if (fps) {
            _fps = ((_fps * frames) + fps) / (frames + 1.0);
        }
    }

    if (_animation) {
        _animation->loop(options.getMillis());
    }
    if (_blendAnimation) {
        _blendAnimation->loop(options.getMillis());
    }

    if (options.doUpdate()) {
        _loopDoUpdate(options);
    }

    _display_show();

    #if ESP8266 && HAVE_ESP_ASYNC_WEBSERVER_COUNTERS
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
    #endif
}

// use O3 for the show function on ESP8266 and keep all code in IRAM since it is called very frequently
#if ESP8266
#    pragma GCC push_options
#    pragma GCC optimize ("O3")
#endif

void IRAM_ATTR ClockPlugin::_display_show()
{
    _display.show();
}

#if ESP8266
#   pragma GCC pop_options
#endif

// keep all the animation code in flash memory
void ICACHE_FLASH_ATTR ClockPlugin::_loopDoUpdate(LoopOptionsType &options)
{
    // start update process
    _lastUpdateTime = millis();

    // check if we need to update the animation
    if (options.doRedraw()) {

        #if IOT_LED_MATRIX == 0
            // we can use a boolean here, the compiler seems to optimize the code and removes the if else in the setColons() call
            bool displayColon = true;

            // does the animation allow blinking and is it set?
            if ((!_animation || (_animation && !_animation->isBlinkColonDisabled())) && (_config.blink_colon_speed >= kMinBlinkColonSpeed)) {
                // set on/off depending on the update rate
                displayColon = ((_lastUpdateTime / _config.blink_colon_speed) % 2 == 0);
            }

            auto &tm = options.getLocalTime();
            auto tm_hour = tm.tm_hour_format(); // get hour in 12/24h format

            _display.setDigit(0, tm_hour / 10);
            _display.setDigit(1, tm_hour % 10);
            _display.setDigit(2, tm.tm_min / 10);
            _display.setDigit(3, tm.tm_min % 10);
            #if IOT_CLOCK_NUM_DIGITS == 6
                _display.setDigit(4, tm.tm_sec / 10);
                _display.setDigit(5, tm.tm_sec % 10);
            #endif

            _display.setColons(displayColon ? Clock::SevenSegment::ColonType::BOTH : Clock::SevenSegment::ColonType::NONE);
        #endif

        if (_blendAnimation) {
            // generate new frames for both animations
            _animation->nextFrame(options.getMillis());
            _blendAnimation->nextFrame(options.getMillis());

            // blend frames together into _display
            if (!_blendAnimation->blend(_display, options.getMillis())) {
                __LDBG_printf("blending done");
                delete _blendAnimation;
                _blendAnimation = nullptr;
            }
        }
        else if (_animation) {
            // create next frame and copy it to display
            _animation->nextFrame(options.getMillis());
            _animation->copyTo(_display, options.getMillis());
        }

        #if IOT_LED_MATRIX_GROUP_PIXELS

            if (_config.group_pixels > 0) {

                // number of steps
                const size_t step = _config.group_pixels;
                // number of pixels
                const size_t num = step * 3;
                // fraction to blend of each pixel
                const uint8_t blendFraction = 255 / num;

                // interval in milliseconds to cycle through the 3 pixels
                constexpr uint32_t kInterval = 5000;
                // make sure it is dividable by 256 and more than 1
                constexpr uint32_t kInterval256 = std::max(1, int((kInterval + 255) / 256.0));
                // convert millis into our current relative position
                const uint32_t n = options.getMillis() / kInterval256;

                // scaling state
                const size_t bp = n / 256;
                // scaling value
                const size_t bn = n % 256;

                // scaling values for each pixel
                std::array<uint8_t, 3> scale;
                auto iterator = scale.begin() + (bp % 3);
                *iterator = 0;
                if (++iterator == scale.end()) {
                    iterator = scale.begin();
                }
                *iterator = ~bn;
                if (++iterator == scale.end()) {
                    iterator = scale.begin();
                }
                *iterator = bn;

                // pixels range
                auto pixels = _display.begin();
                const auto endPtr = _display.end();
                while(pixels < endPtr) {
                    // read pixel group and blend all into 3 pixels
                    auto px = pixels;
                    // blend colors
                    auto color = *pixels;
                    *pixels++ = 0;
                    for (size_t j = 1; j < num && pixels < endPtr; j++) {
                        nblend(color, *pixels, blendFraction);
                        *pixels++ = 0;
                    }
                    // pixels is the end pointer for this group
                    if (px < pixels) {
                        // color = 0xff0000;
                        *px = CRGB(color).nscale8(scale[0]);
                    }
                    if ((px = px + step) < pixels) {
                        // color = 0x00ff00;
                        *px = CRGB(color).nscale8(scale[1]);
                    }
                    if ((px = px + step) < pixels) {
                        // color = 0x0000ff;
                        *px = color.nscale8(scale[2]);
                    }
                }
            }
        #endif
    }
}

#if IOT_CLOCK_OPTIMIZE_LOOP
#    pragma GCC pop_options
#endif

void ClockPluginClearPixels()
{
    ClockPlugin::getInstance().clear();
}
