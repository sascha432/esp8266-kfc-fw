/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "clock.h"
#include <MicrosTimer.h>
#include <ReadADC.h>
#include <KFCForms.h>
#include <WebUISocket.h>
#include <WebUIAlerts.h>
#include <async_web_response.h>
#include "blink_led_timer.h"
#include "./plugins/mqtt/mqtt_client.h"
#include "./plugins/ntp/ntp_plugin.h"
#include "./plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

__DBGTM(
    FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _anmationTime;
    FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _animationRenderTime;
    FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _displayTime;
    FixedCircularBuffer<uint16_t, IOT_CLOCK_DEBUG_ANIMATION_TIME> _timerDiff;
);

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
#define IF_LIGHT_SENSOR(...) __VA_ARGS__
#else
#define IF_LIGHT_SENSOR(...)
#endif

static ClockPlugin plugin;

#if HAVE_SMOOTH_BRIGHTNESS_ADJUSTMENT == 0

#include <OSTimer.h>

class BrightnessTimer : public OSTimer {
public:
    using BrightnessType = Clock::SevenSegmentDisplay::BrightnessType;
    using FadingFinishedCallback = Clock::SevenSegmentDisplay::FadingFinishedCallback;
    using FadingRefreshCallback = Clock::SevenSegmentDisplay::FadingRefreshCallback;

    static constexpr double kInterval = 1000 / 20.0; // 20ms/50Hz refresh rate

    BrightnessTimer() {}

    BrightnessTimer(BrightnessType targetBrightness, float fadeTime, FadingFinishedCallback finishedCallback = nullptr, FadingRefreshCallback refreshCallback = nullptr) :
        _finishedCallback(finishedCallback),
        _refreshCallback(refreshCallback),
        _targetBrightness(targetBrightness),
        _stepSize(std::max_unsigned( 1, (BrightnessType)(Clock::SevenSegmentDisplay::kMaxBrightness / (fadeTime * kInterval)) ))
    {
        startTimer(kInterval, true);
    }

    virtual void ICACHE_RAM_ATTR run() override;

private:
    FadingFinishedCallback _finishedCallback;
    FadingRefreshCallback _refreshCallback;
    BrightnessType _targetBrightness;
    uint16_t _stepSize;
};

void ICACHE_RAM_ATTR BrightnessTimer::run()
{
    int32_t tmp = plugin._display._params.brightness;
    if (tmp < _targetBrightness) {
        tmp += _stepSize;
        if (tmp > _targetBrightness) {
            tmp = _targetBrightness;
        }
    }
    else if (tmp > _targetBrightness) {
        tmp -= _stepSize;
        if (tmp < _targetBrightness) {
            tmp = _targetBrightness;
        }
    }
    else {
        detach();
        if (_finishedCallback) {
            _finishedCallback(tmp);
        }
        return;
    }
    if (_refreshCallback) {
        _refreshCallback(tmp);
    }
}

#endif

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    ClockPlugin,
    "clock",            // name
    "Clock",            // friendly name
    "",                 // web_templates
    "clock",            // config_forms
    // reconfigure_dependencies
    IF_LIGHT_SENSOR("http,") "mqtt",
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

ClockPlugin::ClockPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(ClockPlugin)),
    MQTTComponent(ComponentTypeEnum_t::LIGHT),
#if IOT_CLOCK_BUTTON_PIN
    _button(IOT_CLOCK_BUTTON_PIN, PRESSED_WHEN_HIGH),
    _buttonCounter(0),
#endif
    _savedBrightness(0),
    _color(0, 0, 0xff),
    _lastUpdateTime(0),
    _time(0),
    _updateRate(1000),
    _isSyncing(true),
    _displaySensorValue(0),
    _tempProtection(ProtectionType::OFF),
    _schedulePublishState(false),
    _forceUpdate(false),
    IF_LIGHT_SENSOR(
        _autoBrightness(1023),
        _autoBrightnessValue(0),
        _autoBrightnessLastValue(0),
    )
    _timerCounter(0),
    _animation(nullptr),
    _nextAnimation(nullptr)
{
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

    REGISTER_PLUGIN(this, "ClockPlugin");
}

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

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
            const float interval = 2000 / IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL;
            _autoBrightnessValue = ((_autoBrightnessValue * interval) + value) / (interval + 1.0); // integrate over 2 seconds to get a smooth transition and avoid flickering
        }
        _display.setBrightness(_brightness * _autoBrightnessValue);
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
    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    auto &events = json.addArray(JJ(events), 1);
    auto &obj = events.addObject(3);
    obj.add(JJ(id), FSPGM(light_sensor, "light_sensor"));
    obj.add(JJ(value), _getLightSensorWebUIValue());
    obj.add(JJ(state), true);

    WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
}

uint16_t ClockPlugin::_readLightSensor() const
{
    return ADCManager::getInstance().readValue();
}

uint16_t ClockPlugin::_getBrightness() const
{
    if (_autoBrightness == kAutoBrightnessOff) {
        return _brightness;
    }
    return _brightness * _autoBrightnessValue;
}

#else

uint16_t ClockPlugin::_getBrightness() const
{
    return _brightness;
}

#endif

bool ClockPlugin::_isTemperatureBelowThresholds(float temp) const
{
    float minThreshold = std::min(std::min(_config.protection.max_temperature, _config.protection.temperature_50), _config.protection.temperature_75) - kTemperatureThresholdDifference;
    return temp < minThreshold;
}

void ClockPlugin::_startTempProtectionAnimation()
{
    _autoBrightness = kAutoBrightnessOff;
    _brightness = Clock::kBrightnessTempProtection; // 25% brightness and 20% of the time enabled = 5% power
    _display.setBrightness(_brightness);
    _setAnimation(new Clock::FlashingAnimation(*this, Color(0xff0000), 250, 5));
}

void ClockPlugin::setup(SetupModeType mode)
{
    _debug_println();

    readConfig();
    _setSevenSegmentDisplay();

#if IOT_CLOCK_BUTTON_PIN
    _button.onHoldRepeat(800, 100, onButtonHeld);
    _button.onRelease(onButtonReleased);
#endif

    IF_LIGHT_SENSOR(

        _Timer(_autoBrightnessTimer).add(IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL, true, [this](Event::TimerPtr &timer) {
            _adjustAutobrightness();
        });
        _adjustAutobrightness();

        _installWebHandlers();

    );

    _Timer(_timer).add(1000, true, [this](Event::TimerPtr &timer) {
        _timerCounter++;
        IF_LIGHT_SENSOR(
            // update light sensor webui
            if (_timerCounter % kUpdateAutobrightnessInterval == 0) {
                uint8_t tmp = _autoBrightnessValue * 100;
                if (tmp != _autoBrightnessLastValue) {
                    __LDBG_printf("auto brightness %f", _autoBrightnessValue);
                    _autoBrightnessLastValue = tmp;
                    // check if we will publiush the state in this loop
                    if (!_schedulePublishState) {
                        _updateLightSensorWebUI();
                    }
                }
            }
        );

        // temperature protection
        if (_timerCounter % kCheckTemperatureInterval == 0) {
            SensorPlugin::for_each<Sensor_LM75A>(nullptr, [](MQTTSensor &sensor, Sensor_LM75A &) {
                return (sensor.getType() == MQTTSensor::SensorType::LM75A);
            }, [this](Sensor_LM75A &sensor) {
                auto temp = sensor.readSensor();
                auto message1_P = PSTR("Over temperature protection activated: %.1f°C > %u°C");
                auto message2_P = PSTR("Temperature exceeded threshold, reducing brightness to %u%%: %.1f°C > %u°C");
                auto message2b_P = PSTR("Temperature exceeded threshold, brightness already below %u%%: %.1f°C > %u°C");
                auto message3_P = PSTR("Temperature %u°C below thresholds, restoring brightness");
                PrintString message;

                __LDBG_printf("temperature %.1f prot=%u max=%u 75=%u 50=%u min_th=%.1f", temp, _tempProtection, (temp > _config.protection.max_temperature), (temp > _config.protection.temperature_75), (temp > _config.protection.temperature_50), (float)(std::min(std::min(_config.protection.max_temperature, _config.protection.temperature_50), _config.protection.temperature_75) - kTemperatureThresholdDifference));


//todo fix temp protection
                if (temp > _config.protection.max_temperature) {
                    // over temp. protection, reduce brightness to 20% and flash red
                    if (_tempProtection < ProtectionType::MAX) { // send warning once
                        message.printf_P(message1_P, temp, _config.protection.max_temperature);
                        WebUIAlerts_error(message);
                        _isSyncing = false;
                        _displaySensorValue = 0;
                        _startTempProtectionAnimation();
                        _tempProtection = ProtectionType::MAX;
                    }
                    if (_brightness > Clock::kBrightnessTempProtection) {
                        _brightness = Clock::kBrightnessTempProtection;
                    }
                }
                else if (temp > _config.protection.temperature_50 && _tempProtection < ProtectionType::B50) {
                    // temp. too high, reduce to 50% brightness
                    PGM_P msg;
                    _tempProtection = ProtectionType::B50;
                    _autoBrightness = kAutoBrightnessOff;
                    if (_brightness > Clock::kBrightness50) {
                        _setBrightness(Clock::kBrightness50);
                        msg = message2_P;
                    }
                    else {
                        msg = message2b_P;
                    }
                    message.printf_P(msg, 50, temp, _config.protection.temperature_50);
                    WebUIAlerts_warning(message);
                }
                else if (temp > _config.protection.temperature_75 && _tempProtection < ProtectionType::B75) {
                    // temp. too high, reduce to 75% brightness
                    PGM_P msg;
                    _tempProtection = ProtectionType::B75;
                    _autoBrightness = kAutoBrightnessOff;
                    if (_brightness > Clock::kBrightness75) {
                        _setBrightness(Clock::kBrightness75);
                        msg = message2_P;
                    }
                    else {
                        msg = message2b_P;
                    }
                    message.printf_P(msg, 75, temp, _config.protection.temperature_75);
                    WebUIAlerts_warning(message);
                }
                // restore if temperature falls 10°C below temperature_75 (or any other if lower)
                // no recovery if the max temperature had been exceeded
                else if ((_tempProtection != ProtectionType::OFF) && (_tempProtection != ProtectionType::MAX) && _isTemperatureBelowThresholds(temp)) {
                    message.printf_P(message3_P, kTemperatureThresholdDifference);
                    Logger_notice(message);
                    _tempProtection = ProtectionType::OFF;
                    // restore settings
                    _autoBrightness = _config.auto_brightness;
                    _setBrightness(_config.brightness);
                }
                if (message.length()) {
                    __LDBG_printf("%s", message.c_str());
                    _schedulePublishState = true;
                }
            });
        }
        // mqtt update / webui update
        if (_schedulePublishState || (_timerCounter % kUpdateMQTTInterval == 0)) {
            _schedulePublishState = false;
            _publishState();
        }
    });

    if (IS_TIME_VALID(time(nullptr))) {
        _isSyncing = false;
    } else {
        setSyncing(true);
    }
    addTimeUpdatedCallback(ntpCallback);

    MQTTClient::safeRegisterComponent(this);

    LoopFunctions::add(ClockPlugin::loop);
#if IOT_ALARM_PLUGIN_ENABLED
    AlarmPlugin::setCallback(alarmCallback);
#endif
}

void ClockPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    if (String_equals(source, SPGM(mqtt))) {
        MQTTClient::safeReRegisterComponent(this);
    }
    IF_LIGHT_SENSOR(else if (String_equals(source, SPGM(http))) {
        _installWebHandlers();
    })
    else {
        readConfig();
        _setSevenSegmentDisplay();
        _schedulePublishState = true;
    }
}

void ClockPlugin::shutdown()
{
    _debug_println();
#if IOT_ALARM_PLUGIN_ENABLED
    _resetAlarm();
    AlarmPlugin::setCallback(nullptr);
#endif
    _timer.remove();
#if HAVE_SMOOTH_BRIGHTNESS_ADJUSTMENT == 0
    _displayBrightnessTimer.detach();
#endif
    IF_LIGHT_SENSOR(
        _autoBrightnessTimer.remove();
    );
    LoopFunctions::remove(loop);
    _display.clear();
    _display.show();
}

void ClockPlugin::getStatus(Print &output)
{
    output.print(F("Clock Plugin" HTML_S(br)));
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    output.print(F("Ambient light sensor"));
    if (_config.protection.max_temperature < 85) {
        output.print(F(", over-temperature protection enabled"));
    }
    output.print(F(HTML_S(br)));
#else
    if (_config.protection.max_temperature < 85) {
        output.print(F("Over-temperature protection enabled" HTML_S(br)));
    }
#endif
    output.printf_P(PSTR("Total pixels %u, digits pixels %u"), SevenSegmentDisplay::getTotalPixels(), SevenSegmentDisplay::getDigitsPixels());
    if (_tempProtection == ProtectionType::MAX) {
        output.printf_P(PSTR("The temperature exceeded %u"));
    }
}

void ClockPlugin::loop()
{
    plugin._loop();
}

void ClockPlugin::ntpCallback(time_t now)
{
    plugin.setSyncing(false);
}

void ClockPlugin::enable(bool enable)
{
    __LDBG_printf("enable=%u", enable);
    _display.clear();
    _display.show();
    if (enable) {
        LoopFunctions::add(loop);
    }
    else {
        LoopFunctions::remove(loop);
    }
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

void ClockPlugin::setBrightness(uint16_t brightness)
{
    _setBrightness(brightness);
    _schedulePublishState = true;
}

void ClockPlugin::setColorAndRefresh(Color color)
{
    __LDBG_printf("color=%s", color.toString().c_str());
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        return;
    }
    _color = color;
    _forceUpdate = true;
    _schedulePublishState = true;
}

void ClockPlugin::setAnimation(AnimationType animation)
{
    __LDBG_printf("animation=%d", animation);
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        return;
    }
    _config.animation = static_cast<uint8_t>(animation);
    switch(animation) {
        case AnimationType::FADING:
            _setAnimation(new Clock::FadingAnimation(*this, _color, Color().rnd(), _config.fading.speed, _config.fading.delay, _config.fading.factor.value));
            break;
        case AnimationType::RAINBOW:
            _setAnimation(new Clock::RainbowAnimation(*this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.factor.value, _config.rainbow.minimum.value));
            break;
        case AnimationType::FLASHING:
            _setAnimation(new Clock::FlashingAnimation(*this, _color, _config.flashing_speed));
            break;
        case AnimationType::NEXT:
            __LDBG_printf("animation NEXT animation=%p next=%p", _animation, _nextAnimation);
            _deleteAnimaton(true);
            if (_animation) {
                break;
            }
            // fallthrough
        default:
            _config.animation = static_cast<uint8_t>(AnimationType::NONE);
            // fallthrough
        case AnimationType::NONE:
            __LDBG_printf("animation NONE animation=%p next=%p", _animation, _nextAnimation);
            _deleteAnimaton(false);
            setUpdateRate((_config.blink_colon_speed < kMinBlinkColonSpeed) ? kDefaultUpdateRate : _config.blink_colon_speed);
            break;
    }
    _schedulePublishState = true;
}

void ClockPlugin::setAnimationCallback(Clock::AnimationCallback callback)
{
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        return;
    }
    _display.setCallback(callback);
}

void ClockPlugin::setUpdateRate(uint16_t updateRate)
{
    __LDBG_printf("before=%u rate=%u", _updateRate, updateRate);
    _updateRate = updateRate;
    _forceUpdate = true;
}

uint16_t ClockPlugin::getUpdateRate() const
{
    return _updateRate;
}

void ClockPlugin::setColor(Color color)
{
    _color = color;
}

ClockPlugin::Color ClockPlugin::getColor() const
{
    return _color;
}

void ClockPlugin::readConfig()
{
    _config = Plugins::Clock::getConfig();
    if (!std::isnormal(_config.rainbow.multiplier) || _config.rainbow.multiplier == 0) {
        _config.rainbow.multiplier = 1.23;
    }

    _config.protection.max_temperature = std::max((uint8_t)kMinimumTemperatureThreshold, _config.protection.max_temperature);
    _config.protection.temperature_50 = std::max((uint8_t)kMinimumTemperatureThreshold, _config.protection.temperature_50);
    _config.protection.temperature_75 = std::max((uint8_t)kMinimumTemperatureThreshold, _config.protection.temperature_75);

    _tempProtection = ProtectionType::OFF;
    _color = Color(_config.solid_color.value);
    _autoBrightness = _config.auto_brightness;
    _setBrightness(_config.brightness << 8);
    setAnimation(static_cast<AnimationType>(_config.animation));
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
        plugin._setAnimation(new Clock::FlashingAnimation(*this, Color(255, 0, 0), 150));

        _Scheduler.add(2000, false, [](Event::TimerPtr &timer) {   // call restart if no reset occured
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
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        return;
    }
    _deleteAnimaton(false);
    _animation = animation;
    _animation->begin();
    _schedulePublishState = true;
}

void ClockPlugin::_setNextAnimation(Clock::Animation *nextAnimation)
{
    __LDBG_printf("next=%p", nextAnimation);
    if (_nextAnimation) {
        delete _nextAnimation;
    }
    _nextAnimation = nextAnimation;
}

void ClockPlugin::_deleteAnimaton(bool startNext)
{
    __LDBG_printf("animation=%p next=%p start_next=%u", _animation, _nextAnimation, startNext);
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        return;
    }
    if (_animation) {
        delete _animation;
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
            delete _nextAnimation;
            _nextAnimation = nullptr;
        }
    }
}

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

void ClockPlugin::handleWebServer(AsyncWebServerRequest *request)
{
    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
        HttpHeaders httpHeaders(false);
        auto response = new AsyncFillBufferCallbackResponse([](bool *async, bool fillBuffer, AsyncFillBufferCallbackResponse *response) {
            if (*async && !fillBuffer) {
                response->setContentType(FSPGM(mime_text_plain));
                if (!ADCManager::getInstance().requestAverage(10, 25, [async, response](const ADCManager::ADCResult &result) {
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
    } else {
        request->send(403);
    }
}

void ClockPlugin::_installWebHandlers()
{
    __LDBG_printf("server=%p", WebServerPlugin::getWebServerObject());
    WebServerPlugin::addHandler(F("/ambient_light_sensor"), handleWebServer);
}

#endif

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

void ClockPlugin::_setBrightness()
{
#if HAVE_SMOOTH_BRIGHTNESS_ADJUSTMENT
    _display.setBrightness(_getBrightness(), 2.5, [this](uint16_t) {
        _schedulePublishState = true;
    }, [this](uint16_t) {
        _forceUpdate = true;
    });
    _schedulePublishState = true;
#else
    // smooth brightness change
    // NOTE: callbacks are called inside ISR, do not call any code without IRAM attribute or schedule the function
    if (_displayBrightnessTimer.isRunning()) {
        _displayBrightnessTimer.setTargetBrightness(_getBrightness());
    }
    else {
        _displayBrightnessTimer = BrightnessTimer(_getBrightness(), 2.5 fadeTime, [this](uint16_t brightness) {
            _schedulePublishState = true;
            _display.setBrightness(brightness);
        }, [this](uint16_t brightness) {
            _forceUpdate = true;
            _display.setBrightness(brightness);
        });
        _displayBrightnessTimer.start();

    }
#endif
}

void ClockPlugin::_setBrightness(uint16_t brightness)
{
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        AlarmPlugin::resetAlarm();
        return;
    }
    if (_brightness > Clock::kMaxBrightness / 50) { // >2%
        _savedBrightness = _brightness;
    }
    _brightness = brightness;
    _setBrightness();
}

#if IOT_ALARM_PLUGIN_ENABLED

void ClockPlugin::alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
{
    plugin._alarmCallback(mode, maxDuration);
}

void ClockPlugin::_alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
{
    if (_tempProtection == ProtectionType::MAX) {
        __LDBG_printf("temperature protection active");
        // LoopFunctions::callOnce([]() {
        //     AlarmPlugin::resetAlarm();
        // });
        AlarmPlugin::resetAlarm();
        return;
    }
    if (maxDuration == Alarm::STOP_ALARM) {
        _resetAlarm();
        return;
    }

    // alarm reset function already set?
    if (!_resetAlarmFunc) {
        auto animation = static_cast<AnimationType>(_config.animation);
        auto brightness = _brightness;
        auto autoBrightness = _autoBrightness;

        __LDBG_printf("storing parameters brightness=%u auto_brightness=%d color=#%06x animation=%u", _brightness, _autoBrightness, _color.get(), animation);
        _resetAlarmFunc = [this, animation, brightness, autoBrightness](Event::TimerPtr &timer) {
            _autoBrightness = autoBrightness;
            _brightness = brightness;
            _display.setBrightness(brightness);
            setAnimation(animation);
            __LDBG_printf("restored parameters brightness=%u auto_brightness=%d color=#%06x animation=%u timer=%u", _brightness, _autoBrightness, _color.get(), animation, (bool)timer);
            __Scheduler.remove(timer);
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer.isActive()) {
        __LDBG_printf("alarm brightness=%u auto_brightness=%d color=#%06x", _brightness, _autoBrightness, _color.get());
        _autoBrightness = kAutoBrightnessOff;
        _brightness = SevenSegmentDisplay::kMaxBrightness;
        _display.setBrightness(_brightness);
        _setAnimation(new Clock::FlashingAnimation(*this, _config.alarm.color.value, _config.alarm.speed));
    }

    if (maxDuration == 0) {
        maxDuration = 300; // limit time
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
        _resetAlarmFunc(_timer);
        AlarmPlugin::resetAlarm();
        _schedulePublishState = true;
        return true;
    }
    return false;
}

#endif

