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

#include <stl_ext/fixed_circular_buffer.h>
__DBGTM(
    stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _anmationTime;
    stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _animationRenderTime;
    stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _displayTime;
    stdex::fixed_circular_buffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _timerDiff;
);

#define GET_COLOR_STRING()          getColor().toString().c_str()

static ClockPlugin plugin;

#if IOT_LED_MATRIX

#define PLUGIN_OPTIONS_NAME                             "led_matrix"
#define PLUGIN_OPTIONS_FRIENDLY_NAME                    "LED Matrix"
#define PLUGIN_OPTIONS_WEB_TEMPLATES                    ""
#define PLUGIN_OPTIONS_CONFIG_FORMS                     "led-matrix"

#else

#define PLUGIN_OPTIONS_NAME                             "clock"
#define PLUGIN_OPTIONS_FRIENDLY_NAME                    "Clock"
#define PLUGIN_OPTIONS_CONFIG_FORMS                     "clock"

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
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
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
    _updateRate(plugin._updateRate),
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
    MQTTComponent(ComponentTypeEnum_t::LIGHT),
    _schedulePublishState(false),
    _isFading(false),
    _forceUpdate(false),
    __color(0, 0, 0xff),
    _lastUpdateTime(0),
    _updateRate(1000),
    _tempOverride(0),
    _tempBrightness(1.0),
    _timerCounter(0),
    _savedBrightness(0),
    _targetBrightness(0),
    _animation(nullptr),
    _nextAnimation(nullptr)
{
#if !IOT_LED_MATRIX
    size_t ofs = 0;
    auto ptr = _pixelOrder.data();
    static const char pixel_order[] PROGMEM = IOT_CLOCK_PIXEL_ORDER;
    for(int i = 0; i < IOT_CLOCK_NUM_DIGITS; i++) {
        memcpy_P(ptr, pixel_order, IOT_CLOCK_PIXEL_ORDER_LEN);
        for (int j = 0; j < IOT_CLOCK_PIXEL_ORDER_LEN; j++) {
            ptr[j] += ofs;
        }
        ofs += SevenSegmentDisplay::getNumPixelsPerDigit();
        ptr += IOT_CLOCK_PIXEL_ORDER_LEN;
    }
#endif

    REGISTER_PLUGIN(this, "ClockPlugin");
}

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR

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
    return ((_autoBrightness == kAutoBrightnessOff) ? _getFadingBrightness() : _getFadingBrightness() * _autoBrightnessValue) * (temperatureProtection ? getTempProtectionFactor() : 1);
}

#else

uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
{
    if (!temperatureProtection) {
        return _getFadingBrightness();
    }
    return _getFadingBrightness() * getTempProtectionFactor();
}

#endif

float ClockPlugin::_getFadingBrightness() const
{
    return _fadeTimer.isActive() && _fadeTimer.getDelay() ? _targetBrightness - (((int16_t)_targetBrightness - (int16_t)_startBrightness) / (float)_fadeTimer.getDelay()  * _fadeTimer.getTimeLeft()) : _targetBrightness;
}

void ClockPlugin::_setupTimer()
{
    _Timer(_timer).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr timer) {

        _timerCounter++;

        #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
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
        #endif

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
                    tempSensor = max(tempSensor, sensor.readSensor());
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

                    #if IOT_CLOCK_PIXEL_SYNC_ANIMATION
                        _isSyncing = false;
                    #endif

                    #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
                        _autoBrightness = kAutoBrightnessOff;
                        _displaySensor = DisplaySensorType::OFF;
                    #endif
                    setBrightness(0);
                    _disable();
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

void ClockPlugin::setup(SetupModeType mode)
{
#if IOT_CLOCK_HAVE_ENABLE_PIN
    _disable();
    pinMode(IOT_CLOCK_EN_PIN, OUTPUT);
#endif

    readConfig();
    _targetBrightness = 0;

#if !IOT_LED_MATRIX
    _setSevenSegmentDisplay();
#if IOT_CLOCK_PIXEL_SYNC_ANIMATION
    if (IS_TIME_VALID(time(nullptr))) {
        _isSyncing = false;
    } else {
        setSyncing(true);
    }
    addTimeUpdatedCallback(ntpCallback);
#endif
#endif

#if IOT_CLOCK_BUTTON_PIN
    _button.onHoldRepeat(800, 100, onButtonHeld);
    _button.onRelease(onButtonReleased);
#endif

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    ADCManager::getInstance().addAutoReadTimer(Event::seconds(1), Event::milliseconds(30), 24);
    _Timer(_autoBrightnessTimer).add(Event::milliseconds_cast(kAutoBrightnessInterval), true, adjustAutobrightness, Event::PriorityType::TIMER);
    _adjustAutobrightness();
    _installWebHandlers();
#endif

    MQTTClient::safeRegisterComponent(this);

    LoopFunctions::add(ClockPlugin::loop);
#if IOT_ALARM_PLUGIN_ENABLED
    AlarmPlugin::setCallback(alarmCallback);
#endif

#if IOT_CLOCK_HAVE_ENABLE_PIN
    _enable();
#endif

#if IOT_CLOCK_SAVE_STATE
    auto state = _getState();
    if (state.hasValidData()) {
        switch(_config.getInitialState()) {
            case InitialStateType::OFF:
                // start with clock turned off and the power button sets the configured brightness and animation
                _savedBrightness = _config.getBrightness();
                _targetBrightness = 0;
                break;
            case InitialStateType::ON:
                // start with clock turned on using default settings
                _savedBrightness = _config.getBrightness();
                setBrightness(_savedBrightness);
                break;
            case InitialStateType::RESTORE:
                // restore last settings
                _config = state.getConfig();
                _savedBrightness = _config.getBrightness();
                setBrightness(_savedBrightness);
                break;
            case InitialStateType::MAX:
                break;
        }
    }
#endif
}

void ClockPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
#if IOT_CLOCK_HAVE_ENABLE_PIN
    _disable();
#endif
    if (String_equals(source, SPGM(mqtt))) {
        MQTTClient::safeReRegisterComponent(this);
    }
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    else if (String_equals(source, SPGM(http))) {
        _installWebHandlers();
    }
#endif
    else {
        readConfig();
#if !IOT_LED_MATRIX
        _setSevenSegmentDisplay();
#endif
        _schedulePublishState = true;
    }
#if IOT_CLOCK_HAVE_ENABLE_PIN
    _enable();
#endif
}

void ClockPlugin::shutdown()
{
    __LDBG_println();

#if IOT_CLOCK_SAVE_STATE
    if (_saveTimer) {
        _saveTimer.remove();
        _saveState(_targetBrightness ? _targetBrightness : _savedBrightness);
    }
#endif
#if IOT_ALARM_PLUGIN_ENABLED
    _resetAlarm();
    AlarmPlugin::setCallback(nullptr);
#endif
    _timer.remove();
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    _autoBrightnessTimer.remove();
#endif
    LoopFunctions::remove(loop);
    _display.clear();
    _display.show();

#if IOT_CLOCK_HAVE_ENABLE_PIN
    delay(50);
    _disable();
#endif
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
    output.printf_P(PSTR(HTML_S(br) "Total pixels %u"), SevenSegmentDisplay::getTotalPixels());
#else
    output.printf_P(PSTR(HTML_S(br) "Total pixels %u, digits pixels %u"), SevenSegmentDisplay::getTotalPixels(), SevenSegmentDisplay::getDigitsPixels());
#endif
    if (isTempProtectionActive()) {
        output.printf_P(PSTR(HTML_S(br) "The temperature exceeded %u"), _config.protection.max_temperature);
    }
}

void ClockPlugin::loop()
{
    plugin._loop();
}

#if IOT_CLOCK_USE_DITHERING

void ClockPlugin::_dithering()
{
    if (_config.dithering) {
        _display.setBrightness(_getBrightness());
        FastLED.show();
        delay(1);
    }
}

#endif

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

static const char pgm_digit_order[] PROGMEM = IOT_CLOCK_DIGIT_ORDER;
static const char pgm_segment_order[] PROGMEM = IOT_CLOCK_SEGMENT_ORDER;

void ClockPlugin::_setSevenSegmentDisplay()
{
    SevenSegmentDisplay::PixelAddressType addr = 0;
    auto ptr = pgm_digit_order;
    auto endPtr = ptr + sizeof(pgm_digit_order);

    while(ptr < endPtr) {
        int n = pgm_read_byte(ptr++);
        if (n >= 30) {
            n -= 30;
            __LDBG_printf("address=%u colon=%u", addr, n);
#if IOT_CLOCK_NUM_PX_PER_COLON == 1
            _display.setColons(n, addr + IOT_CLOCK_NUM_COLON_PIXELS, addr);
#else
            _display.setColons(n, addr, addr + IOT_CLOCK_NUM_COLON_PIXELS);

#endif
            addr += IOT_CLOCK_NUM_COLON_PIXELS * 2;
        }
        else {
            __LDBG_printf("address=%u digit=%u", addr, n);
            addr = _display.setSegments(n, addr, pgm_segment_order);
        }
    }
}

void ClockPlugin::setBlinkColon(uint16_t value)
{
    uint16_t updateRate = value;
    if (value < kMinBlinkColonSpeed) {
        updateRate = kDefaultUpdateRate;
        value = 0;
    }
    if (_animation) {
        _updateRate = std::min(updateRate, _updateRate);
    }
    else {
        _updateRate = updateRate;
    }
    _config.blink_colon_speed = value;
    _schedulePublishState = true;
    __LDBG_printf("blinkcolon=%u update_rate=%u", value, _updateRate);
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

void ClockPlugin::setBrightness(uint8_t brightness, int ms)
{
    if (ms < 0) {
        ms = _config.getFadingTimeMillis();
    }
    __LDBG_printf("brightness=%u fading=%u time=%d", brightness, _isFading, ms);
    if (ms == 0) {
        // use _setBrightness
        _setBrightness(brightness);
    }
    else {
        // _setBrightness is called once the fading is complete
#if IOT_CLOCK_HAVE_ENABLE_PIN
        // enable LEDs now if the brightness is not 0, but wait until fading is complete before disabling them
        if (brightness != 0) {
            _enable();
        }
#endif
        _startBrightness = _getFadingBrightness();
        _fadingBrightness = ~_getBrightness();
        _targetBrightness = brightness;
        int16_t diff = (int16_t)_startBrightness - (int16_t)_targetBrightness;
        if (diff < 0) {
            diff = -diff;
        }
        __LDBG_printf("from=%u to=%u time=%d partial=%d", _startBrightness, _targetBrightness, ms, ms * diff / Clock::kMaxBrightness);
        // calculate time relative to the level change
        _fadeTimer.set(ms * diff / Clock::kMaxBrightness);
        _isFading = true;
        // publish new state immediatelly
        _schedulePublishState = true;
        _config.setBrightness(_targetBrightness);
    }
}

void ClockPlugin::setColorAndRefresh(Color color)
{
    __LDBG_printf("color=%s", color.toString().c_str());
    _setColor(color);
    _forceUpdate = true;
    _schedulePublishState = true;
}

void ClockPlugin::_setAnimatonNone()
{
    __LDBG_printf("animation NONE animation=%p next=%p", _animation, _nextAnimation);
    _deleteAnimaton(false);
#if IOT_LED_MATRIX
    setUpdateRate(kDefaultUpdateRate);
#else
    setUpdateRate((_config.blink_colon_speed < kMinBlinkColonSpeed) ? kDefaultUpdateRate : _config.blink_colon_speed);
#endif
}

void ClockPlugin::setAnimation(AnimationType animation)
{
    __LDBG_printf("animation=%d", animation);
    _config.animation = static_cast<uint8_t>(animation);
    switch(animation) {
        case AnimationType::FADING:
            _setAnimation(__LDBG_new(Clock::FadingAnimation, *this, _getColor(), Color().rnd(), _config.fading.speed, _config.fading.delay, _config.fading.factor.value));
            break;
        case AnimationType::RAINBOW:
            _setAnimation(__LDBG_new(Clock::RainbowAnimation, *this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.color));
            break;
        case AnimationType::FLASHING:
            _setAnimation(__LDBG_new(Clock::FlashingAnimation, *this, _getColor(), _config.flashing_speed));
            break;
#if IOT_LED_MATRIX
        case  AnimationType::FIRE:
            _setAnimation(__LDBG_new(Clock::FireAnimation, *this, _config.fire));
            break;
        case  AnimationType::SKIP_ROWS:
            _setAnimation(__LDBG_new(Clock::SkipRowsAnimation, *this, _config.skip_rows.rows, _config.skip_rows.cols, _config.skip_rows.time));
            break;
#endif
        case AnimationType::NEXT:
            __LDBG_printf("animation NEXT animation=%p next=%p", _animation, _nextAnimation);
            _deleteAnimaton(true);
            if (_animation) {
                break;
            }
            // fallthrough
        default:
            _config.animation = static_cast<uint8_t>(AnimationType::SOLID);
            // fallthrough
        case AnimationType::SOLID:
            _setColor(_config.solid_color);
            _setAnimatonNone();
            break;
    }
    _schedulePublishState = true;
}

void ClockPlugin::_setColor(uint32_t color)
{
    __color = color;
    if (_config.getAnimation() == AnimationType::SOLID) {
        _config.solid_color = __color;
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
    _display.setDithering(_config.dithering);

#if 0
    FastLED.setMaxPowerInMilliWatts(_config.power_limit ? _config.power_limit * 1000 : 1000000);
#else
    if (FastLED.m_pPowerFunc) { // setting power to zero causes a crash with the default version
        ; // do nothing, just a test if it has been patched
    }
    FastLED.setMaxPowerInMilliWatts(_config.power_limit * 1000);
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

#if IOT_CLOCK_BUTTON_PIN

void ClockPlugin::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    __LDBG_printf("duration=%u repeat=%u", duration, repeatCount);
    if (repeatCount == 1) {
        plugin.readConfig();
        plugin._buttonCounter = 0;
    }
    if (repeatCount == 12) {    // start flashing after 2 seconds, hard reset occurs ~2.5s
        plugin._autoBrightness = kAutoBrightnessOff;
        plugin._display.setBrightness(SevenSegmentDisplay::kMaxBrightness);
        plugin._setAnimation(__LDBG_new(Clock::FlashingAnimation, *this, Color(255, 0, 0), 150));

        _Scheduler.add(2000, false, [](Event::CallbackTimerPtr timer) {   // call restart if no reset occured
            __LDBG_printf("restarting device\n"));
            config.restartDevice();
        });
    }
}

void ClockPlugin::onButtonReleased(Button& btn, uint16_t duration)
{
    __LDBG_printf("duration=%u", duration);
    if (duration < 800) {
        plugin._onButtonReleased(duration);
    }
}

void ClockPlugin::_onButtonReleased(uint16_t duration)
{
    __LDBG_printf("press=%u", _buttonCounter % 4);
    if (_resetAlarm()) {
        return;
    }
    switch(_buttonCounter % 4) {
        case 0:
            setAnimation(AnimationType::RAINBOW);
            break;
        case 1:
            setAnimation(AnimationType::FLASHING);
            break;
        case 2:
            setAnimation(AnimationType::FADING);
            break;
        case 3:
        default:
            setAnimation(AnimationType::NONE);
            break;
    }
    _buttonCounter++;
}

#endif

void ClockPlugin::_setAnimation(Clock::Animation *animation)
{
    __LDBG_printf("animation=%p", animation);
    _deleteAnimaton(false);
    _animation = animation;
    _animation->begin();
    _forceUpdate = true;
    _schedulePublishState = true;
}

void ClockPlugin::_setNextAnimation(Clock::Animation *nextAnimation)
{
    __LDBG_printf("next=%p", nextAnimation);
    if (_nextAnimation) {
        __LDBG_delete(_nextAnimation);
    }
    _nextAnimation = nextAnimation;
}

void ClockPlugin::_deleteAnimaton(bool startNext)
{
    __LDBG_printf("animation=%p next=%p start_next=%u", _animation, _nextAnimation, startNext);
    if (_animation) {
        __LDBG_delete(_animation);
        _animation = nullptr;
    }
    if (_nextAnimation) {
        if (startNext) {
            _animation = _nextAnimation;
            _nextAnimation = nullptr;
            __LDBG_printf("begin next animation=%p", _animation);
            _animation->begin();
        }
        else {
            __LDBG_delete(_nextAnimation);
            _nextAnimation = nullptr;
        }
    }
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
    if (_targetBrightness >= Clock::kMaxBrightness / 50) { // >=2%
        __LDBG_printf("saved=%u set=%u", _savedBrightness, _targetBrightness);
        _savedBrightness = _targetBrightness;
    }
    __LDBG_printf("brightness=%u fading=%u", brightness, _isFading);
    _targetBrightness = brightness;
    _config.setBrightness(brightness);
    _fadeTimer.disable();
    _forceUpdate = true;
    _isFading = false;
    _schedulePublishState = true;
#if IOT_CLOCK_HAVE_ENABLE_PIN
    if (_targetBrightness) {
        _enable();
    }
    else {
        _disable();
    }
#endif
}

#if IOT_CLOCK_HAVE_ENABLE_PIN

void ClockPlugin::_enable()
{
    if (_isEnabled) {
    __LDBG_printf("enable LED pin %u state %u (is_enabled=%u) SKIPPED", IOT_CLOCK_EN_PIN, enablePinState(true), _isEnabled);
        return;
    }
    if (isTempProtectionActive()) {
        __LDBG_printf("temperature protection active");
        return;
    }
    __LDBG_printf("enable LED pin %u state %u (is_enabled=%u)", IOT_CLOCK_EN_PIN, enablePinState(true), _isEnabled);
    digitalWrite(IOT_CLOCK_EN_PIN, enablePinState(true));
    _isEnabled = true;
}

void ClockPlugin::_disable()
{
    __LDBG_printf("disable LED pin %u state %u (is_enabled=%u)", IOT_CLOCK_EN_PIN, enablePinState(false), _isEnabled);
    _isEnabled = false;
    digitalWrite(IOT_CLOCK_EN_PIN, enablePinState(false));
    delay(100);
}

#endif


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
            int16_t autoBrightness;
        } saved = {
            _config.getAnimation(),
            _targetBrightness,
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
            _autoBrightness,
#else
            0
#endif
        };

        __LDBG_printf("storing parameters brightness=%u auto=%d color=%s animation=%u", saved.targetBrightness, saved.autoBrightness, GET_COLOR_STRING(), saved.animation);
        _resetAlarmFunc = [this, saved](Event::CallbackTimerPtr timer) {
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
            _autoBrightness = saved.autoBrightness;
#endif
            _targetBrightness = saved.targetBrightness;
            setAnimation(saved.animation);
            __LDBG_printf("restored parameters brightness=%u auto=%d color=%s animation=%u timer=%u", saved.targetBrightness, saved.autoBrightness, GET_COLOR_STRING(), saved.animation, (bool)timer);
            timer->disarm();
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer) {
        __LDBG_printf("alarm brightness=%u color=%s", _targetBrightness, GET_COLOR_STRING());
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
        _autoBrightness = kAutoBrightnessOff;
#endif
        _targetBrightness = SevenSegmentDisplay::kMaxBrightness;
        _setAnimation(__LDBG_new(Clock::FlashingAnimation, *this, _config.alarm.color.value, _config.alarm.speed));
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

