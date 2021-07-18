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
// #include "../src/plugins/sensor/sensor.h"
#include "../src/plugins/mqtt/mqtt_json.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if DEBUG_MEASURE_ANIMATION
Clock::AnimationStats Clock::animationStats;
#endif

static ClockPlugin plugin;

#if HAVE_PCF8574

void initialize_pcf8574()
{
    // begin sets all pins to high level output and then to pin mode input
    _PCF8574.begin(PCF8574_I2C_ADDRESS, &config.initTwoWire());
    _PCF8574.DDR = 0b00111111;
}

void print_status_pcf8574(Print &output)
{
    output.printf_P(PSTR("PCF8574 @ I2C address 0x%02x"), PCF8574_I2C_ADDRESS);
    output.print(F(HTML_S(br) "Interrupt disabled" HTML_S(br)));
    if (_PCF8574.isConnected()) {
        output.print(HTML_S(small));
        uint8_t ddr = _PCF8574.DDR;
        uint8_t state = _PCF8574.PIN;
        for(uint8_t i = 0; i < 8; i++) {
            output.printf_P(PSTR("%u(%s)=%s "), i, (ddr & _BV(i) ? PSTR("OUT") : PSTR("IN")), (state & _BV(i)) ? PSTR("HIGH") : PSTR("LOW"));
        }
        output.print(HTML_E(small));
    }
    else {
        output.print(F(HTML_S(br) "ERROR - Device not found!"));
    }
}

#endif

#if HAVE_TINYPWM

void print_status_tinypwm(Print &output)
{
    output.printf_P(PSTR("TinyPWM @ I2C address 0x%02x"), TINYPWM_I2C_ADDRESS);
    if (!_TinyPwm.isConnected()) {
        output.print(F(HTML_S(br) "ERROR - Device not found!"));
    }
}

#endif

#if IOT_LED_MATRIX

#define PLUGIN_OPTIONS_NAME                             "led_matrix"
#define PLUGIN_OPTIONS_FRIENDLY_NAME                    "LED Matrix"
#define PLUGIN_OPTIONS_WEB_TEMPLATES                    ""
#define PLUGIN_OPTIONS_CONFIG_FORMS                     "settings,animations,protection"

#else

#define PLUGIN_OPTIONS_NAME                             "clock"
#define PLUGIN_OPTIONS_FRIENDLY_NAME                    "Clock"
#define PLUGIN_OPTIONS_CONFIG_FORMS                     "settings,animations,protection"

#endif

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

Clock::LoopOptionsBase::LoopOptionsBase(ClockPlugin &plugin) :
    // _updateRate(plugin._updateRate),
    _forceUpdate(plugin._forceUpdate),
    _brightness(plugin._getBrightness()),
    _millis(millis()),
    _millisSinceLastUpdate(get_time_diff(plugin._lastUpdateTime, _millis))
{
    if (plugin._isFading && plugin._fadeTimer.reached()) {
        __LDBG_printf("fading=done brightness=%u target_brightness=%u", plugin._getBrightness(), plugin._targetBrightness);
        plugin._setBrightness(plugin._targetBrightness);
        plugin._isFading = false;
        // update mqtt and webui
        plugin._publishState();
    }
}

ClockPlugin::ClockPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(ClockPlugin)),
    MQTTComponent(ComponentType::LIGHT),
    _schedulePublishState(false),
    _isFading(false),
    _isEnabled(false),
    _forceUpdate(false),
    __color(0, 0, 0xff),
    _lastUpdateTime(0),
    _tempOverride(0),
    _tempBrightness(1.0),
    _timerCounter(0),
    _savedBrightness(0),
    _targetBrightness(0),
    _animation(nullptr),
    _blendAnimation(nullptr),
    _running(false),
    _method(Clock::ShowMethodType::FASTLED)
{
    REGISTER_PLUGIN(this, "ClockPlugin");
}

Clock::ShowMethodType ClockPlugin::getShowMethod()
{
    return plugin._method;
}

const __FlashStringHelper *ClockPlugin::getShowMethodStr()
{
    switch(getShowMethod()) {
        case Clock::ShowMethodType::NONE:
            return F("None");
        case Clock::ShowMethodType::FASTLED:
            return F("FastLED");
        case Clock::ShowMethodType::NEOPIXEL:
            return F("NeoPixel");
        case Clock::ShowMethodType::NEOPIXEL_REPEAT:
            return F("NeoPixelRepeat");
        default:
            break;
    }
    return F("Unknown");
}

void ClockPlugin::setShowMethod(Clock::ShowMethodType method)
{
    plugin._method = method;
}

void ClockPlugin::toggleShowMethod()
{
    constexpr auto kFirst = static_cast<int>(Clock::ShowMethodType::NONE);
    constexpr auto kRange = static_cast<int>(Clock::ShowMethodType::MAX) - kFirst;
    auto method = static_cast<uint8_t>(plugin._method);
    method -= kFirst + 1;
    if (method < 0) {
        method = 0;
    }
    else {
        method %= kRange;
    }
    plugin._method = static_cast<Clock::ShowMethodType>(method + kFirst);
}

uint8_t getNeopixelShowMethodInt()
{
    return static_cast<uint8_t>(plugin.getShowMethod());
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
    subMenu.addMenuItem(F("Protection"), F(MENU_URI_PREFIX "protection.html"));
}

#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
{
    return isAutobrightnessEnabled() ?
        (_getFadingBrightness() * (getAutoBrightness()) * (temperatureProtection ? getTempProtectionFactor() : 1.0f)) :
        (temperatureProtection ?
            (_getFadingBrightness() * getTempProtectionFactor()) :
            _getFadingBrightness());
}

#else

uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
{
    return temperatureProtection ?
        (_getFadingBrightness() * getTempProtectionFactor()) :
        _getFadingBrightness();
}

#endif

float ClockPlugin::_getFadingBrightness() const
{
    return (_fadeTimer.isActive() && _fadeTimer.getDelay()) ?
        (_targetBrightness - (((int16_t)_targetBrightness - (int16_t)_startBrightness) / (float)_fadeTimer.getDelay()  * _fadeTimer.getTimeLeft())) :
        _targetBrightness;
}

ClockPlugin::AnimationType ClockPlugin::_getAnimationType(String name)
{
    auto list = Clock::ConfigType::getAnimationNames();
    uint32_t animation = stringlist_ifind_P_P(list, name.c_str(), ',');
    if (animation < static_cast<uint32_t>(AnimationType::MAX)) {
        return static_cast<AnimationType>(animation);
    }
    return AnimationType::MAX;
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
                            _digitalWrite(IOT_CLOCK_HAVE_OVERHEATED_PIN, LOW);
                            #if IOT_LED_MATRIX_STANDBY_PIN != -1
                                _digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
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
                                std::clamp<float>(1.0 - ((tempSensor - _config.protection.temperature_reduce_range.min) * 0.75 / (float)(_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min)), 0.25, 1.0) :
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
    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, enablePinState(false));
        pinMode(IOT_LED_MATRIX_ENABLE_PIN, OUTPUT);
    #endif

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        _digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
        _pinMode(IOT_LED_MATRIX_STANDBY_PIN, OUTPUT);
    #endif

    _disable(5);

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        WsClient::addClientCallback(webSocketCallback, this);
    #endif

    readConfig();
    _targetBrightness = 0;

    #if IOT_LED_MATRIX == 0
        // _setSevenSegmentDisplay();
        #if IOT_CLOCK_PIXEL_SYNC_ANIMATION
            if (isTimeValid()) {
                _isSyncing = false;
            } else {
                setSyncing(true);
            }
            addTimeUpdatedCallback(ntpCallback);
        #endif
    #endif

    #if IOT_CLOCK_BUTTON_PIN != -1
        pinMonitor.begin();

        __LDBG_printf("button at pin %u", IOT_CLOCK_BUTTON_PIN);
        pinMonitor.attach<Clock::Button>(IOT_CLOCK_BUTTON_PIN, 0, *this);
        _pinMode(IOT_CLOCK_BUTTON_PIN, INPUT);

        #if IOT_CLOCK_TOUCH_PIN != -1
            __LDBG_printf("touch sensor at pin %u", IOT_CLOCK_TOUCH_PIN);
            pinMonitor.attach<Clock::TouchButton>(IOT_CLOCK_TOUCH_PIN, 1, *this);
            _pinMode(IOT_CLOCK_TOUCH_PIN, INPUT);
        #endif

        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            __LDBG_printf("rotary encoder at pin %u,%u active_low=%u", IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB, PIN_MONITOR_ACTIVE_STATE == ActiveStateType::ACTIVE_LOW);
            auto encoder = new Clock::RotaryEncoder(PIN_MONITOR_ACTIVE_STATE);
            encoder->attachPins(IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB);
            pinMode(IOT_CLOCK_ROTARY_ENC_PINA, INPUT);
            pinMode(IOT_CLOCK_ROTARY_ENC_PINB, INPUT);
        #endif

        PinMonitor::GPIOInterruptsEnable();
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            auto lightSensor = reinterpret_cast<Sensor_AmbientLight *>(sensor);
            if (sensor->getType() == SensorPlugin::SensorType::AMBIENT_LIGHT && lightSensor->getId() == 0) {
                #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR == 1
                    auto config = Sensor_AmbientLight::SensorConfig(Sensor_AmbientLight::SensorType::INTERNAL_ADC);
                    config.adc.inverted = IOT_CLOCK_AMBIENT_LIGHT_SENSOR_INVERTED;
                #elif IOT_CLOCK_AMBIENT_LIGHT_SENSOR == 2
                    auto config = Sensor_AmbientLight::SensorConfig(Sensor_AmbientLight::SensorType::TINYPWM);
                    config.tinyPWM = Sensor_AmbientLight::SensorConfig::TinyPWM(TINYPWM_I2C_ADDRESS);
                #endif
                lightSensor->begin(this, config);
            }
            // #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2
            //     else if (sensor->getType() == SensorPlugin::SensorType::AMBIENT_LIGHT && lightSensor->getId() == 1) {
            //         auto config = Sensor_AmbientLight::SensorConfig(Sensor_AmbientLight::SensorType::BH1750FVI);
            //         config.bh1750FVI = Sensor_AmbientLight::SensorConfig::BH1750FVI(Sensor_AmbientLight::BH1750FVI_ADDR_LOW, true);
            //         lightSensor->begin(&_lightSensor2, config);
            //     }
            // #endif
        }
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            if (sensor->getType() == SensorPlugin::SensorType::MOTION) {
                reinterpret_cast<Sensor_Motion *>(sensor)->begin(this, IOT_CLOCK_MOTION_SENSOR_PIN, false);
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
        auto state = _getState();
        if (state.hasValidData()) {
            switch(_config.getInitialState()) {
                case InitialStateType::OFF:
                    // start with clock turned off and the power button sets the configured brightness and animation
                    _savedBrightness = _config.getBrightness();
                    _config.enabled = false;
                    _setBrightness(0);
                    break;
                case InitialStateType::ON:
                    // start with clock turned on using default settings
                    _savedBrightness = _config.getBrightness();
                    _config.enabled = true;
                    setBrightness(_savedBrightness);
                    break;
                case InitialStateType::RESTORE:
                    // restore last settings
                    _config = state.getConfig();
                    _savedBrightness = _config.getBrightness();
                    if (_config.enabled) {
                        setBrightness(_savedBrightness);
                    }
                    else {
                        _setBrightness(0);
                    }
                    break;
                case InitialStateType::MAX:
                    break;
            }
            setAnimation(state.getConfig().getAnimation(), 0);
        }
    #endif

    _setupTimer();

    _running = true;
}

void ClockPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    _running = false;
    #if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
        _removeDisplayLedTimer();
    #endif
    _disable(5);
    readConfig();

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(_config.fan_speed);
    #endif
    #if IOT_CLOCK_SAVE_STATE
        _saveState();
    #endif
    _schedulePublishState = true;
    if (_targetBrightness) {
        _enable();
    }
    _running = true;
}

void ClockPlugin::shutdown()
{
    _running = false;

    #if PIN_MONITOR
        PinMonitor::GPIOInterruptsDisable();
    #endif

    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            if (sensor->getType() == SensorPlugin::SensorType::MOTION) {
                reinterpret_cast<Sensor_Motion *>(sensor)->end();
                break;
            }
        }
    #endif

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        for(const auto &sensor: SensorPlugin::getSensors()) {
            if (sensor->getType() == SensorPlugin::SensorType::AMBIENT_LIGHT && reinterpret_cast<Sensor_AmbientLight *>(sensor)->getId() == 0) {
                reinterpret_cast<Sensor_AmbientLight *>(sensor)->end();
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

    #if HAVE_PCF8574
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
        max_delay += 50;
        while(_targetBrightness && max_delay--) {
            loop();
            _display.delay(1);
        }
    }

    LoopFunctions::remove(loop);

    // turn all LEDs off
    _disable(5);

    delete _animation;
    delete _blendAnimation;
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
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u"), Clock::DisplayType::kNumPixels);
    #else
        output.printf_P(PSTR(HTML_S(br) "Total pixels %u, digits pixels %u"), Clock::DisplayType::kNumPixels, Clock::SevenSegment::kNumPixelsDigits);
    #endif

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
        output.printf_P(PSTR(HTML_S(br) "The temperature exceeded %u"), _config.protection.max_temperature);
    }
}

void ClockPlugin::loop()
{
    plugin._loop();
}

#if !IOT_LED_MATRIX

void ClockPlugin::setBlinkColon(uint16_t value)
{
    if (value < kMinBlinkColonSpeed) {
        value = 0;
    }
    _config.blink_colon_speed = value;
    _schedulePublishState = true;
    __LDBG_printf("blinkcolon=%u update_rate=%u", value, value);
}

#endif

#if IOT_CLOCK_PIXEL_SYNC_ANIMATION

void ClockPlugin::ntpCallback(time_t now)
{
    plugin.setSyncing(false);
}

void ClockPlugin::setSyncing(bool sync)
{
    __LDBG_printf("sync=%u", sync);
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        return;
    }
    if (sync != _isSyncing) {
        _isSyncing = sync;
        _time = 0;
        _display.clear();
        _updateRate = 100;
        __LDBG_printf("update_rate=%u", _updateRate);
        _forceUpdate = true;
    }
}
#endif

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

void ClockPlugin::setColorAndRefresh(Color color)
{
    __LDBG_printf("color=%s", color.toString().c_str());
    _setColor(color);
    _forceUpdate = true;
    _schedulePublishState = true;
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
            if (_config.rainbow.hue == 0) {
                _setAnimation(new Clock::RainbowAnimation(*this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.color));
            }
            else {
                _setAnimation(new Clock::RainbowAnimationFastLED(*this, _config.rainbow.bpm, _config.rainbow.hue));
            }
            break;
        case AnimationType::FLASHING:
            _setAnimation(new Clock::FlashingAnimation(*this, _getColor(), _config.flashing_speed));
            break;
        case  AnimationType::FIRE:
            _setAnimation(new Clock::FireAnimation(*this, _config.fire));
            break;
        case  AnimationType::INTERLEAVED:
            _setAnimation(new Clock::InterleavedAnimation(*this, _getColor(), _config.interleaved.rows, _config.interleaved.cols, _config.interleaved.time));
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

void ClockPlugin::_setColor(uint32_t color, bool updateAnimation)
{
    __color = color;
    if (_config.getAnimation() == AnimationType::SOLID) {
        _config.solid_color = __color;
    }
    #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
        else if (_config.getAnimation() == AnimationType::VISUALIZER) {
            _config.visualizer.color = __color;
        }
    #endif
    if (updateAnimation && _animation) {
            _animation->setColor(__color);
    }
}

uint32_t ClockPlugin::_getColor() const
{
    return __color;
}

void ClockPlugin::readConfig()
{
    _config = Plugins::Clock::getConfig();
    _config.rainbow.multiplier.value = std::clamp<float>(_config.rainbow.multiplier.value, 0.1, 100.0);
    _config.protection.max_temperature = std::max<uint8_t>(kMinimumTemperatureThreshold, _config.protection.max_temperature);
    #if IOT_CLOCK_USE_DITHERING
        _display.setDither(_config.dithering);
    #else
        _display.setDither(false);
    #endif

    #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        __DBG_printf("limit=%u/%u r/g/b/idle=%u/%u/%u/%u", _config.power_limit, _getPowerLevelLimit(_config.power_limit), _config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
        FastLED.setMaxPowerInMilliWatts(_getPowerLevelLimit(_config.power_limit) IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(, &calcPowerFunction));
        FastLED.setPowerConsumptionInMilliwattsPer256(_config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
    #endif

    // reset temperature protection
    _tempBrightness = 1.0;
    #if IOT_CLOCK_HAVE_OVERHEATED_PIN
        _digitalWrite(IOT_CLOCK_HAVE_OVERHEATED_PIN, HIGH);
    #endif

    _setColor(_config.solid_color.value);
    _setBrightness(_config.getBrightness(), false);
    setAnimation(_config.getAnimation());
}

Clock::ConfigType &ClockPlugin::getWriteableConfig()
{
    auto &cfg = Plugins::Clock::getWriteableConfig();
    cfg = _config;
    return cfg;
}

void ClockPlugin::_setAnimation(Clock::Animation *animation)
{
    __LDBG_printf("animation=%p _ani=%p _blend_ani=%p", animation, _animation, _blendAnimation);
    if (_animation) {
        _setBlendAnimation(animation);
    }
    else {
        _animation = animation;
        _animation->begin();
    }
}

void ClockPlugin::_setBlendAnimation(Clock::Animation *blendAnimation)
{
    __LDBG_printf("blend=%p", blendAnimation);
    // we do not support blending more than 2 animations
    // if switched to quickly it will stop and continue with the new one
    if (_blendAnimation) {
        delete _blendAnimation;
    }
    _blendAnimation = new Clock::BlendAnimation(_animation, blendAnimation, _display, _blendTime);
    if (!_blendAnimation) {
        delete _animation;
        _animation = blendAnimation;
        return;
    }
    _blendAnimation->begin();
}

#if IOT_SENSOR_HAVE_MOTION_SENSOR && IOT_SENSOR_HAVE_MOTION_AUTO_OFF

void ClockPlugin::eventMotionDetected(bool motion)
{
    _digitalWrite(131, !motion);
}

bool ClockPlugin::eventMotionAutoOff(bool off)
{
    if (off && _isEnabled) {
        _disable();
        // mark as deactivated by the motion sensor
        _autoOff = true;
        return true;
    }
    if (!off && !_isEnabled && _autoOff) {
        // reactivate if disabled by the motion sensor
        _enable();
        return true;
    }
    return false;
}

#endif

#if IOT_LED_MATRIX_FAN_CONTROL

void ClockPlugin::_setFanSpeed(uint8_t speed)
{
#if DEBUG_IOT_CLOCK
    auto setSpeed = speed;
#endif
    if (speed < _config.min_fan_speed) {
        speed = 0;
    }
    else {
        speed = std::min<uint8_t>(speed, _config.max_fan_speed);
    }
    if (_TinyPwm.analogWrite(_TinyPwm.PB1, speed)) {
        _fanSpeed = speed;
    }
#if DEBUG_IOT_CLOCK
    __DBG_printf("set %u speed %u result %u", setSpeed, speed, _fanSpeed);
#endif
}

#endif

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

void ClockPlugin::_updateBrightnessSettings()
{
    if (_targetBrightness != 0) {
        __LDBG_printf("saved=%u set=%u", _savedBrightness, _targetBrightness);
        _savedBrightness = _targetBrightness;
        _config.setBrightness(_targetBrightness);
        // __LDBG_assert_printf(_config.enabled, "_config.enabled not true");
        // _config.enabled = true;
    }
    else {
        // __LDBG_assert_printf(!_config.enabled, "_config.enabled not false");
        // _config.enabled = false;
    }
}

void ClockPluginClearPixels()
{
    plugin.clear();
}

void ClockPlugin::_reset()
{
    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, enablePinState(true));
    #endif
    NeoPixelEx::forceClear(IOT_LED_MATRIX_OUTPUT_PIN, IOT_CLOCK_NUM_PIXELS);
    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, enablePinState(false));
    #endif
    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, INPUT);
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
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, enablePinState(true));
        __LDBG_printf("enable LED pin %u state %u (is_enabled=%u, config=%u)", IOT_LED_MATRIX_ENABLE_PIN, enablePinState(true), _isEnabled, _config.enabled);
    #endif

    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, OUTPUT);

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        if (_config.standby_led) {
            _digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
        }
    #endif
    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(_config.fan_speed);
    #endif
    enableLoopNoClear(true);

    _config.enabled = true;
    _isEnabled = true;
    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        _autoOff = false;
    #endif
}

void ClockPlugin::_disable(uint8_t delayMillis)
{
    __LDBG_printf("disable LED pin %u state %u (is_enabled=%u, config=%u)", IOT_LED_MATRIX_ENABLE_PIN, enablePinState(false), _isEnabled, _config.enabled);

    // turn all leds off and set brightness to 0
    _display.setBrightness(0);
    _display.clear();
    _display.show();

    _config.enabled = false;
    _isEnabled = false;
    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        _autoOff = false;
    #endif

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        _powerLevelCurrentmW = 0;
        _powerLevelUpdateTimer = 0;
        _powerLevelAvg = 0;
    #endif

    // avoid leakage through the level shifter
    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, INPUT);

    #if IOT_LED_MATRIX_ENABLE_PIN != -1
        digitalWrite(IOT_LED_MATRIX_ENABLE_PIN, enablePinState(false));
    #endif

    #if IOT_LED_MATRIX_STANDBY_PIN != -1
        if (_config.standby_led) {
            _digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(true));
        }
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        _setFanSpeed(0);
    #endif

    LoopFunctions::remove(loop);
    LoopFunctions::add(standbyLoop);

    // artificial delay to avoid switching on/off too quickly
    delay(delayMillis);
}

ClockPlugin &ClockPlugin::getInstance()
{
    return plugin;
}

#if IOT_ALARM_PLUGIN_ENABLED

void ClockPlugin::alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
{
    plugin._alarmCallback(mode, maxDuration);
}

void ClockPlugin::_alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
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
                isAutobrightnessEnabled()
            #else
                false
            #endif
        };

        __LDBG_printf("storing parameters brightness=%u auto=%d color=%s animation=%u", saved.targetBrightness, saved.autoBrightness, getColor().toString().c_str(), saved.animation);
        _resetAlarmFunc = [this, saved](Event::CallbackTimerPtr timer) {
            #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
                setAutobrightness(saved.autoBrightness);
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
            setAutobrightness(false);
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

bool ClockPlugin::_resetAlarm()
{
    __LDBG_printf("alarm_func=%u alarm_state=%u", _resetAlarmFunc ? 1 : 0, AlarmPlugin::getAlarmState());
    if (_resetAlarmFunc) {
        // reset prior clock settings
        _resetAlarmFunc(*_timer);
        AlarmPlugin::resetAlarm();
        _schedulePublishState = true;
        return true;
    }
    return false;
}

#endif

#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION || IOT_CLOCK_HAVE_POWER_LIMIT

uint8_t ClockPlugin::calcPowerFunction(uint8_t scale, uint32_t data)
{
    return plugin._calcPowerFunction(scale, data);
}

void ClockPlugin::webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id)
{
    if (plugin._running) {
        plugin._webSocketCallback(type, client, server, id);
    }
}

float ClockPlugin::__getPowerLevel(float P, float min) const
{
    return std::max<float>(IOT_CLOCK_POWER_CORRECTION_OUTPUT);
}

uint32_t ClockPlugin::_getPowerLevelLimit(uint32_t P_Watt) const
{
    if (P_Watt == 0) {
        return ~0U; // unlimited
    }
    return P_Watt * 1090;
    // return std::max<float>(0, IOT_CLOCK_POWER_CORRECTION_LIMIT);
}

#endif

#if IOT_LED_MATRIX == 0

Clock::ClockLoopOptions::ClockLoopOptions(ClockPlugin &plugin) :
    LoopOptionsBase(plugin),
    _time(plugin._time),
    _now(time(nullptr)),
    _tm({}),
    format_24h(plugin._config.time_format_24h)
{
}

struct Clock::ClockLoopOptions::tm24 &Clock::ClockLoopOptions::getLocalTime(time_t *nowPtr)
{
    if (!nowPtr) {
        nowPtr = &_now;
    }
    _tm = localtime(nowPtr);
    _tm.set_format_24h(format_24h);
    return _tm;
}


#if IOT_CLOCK_PIXEL_SYNC_ANIMATION

#error NOT UP TO DATE

bool ClockPlugin::_loopSyncingAnimation(LoopOptionsType &options)
{
    if (!_isSyncing) {
        return false;
    }
    // __LDBG_printf("isyncing=%u", _isSyncing);

    //     if (_isSyncing) {
//         if (get_time_diff(_lastUpdateTime, millis()) >= 100) {
//             _lastUpdateTime = millis();

// #if IOT_CLOCK_PIXEL_SYNC_ANIMATION
//             #error crashing

//             // show syncing animation until the time is valid
//             if (_pixelOrder) {
//                 if (++_isSyncing > IOT_CLOCK_PIXEL_ORDER_LEN) {
//                     _isSyncing = 1;
//                 }
//                 for(uint8_t i = 0; i < SevenSegmentDisplay::getNumDigits(); i++) {
//                     _display.rotate(i, _isSyncing - 1, _color, _pixelOrder.data(), _pixelOrder.size());
//                 }
//             }
//             else {
//                 if (++_isSyncing > SevenSegmentPixel_DIGITS_NUM_PIXELS) {
//                     _isSyncing = 1;
//                 }
//                 for(uint8_t i = 0; i < _display->numDigits(); i++) {
//                     _display->rotate(i, _isSyncing - 1, _color, nullptr, 0);
//
//             }
// #endif
//             _display.show();
//         }
//         return;
//     }

    // show 88:88:88 instead of the syncing animation
    uint32_t color = _color;
    _display.setParams(options.getMillis(), _getBrightness());
    _display.setDigit(0, 8, color);
    _display.setDigit(1, 8, color);
    _display.setDigit(2, 8, color);
    _display.setDigit(3, 8, color);
#        if IOT_CLOCK_NUM_DIGITS == 6
    _display.setDigit(4, 8, color);
    _display.setDigit(5, 8, color);
#        endif
    _display.setColon(0, SevenSegmentDisplay::BOTH, color);
#        if IOT_CLOCK_NUM_COLONS == 2
    _display.setColon(1, SevenSegmentDisplay::BOTH, color);
#        endif
    _display.show();
    return true;
}

#endif

#endif

    static constexpr int abc  = Clock::DisplayType::kMappingTypeId;
void ClockPlugin::_loop()
{
    LoopOptionsType options(*this);
    _display.setBrightness(_getBrightness());

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

        #if IOT_CLOCK_PIXEL_SYNC_ANIMATION
            if (_loopSyncingAnimation(options)) {
                // ...
            }
            else
        #endif
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
}
