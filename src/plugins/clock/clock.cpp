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
#include <WebUIAlerts.h>
#include <async_web_response.h>
#include "clock.h"
#include "blink_led_timer.h"
#include <stl_ext/algorithm.h>
#include "../src/plugins/mqtt/mqtt_client.h"
#include "../src/plugins/ntp/ntp_plugin.h"
#include "../src/plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static ClockPlugin plugin;

#if HAVE_PCF8574

void initialize_pcf8574()
{
    _PCF8574.begin(PCF8574_I2C_ADDRESS);
    _PCF8574.DDR = 0b00111111;
    _pinMode(_PCF8574Range::pin2DigitalPin(6), INPUT);
    _pinMode(_PCF8574Range::pin2DigitalPin(7), INPUT);
}

void print_status_pcf8574(Print &output)
{
    output.printf_P(PSTR("PCF8574 @ I2C address 0x%02x"), PCF8574_I2C_ADDRESS);
    output.print(F(HTML_S(br) "Interrupt disabled" HTML_S(br)));
    if (_PCF8574.isConnected()) {
        uint8_t ddr = _PCF8574.DDR;
        uint8_t state = _PCF8574.PIN;
        for(uint8_t i = 0; i < 8; i++) {
            output.printf_P(PSTR("%u(%s)=%s "), i, (ddr & _BV(i) ? PSTR("OUTPUT") : PSTR("INPUT")), (state & _BV(i)) ? PSTR("HIGH") : PSTR("LOW"));
        }
    }
    else {
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

#define PLUGIN_OPTIONS_WEB_TEMPLATES                    ""

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#define PLUGIN_OPTIONS_RECONFIGURE_DEPENDENCIES         "http,mqtt"
#else
#define PLUGIN_OPTIONS_RECONFIGURE_DEPENDENCIES         "mqtt"
#endif

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    ClockPlugin,
    PLUGIN_OPTIONS_NAME,
    PLUGIN_OPTIONS_FRIENDLY_NAME,
    PLUGIN_OPTIONS_WEB_TEMPLATES,
    PLUGIN_OPTIONS_CONFIG_FORMS,
    PLUGIN_OPTIONS_RECONFIGURE_DEPENDENCIES,
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
    _doRefresh(plugin._isFading && _brightness != plugin._fadingBrightness),
    _millis(millis()),
    _millisSinceLastUpdate(get_time_diff(plugin._lastUpdateTime, _millis))
{
    if (plugin._isFading && plugin._fadeTimer.reached()) {
        __LDBG_printf("fading=done brightness=%u target_brightness=%u", plugin._getBrightness(), plugin._targetBrightness);
        plugin._setBrightness(plugin._targetBrightness);
        plugin._isFading = false;
        _doRefresh = true;
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
    _blendAnimation(nullptr)
{
    REGISTER_PLUGIN(this, "ClockPlugin");
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

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR

bool ClockPlugin::_loopDisplayLightSensor(LoopOptionsType &options)
{
    if (_displaySensor == DisplaySensorType::OFF) {
        return false;
    }
    _forceUpdate = false;
    if (_displaySensor == DisplaySensorType::SHOW) {
        _displaySensor = DisplaySensorType::BUSY;

        // request to read the ADC 10 times every 25ms ~ 250ms
        ADCManager::getInstance().requestAverage(10, 25000, [this](const ADCManager::ADCResult &result) {

            if (
                result.isInvalid() ||                               // adc queue has been aborted
                _displaySensor != DisplaySensorType::BUSY           // disabled or something went wrong
            ) {
                __LDBG_printf("ADC callback: display sensor value=%u result=%u", _displaySensor, result.isValid());
                return;
            }
            _displaySensor = DisplaySensorType::SHOW; // set back from BUSY to SHOW

            auto str = PrintString(F("% " __STRINGIFY(IOT_CLOCK_NUM_DIGITS) "u"), result.getValue()); // left padded with spaces
            // replace space with #
            String_replace(str, ' ', '#');
            // disable any animation, set color to green and brightness to 50%

            _display.fill(Color(0, 0xff, 0));
            _display.print(str);
            _display.show(kMaxBrightness / 2);
        });
    }
    return true;
}

void ClockPlugin::adjustAutobrightness(Event::CallbackTimerPtr timer)
{
    plugin._adjustAutobrightness();
}

void ClockPlugin::_adjustAutobrightness()
{
    if (_autoBrightness != kAutoBrightnessOff) {
        float value = _readLightSensor() / (float)_autoBrightness;
        if (value > 1) {
            value = 1;
        }
        else if (value < 0.10) {
            value = 0.10;
        }
        if (_autoBrightnessValue == 0) {
            _autoBrightnessValue = value;
        }
        else {
            constexpr float period = 2500 / kAutoBrightnessInterval;
            constexpr float count = period + 1;
            _autoBrightnessValue = ((_autoBrightnessValue * period) + value) / count; // integrate over 2.5 seconds to get a smooth transition and avoid flickering
        }
    }
}

String ClockPlugin::_getLightSensorWebUIValue()
{
    if (_autoBrightness == kAutoBrightnessOff) {
        return PrintString(F("<strong>OFF</strong><br> <span class=\"light-sensor-value\">Sensor value %u</span>"), _readLightSensor());
    }
    return PrintString(F("%.0f &#37;"), _autoBrightnessValue * 100.0);
}

void ClockPlugin::_updateLightSensorWebUI()
{
    if (WsWebUISocket::getWsWebUI() && WsWebUISocket::hasClients(WsWebUISocket::getWsWebUI())) {
        JsonUnnamedObject json(2);
        json.add(JJ(type), JJ(ue));
        auto &events = json.addArray(JJ(events), 1);
        auto &obj = events.addObject(3);
        obj.add(JJ(id), FSPGM(light_sensor, "light_sensor"));
        obj.add(JJ(value), _getLightSensorWebUIValue());
        obj.add(JJ(state), true);

        WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
    }
}

uint16_t ClockPlugin::_readLightSensor() const
{
    return ADCManager::getInstance().readValue();
}

uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
{
    return (_autoBrightness == kAutoBrightnessOff) ?
        _getFadingBrightness() :
        (_getFadingBrightness() * _autoBrightnessValue * (temperatureProtection ? getTempProtectionFactor() : 1.0f));
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
    String list = Clock::Config_t::getAnimationNames();
    list.toLowerCase();
    name.toLowerCase();
    uint32_t animation = stringlist_find_P_P(list.c_str(), name.c_str(), ',');
    if (animation < static_cast<uint32_t>(AnimationType::MAX)) {
        return static_cast<AnimationType>(animation);
    }
    return AnimationType::MAX;
}

void ClockPlugin::_setupTimer()
{
    _Timer(_timer).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr timer) {

        _timerCounter++;

        IF_IOT_CLOCK_HAVE_MOTION_SENSOR(
            auto state = _digitalRead(IOT_CLOCK_HAVE_MOTION_SENSOR_PIN);
            if (state != _motionState) {
                MQTTClient::safePublish(MQTTClient::formatTopic(F("motion")), true, String(_motionState ? 0 : 1));
                // _digitalWrite(_PCF8574Range::pin2DigitalPin(5), !_motionState);

                _motionState = state;
                if (_motionState) {
                    __LDBG_printf("motion detected: last update %u ms ago", _motionLastUpdate ? millis() - _motionLastUpdate : 0);
                    _motionLastUpdate = millis();
                }
            }
        )

        IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
            // update light sensor webui
            if (_timerCounter % kUpdateAutobrightnessInterval == 0) {
                uint8_t tmp = _autoBrightnessValue * 100;
                if (tmp != _autoBrightnessLastValue) {
                    __LDBG_printf("auto brightness %f", _autoBrightnessValue);
                    _autoBrightnessLastValue = tmp;
                    // check if we will publish the state in this loop
                    if (!_schedulePublishState) {
                        _updateLightSensorWebUI();
                    }
                }
            }
        )

        IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(
            _calcPowerLevel();
            // __LDBG_printf("current power %.3fW", _powerLevelAvg / 1000.0);
            // update power level sensor of the webui
            if (_timerCounter % IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE == 0) {
                _updatePowerLevelWebUI();
            }
        )

        // temperature protection
        if (_timerCounter % kCheckTemperatureInterval == 0) {
            float tempSensor;
            if (_tempOverride) {
                tempSensor = _tempOverride;
            }
            else {
                tempSensor = 0;
                SensorPlugin::for_each<Sensor_LM75A>(nullptr, [](MQTTSensor &sensor, Sensor_LM75A &) {
                    return (sensor.getType() == MQTTSensor::SensorType::LM75A);
                }, [this, &tempSensor](Sensor_LM75A &sensor) {

                    tempSensor = max(tempSensor, sensor.readSensor() - (sensor.getAddress() == IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS ? _config.protection.regulator_margin : 0));
                });
            }

            if (tempSensor) {

                #if DEBUG_IOT_CLOCK
                    if (_timerCounter % 60 == 0) {
                        __LDBG_printf("temp=%.1f prot=%u range=%u-%u max=%u", tempSensor, isTempProtectionActive(), _config.protection.temperature_reduce_range.min, _config.protection.temperature_reduce_range.max, _config.protection.max_temperature);
                        __LDBG_printf("reduce=%u val=%f br=%f", _config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min, _config.protection.temperature_reduce_range.min - tempSensor, std::clamp<float>(1.0 - ((tempSensor - _config.protection.temperature_reduce_range.min) * 0.75 / (float)(_config.protection.temperature_reduce_range.max - _config.protection.temperature_reduce_range.min)), 0.25, 1.0));
                    }
                #endif

                if (isTempProtectionActive()) {
                    return;
                }

                if (tempSensor > _config.protection.max_temperature) {
                    auto message = PrintString(F("Over temperature protection activated: %.1f&deg;C &gt; %u&deg;C"), tempSensor, _config.protection.max_temperature);
                    _setBrightness(0);
                    enableLoop(false);
                    _tempBrightness = -1.0;
                    WebAlerts::Alert::error(message);
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

        // mqtt update / webui update
        if (_schedulePublishState || (_timerCounter % kUpdateMQTTInterval == 0)) {
            _schedulePublishState = false;
            _publishState();
        }

    });
}


#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

void ClockPlugin::preSetup(SetupModeType mode)
{
    if (mode == SetupModeType::DEFAULT) {
        auto sensorPlugin = PluginComponent::getPlugin<SensorPlugin>(F("sensor"), false);
        if (sensorPlugin) {
            sensorPlugin->getInstance().setAddCustomSensorsCallback(addPowerSensor);
        }
    }
}

#endif

void ClockPlugin::setup(SetupModeType mode)
{
    _disable(10);
    IF_IOT_CLOCK_HAVE_ENABLE_PIN(
        pinMode(IOT_CLOCK_EN_PIN, OUTPUT);
    )

    IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(
        WsClient::addClientCallback(webSocketCallback, this);
    )

    readConfig();
    _targetBrightness = 0;

    IF_IOT_LED_MATRIX_STANDBY_PIN(
        _digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
        _pinMode(IOT_LED_MATRIX_STANDBY_PIN, OUTPUT);
    )

    IF_IOT_CLOCK(
        // _setSevenSegmentDisplay();
        IF_IOT_CLOCK_PIXEL_SYNC_ANIMATION(
            if (IS_TIME_VALID(time(nullptr))) {
                _isSyncing = false;
            } else {
                setSyncing(true);
            }
            addTimeUpdatedCallback(ntpCallback);
        )
    )

    IF_IOT_CLOCK_BUTTON_PIN(
        pinMonitor.begin();

        __LDBG_printf("button at pin %u", IOT_CLOCK_BUTTON_PIN);
        pinMonitor.attach<Clock::Button>(IOT_CLOCK_BUTTON_PIN, 0, *this);
        pinMode(IOT_CLOCK_BUTTON_PIN, INPUT);

        #if IOT_CLOCK_TOUCH_PIN != -1
            __LDBG_printf("touch sensor at pin %u", IOT_CLOCK_TOUCH_PIN);
            pinMonitor.attach<Clock::TouchButton>(IOT_CLOCK_TOUCH_PIN, 1, *this);
            pinMode(IOT_CLOCK_TOUCH_PIN, INPUT);
        #endif

        IF_IOT_CLOCK_HAVE_ROTARY_ENCODER(
            __LDBG_printf("rotary encoder at pin %u,%u active_low=%u", IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB, PIN_MONITOR_ACTIVE_STATE == ActiveStateType::ACTIVE_LOW);
            auto encoder = new Clock::RotaryEncoder(PIN_MONITOR_ACTIVE_STATE);
            encoder->attachPins(IOT_CLOCK_ROTARY_ENC_PINA, IOT_CLOCK_ROTARY_ENC_PINB);
            pinMode(IOT_CLOCK_ROTARY_ENC_PINA, INPUT);
            pinMode(IOT_CLOCK_ROTARY_ENC_PINB, INPUT);
        )
    )

    IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
        ADCManager::getInstance().addAutoReadTimer(Event::seconds(1), Event::milliseconds(30), 24);
        _Timer(_autoBrightnessTimer).add(Event::milliseconds_cast(kAutoBrightnessInterval), true, adjustAutobrightness, Event::PriorityType::TIMER);
        _adjustAutobrightness();
        _installWebHandlers();
    )

    MQTTClient::safeRegisterComponent(this);

    LoopFunctions::add(ClockPlugin::loop);
    IF_IOT_ALARM_PLUGIN_ENABLED(
        AlarmPlugin::setCallback(alarmCallback);
    )

    IF_IOT_CLOCK_SAVE_STATE(
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
        }
    )

    _setupTimer();
}

void ClockPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
#if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
    _removeDisplayLedTimer();
#endif
    _disable(10);
    if (String_equals(source, SPGM(mqtt))) {
        MQTTClient::safeReRegisterComponent(this);
    }
    IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
        else if (String_equals(source, SPGM(http))) {
            _installWebHandlers();
        }
    )
    else {
        readConfig();
        IF_IOT_CLOCK_SAVE_STATE(
            _saveState();
        )
        _schedulePublishState = true;
    }
    if (_targetBrightness) {
        _enable();
    }
}

void ClockPlugin::shutdown()
{
    __LDBG_println();

    IF_IOT_CLOCK_SAVE_STATE(
        if (_saveTimer) {
            _saveTimer.remove();
            _saveState();
        }
    )

    #if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
        if (_displayLedTimer) {
            _removeDisplayLedTimer();
            if (!_targetBrightness || !_config.enabled) {
                delay(250);
            }
        }
    #endif

    #if HAVE_PCF8574
        _PCF8574.PORT = 0xff;
    #endif

    IF_IOT_CLOCK_HAVE_ROTARY_ENCODER(
        _rotaryActionTimer.remove();
    )

    IF_IOT_CLOCK_BUTTON_PIN(
        pinMonitor.detach(this);
    )

    #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
        _autoBrightnessTimer.remove();
    #endif

    IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(
        WsClient::removeClientCallback(this);
    )

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
            delay(1);
        }
    }

    LoopFunctions::remove(loop);

    // turn all LEDs off
    _disable(10);
}

void ClockPlugin::getStatus(Print &output)
{
#if IOT_LED_MATRIX
    output.print(F("LED Matrix Plugin" HTML_S(br)));
#else
    output.print(F("Clock Plugin" HTML_S(br)));
#endif
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    output.print(F("Ambient light sensor"));
    if (_config.protection.max_temperature > 25 && _config.protection.max_temperature < 85) {
        output.print(F(", over-temperature protection enabled"));
    }
#else
    if (_config.protection.max_temperature > 25 && _config.protection.max_temperature < 85) {
        output.print(F("Over-temperature protection enabled"));
    }
#endif
#if IOT_LED_MATRIX
    output.printf_P(PSTR(HTML_S(br) "Total pixels %u"), Clock::DisplayType::kNumPixels);
#else
    output.printf_P(PSTR(HTML_S(br) "Total pixels %u, digits pixels %u"), Clock::DisplayType::kNumPixels, Clock::SevenSegment::kNumPixelsDigits);
#endif
#if IOT_CLOCK_BUTTON_PIN != -1
#    if IOT_CLOCK_HAVE_ROTARY_ENCODER
#        if IOT_CLOCK_TOUCH_PIN
    output.printf_P(PSTR(HTML_S(br) "Rotary encoder with capacitive touch sensor and button"));
#        else
    output.printf_P(PSTR(HTML_S(br) "Rotary encoder with button"));
#        endif
#    else
    output.printf_P(PSTR(HTML_S(br) "Single button"));
#    endif
#endif
    if (isTempProtectionActive()) {
        output.printf_P(PSTR(HTML_S(br) "The temperature exceeded %u"), _config.protection.max_temperature);
    }
}

void ClockPlugin::loop()
{
    plugin._loop();
}

void ClockPlugin::enableLoop(bool enable)
{
    __LDBG_printf("enable loop=%u", enable);
    _display.clear();
    _display.show();
    if (enable) {
        LoopFunctions::add(loop);
    }
    else {
        LoopFunctions::remove(loop);
    }
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
    IF_IOT_CLOCK(
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
    )
    _config.animation = static_cast<uint8_t>(animation);
    switch(animation) {
        case AnimationType::FADING:
            _setAnimation(new Clock::FadingAnimation(*this, _getColor(), Color().rnd(), _config.fading.speed, _config.fading.delay * 1000, _config.fading.factor.value));
            break;
        case AnimationType::RAINBOW:
            _setAnimation(new Clock::RainbowAnimation(*this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.color));
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
    _config.protection.max_temperature = std::max((uint8_t)kMinimumTemperatureThreshold, _config.protection.max_temperature);
    _display.setDither(_config.dithering);

    __LDBG_printf("config read");

    #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        __LDBG_printf("limit=%u/%u r/g/b/idle=%u/%u/%u/%u", _config.power_limit, _getPowerLevelLimit(_config.power_limit), _config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
        FastLED.setMaxPowerInMilliWatts(_getPowerLevelLimit(_config.power_limit) IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(, &calcPowerFunction));
        FastLED.setPowerConsumptionInMilliwattsPer256(_config.power.red, _config.power.green, _config.power.blue, _config.power.idle);
    #endif

    // reset temperature protection
    _tempBrightness = 1.0;
    _setColor(_config.solid_color.value);
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    _autoBrightness = _config.auto_brightness;
#endif
    _setBrightness(_config.getBrightness());
    setAnimation(static_cast<AnimationType>(_config.animation));
}

Clock::Config_t &ClockPlugin::getWriteableConfig()
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

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR

void ClockPlugin::handleWebServer(AsyncWebServerRequest *request)
{
    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
        HttpHeaders httpHeaders(false);
        auto response = __LDBG_new(AsyncFillBufferCallbackResponse, [](bool *async, bool fillBuffer, AsyncFillBufferCallbackResponse *response) {
            if (*async && !fillBuffer) {
                response->setContentType(FSPGM(mime_text_plain));
                if (!ADCManager::getInstance().requestAverage(10, 30000, [async, response](const ADCManager::ADCResult &result) {
                    char buffer[16];
                    snprintf_P(buffer, sizeof(buffer), PSTR("%u"), result.getValue());
                    response->getBuffer().write(buffer);
                    AsyncFillBufferCallbackResponse::finished(async, response);
                })) {
                    response->setCode(500);
                    AsyncFillBufferCallbackResponse::finished(async, response);
                }
            }
        });
        httpHeaders.addNoCache();
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void ClockPlugin::_installWebHandlers()
{
    __LDBG_printf("server=%p", WebServerPlugin::getWebServerObject());
    WebServerPlugin::addHandler(F("/ambient_light_sensor"), handleWebServer);
}

#endif

void ClockPlugin::_setBrightness(uint8_t brightness)
{
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        AlarmPlugin::resetAlarm();
        return;
    }
    __LDBG_printf("brightness=%u fading=%u", brightness, _isFading);
    if (brightness) {
        _enable();
    }
    else {
        _disable();
    }
    _updateBrightnessSettings();
    _targetBrightness = brightness;
    _fadeTimer.disable();
    _forceUpdate = true;
    _isFading = false;
    _schedulePublishState = true;
#if HTTP2SERIAL_SUPPORT && IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
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

void ClockPlugin::_enable()
{
    if (_isEnabled) {
        // __LDBG_printf("enable LED pin %u state %u (is_enabled=%u, config=%u) SKIPPED", IOT_CLOCK_EN_PIN, enablePinState(true), _isEnabled, _config.enabled);
        return;
    }
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        return;
    }

    IF_IOT_CLOCK_HAVE_ENABLE_PIN(
        digitalWrite(IOT_CLOCK_EN_PIN, enablePinState(true));
        __LDBG_printf("enable LED pin %u state %u (is_enabled=%u, config=%u)", IOT_CLOCK_EN_PIN, enablePinState(true), _isEnabled, _config.enabled);
    )
    IF_IOT_LED_MATRIX_STANDBY_PIN(
        if (_config.standby_led) {
            _digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(false));
        }
    )

    LoopFunctions::remove(standbyLoop);
    LoopFunctions::add(loop);

    _config.enabled = true;
    _isEnabled = true;
}

void ClockPlugin::_disable(uint8_t delayMillis)
{
    __LDBG_printf("disable LED pin %u state %u (is_enabled=%u, config=%u)", IOT_CLOCK_EN_PIN, enablePinState(false), _isEnabled, _config.enabled);

    // turn all leds off and set brightness to 0
    _display.clear();
    _display.show(0);

    _config.enabled = false;
    _isEnabled = false;

    IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(
        _powerLevelCurrentmW = 0;
        _powerLevelUpdateTimer = 0;
        _powerLevelAvg = 0;
    )

    IF_IOT_CLOCK_HAVE_ENABLE_PIN(
        digitalWrite(IOT_CLOCK_EN_PIN, enablePinState(false));
    )
    IF_IOT_LED_MATRIX_STANDBY_PIN(
        if (_config.standby_led) {
            _digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(true));
        }
    )

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
            IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(int16_t autoBrightness);
        } saved = {
            _config.getAnimation(),
            _targetBrightness,
            IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(_autoBrightness)
        };

        __LDBG_printf("storing parameters brightness=%u auto=%d color=%s animation=%u", saved.targetBrightness, IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(saved.autoBrightness) -1, getColor().toString().c_str(), saved.animation);
        _resetAlarmFunc = [this, saved](Event::CallbackTimerPtr timer) {
            IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
                _autoBrightness = saved.autoBrightness;
            )
            _targetBrightness = saved.targetBrightness;
            setAnimation(saved.animation);
            __LDBG_printf("restored parameters brightness=%u auto=%d color=%s animation=%u timer=%u", saved.targetBrightness, IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(saved.autoBrightness) -1, getColor().toString().c_str(), saved.animation, (bool)timer);
            timer->disarm();
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer) {
        __LDBG_printf("alarm brightness=%u color=%s", _targetBrightness, getColor().toString().c_str());
        IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
            _autoBrightness = kAutoBrightnessOff;
        )

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
    plugin._webSocketCallback(type, client, server, id);
}

float ClockPlugin::__getPowerLevel(float P, float min) const
{
    return std::max<float>(IOT_CLOCK_POWER_CORRECTION_OUTPUT);
}

uint32_t ClockPlugin::_getPowerLevelLimit(uint32_t P_mW) const
{
    float P = P_mW / 1000.0;
    return std::max<float>(0, IOT_CLOCK_POWER_CORRECTION_LIMIT);
}

#endif
