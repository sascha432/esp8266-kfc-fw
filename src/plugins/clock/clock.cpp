/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <MicrosTimer.h>
#include <LoopFunctions.h>
#include <EventTimer.h>
#include <KFCForms.h>
#include <WebUISocket.h>
#include "blink_led_timer.h"
#include "./plugins/mqtt/mqtt_client.h"
#include "./plugins/ntp/ntp_plugin.h"
#include "./plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static ClockPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    ClockPlugin,
    "clock",            // name
    "Clock",            // friendly name
    "",                 // web_templates
    "clock",            // config_forms
    "mqtt",             // reconfigure_dependencies
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
    _color(0, 0, 0xff), _updateTimer(0), _time(0), _updateRate(1000), _isSyncing(1),
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    _autoBrightness(1023), _autoBrightnessValue(0), _autoBrightnessLastValue(0),
#endif
    _timerCounter(0), _tempProtection(false), _animation(nullptr), _nextAnimation(nullptr)
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

    setBlinkColon(0);

    _ui_colon = 0;
    _ui_animation = 0;
    _ui_color = 2;

    REGISTER_PLUGIN(this, "ClockPlugin");
}

void ClockPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s has_value=%u value=%s has_state=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        _resetAlarm();
        auto val = (uint8_t)value.toInt();
        if (String_equals(id, PSTR("btn_colon"))) {
            switch(_ui_colon = val) {
                case 0:
                    setBlinkColon(0);
                    break;
                case 1:
                    setBlinkColon(1000);
                    break;
                case 2:
                    setBlinkColon(500);
                    break;
            }
        }
        else if (String_equals(id, PSTR("btn_animation"))) {
            switch(_ui_animation = val) {
                case 0:
                    setAnimation(AnimationEnum_t::NONE);
                    break;
                case 1:
                    setAnimation(AnimationEnum_t::RAINBOW);
                    break;
                case 2:
                    setAnimation(AnimationEnum_t::FLASHING);
                    _updateRate = 250;
                    break;
                case 3: {
                        setAnimation(AnimationEnum_t::FADE);
                    }
                    break;
            }
        }
        else if (String_equals(id, PSTR("btn_color"))) {
            switch(_ui_color = val) {
                case 0:
                    _color = Color(255, 0, 0);
                    break;
                case 1:
                    _color = Color(0, 255, 0);
                    break;
                case 2:
                    _color = Color(0, 0, 255);
                    break;
                case 3:
                    _color.rnd();
                    break;
            }
            // if (_ui_animation == 2) {
            //     _animationData.flashing.color = _color;
            // }
            _updateTimer = 0;
        }
        else if (String_equals(id, SPGM(brightness))) {
            _brightness = value.toInt();
            _setBrightness();
        }
        publishState();
    }
}

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

void ClockPlugin::_adjustAutobrightness()
{
    if (_autoBrightness != AUTO_BRIGHTNESS_OFF) {
        auto adc = analogRead(A0);
        float value = adc / (float)_autoBrightness;
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

void ClockPlugin::_updateLightSensorWebUI()
{
    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    auto &events = json.addArray(JJ(events), 1);
    auto &obj = events.addObject(3);
    obj.add(JJ(id), FSPGM(light_sensor, "light_sensor"));
    obj.add(JJ(value), JsonNumber(_autoBrightness == AUTO_BRIGHTNESS_OFF ? NAN : _autoBrightnessValue * 100, 0));
    obj.add(JJ(state), _autoBrightness == AUTO_BRIGHTNESS_OFF ? false : true);

    WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
}

uint16_t ClockPlugin::_getBrightness() const
{
    if (_autoBrightness == AUTO_BRIGHTNESS_OFF) {
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

void ClockPlugin::setup(SetupModeType mode)
{
    _debug_println();

    readConfig();
    _setSevenSegmentDisplay();

#if IOT_CLOCK_BUTTON_PIN
    _button.onHoldRepeat(800, 100, onButtonHeld);
    _button.onRelease(onButtonReleased);
#endif

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

    _autoBrightnessTimer.add(IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL, true, [this](EventScheduler::TimerPtr) {
        _adjustAutobrightness();
    });
    _adjustAutobrightness();

#endif

    _timer.add(1000, true, [this](EventScheduler::TimerPtr) {
        _timerCounter++;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
        // update auto brightness timer
        if (_timerCounter % 2 == 0) {
            uint8_t tmp = _autoBrightnessValue * 100;
            if (tmp != _autoBrightnessLastValue) {
                __LDBG_printf("auto brightness %f", _autoBrightnessValue);
                _autoBrightnessLastValue = tmp;
                _updateLightSensorWebUI();
            }
        }
#endif
        // temperature protection timer
        if (_timerCounter % 10 == 0) {
            SensorPlugin::for_each<Sensor_LM75A>(nullptr, [](MQTTSensor &sensor, Sensor_LM75A &) {
                return (sensor.getType() == MQTTSensor::SensorType::LM75A);
            }, [this](Sensor_LM75A &sensor) {
                auto temp = sensor.readSensor();
// #if DEBUG_IOT_CLOCK
//                 if (!isnan(temp)) {
//                     __LDBG_printf("temp timer %f", temp);
//                 }
// #endif
                if (temp > _config.protection.max_temperature) {
                    // over temp. protection, reduce brightness to 20% and flash red
                    if (_brightness > SevenSegmentDisplay::MAX_BRIGHTNESS  / 5) {
                        __LDBG_printf("over temperature protection %.1f > %u", temp, _config.protection.max_temperature);
                        _tempProtection = true;
                        _brightness = SevenSegmentDisplay::MAX_BRIGHTNESS / 5;
                        _display.setBrightness(_brightness);
                        _color = Color(255, 0, 0);
                        setAnimation(AnimationEnum_t::FLASHING);
                        _updateRate = 2500;
                        publishState();
                    }
                }
                else if (temp > _config.protection.temperature_50) {
                    // temp. too high, reduce to 50% brightness
                    if (_brightness > SevenSegmentDisplay::MAX_BRIGHTNESS / 2) {
                        __LDBG_printf("temperature %.1f > %uC, reducing brightness to 50%%", temp, _config.protection.temperature_50);
                        _tempProtection = true;
                        _brightness = SevenSegmentDisplay::MAX_BRIGHTNESS / 2;
                        _setBrightness();
                        publishState();
                    }
                }
                else if (temp > _config.protection.temperature_75) {
                    // temp. too high, reduce to 75% brightness
                    if (_brightness > SevenSegmentDisplay::MAX_BRIGHTNESS / 1.3333) {
                        __LDBG_printf("temperature %.1f > %uC, reducing brightness to 75%%", temp, _config.protection.temperature_75);
                        _tempProtection = false;
                        _brightness = SevenSegmentDisplay::MAX_BRIGHTNESS / 1.3333;
                        _setBrightness();
                        publishState();
                    }
                }
                else if (_tempProtection && temp < (_config.protection.temperature_75 - 10) && _autoBrightness == AUTO_BRIGHTNESS_OFF) {
                    __LDBG_printf("recover");
                    _tempProtection = false;
                    // reactivate auto brightness
                    _autoBrightness = _config.auto_brightness;
                    _adjustAutobrightness();
                    publishState();
                }
            });
        }
        // mqtt update timer
        if (_timerCounter % 60 == 0) {
            publishState();
        }
    });

    if (!IS_TIME_VALID(time(nullptr))) {
        setSyncing(true);
    } else {
        _isSyncing = 0;
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
    else {
        readConfig();
        _setSevenSegmentDisplay();
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
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    _autoBrightnessTimer.remove();
#endif
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
}

void ClockPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Clock"), false);

    row = &webUI.addRow();
    row->addSlider(FSPGM(brightness), FSPGM(brightness), 0, SevenSegmentDisplay::MAX_BRIGHTNESS, true);

    row = &webUI.addRow();
    static const uint16_t height = 280;
    row->addButtonGroup(F("btn_colon"), F("Colon"), F("Solid,Blink slowly,Blink fast"), height);
    row->addButtonGroup(F("btn_animation"), F("Animation"), F("Solid,Rainbow,Flashing,Fading"), height);
    row->addButtonGroup(F("btn_color"), F("Color"), F("Red,Green,Blue,Random"), height);
    auto &sensor = row->addSensor(FSPGM(light_sensor), F("Ambient light sensor"), F("&#37;<br><img src=\"http://simpleicon.com/wp-content/uploads/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:10px\">"));
    sensor.add(JJ(height), height);
}

MQTTComponent::MQTTAutoDiscoveryPtr ClockPlugin::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    MQTTAutoDiscoveryPtr discovery;
    switch(num) {
        case 0: {
            discovery = new MQTTAutoDiscovery();
            discovery->create(this, F("clock"), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(MQTTClient::formatTopic(FSPGM(_set)));
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            discovery->addBrightnessStateTopic(MQTTClient::formatTopic(FSPGM(_brightness_state)));
            discovery->addBrightnessCommandTopic(MQTTClient::formatTopic(FSPGM(_brightness_set)));
            discovery->addBrightnessScale(SevenSegmentDisplay::MAX_BRIGHTNESS);
            discovery->addRGBStateTopic(MQTTClient::formatTopic(FSPGM(_color_state)));
            discovery->addRGBCommandTopic(MQTTClient::formatTopic(FSPGM(_color_set)));
        }
        break;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
        case 1: {
            MQTTComponentHelper component(MQTTComponent::ComponentTypeEnum_t::SENSOR);
            discovery = component.createAutoDiscovery(FSPGM(light_sensor), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(light_sensor)));
            discovery->addUnitOfMeasurement(String('%'));
        }
        break;
#endif
        default:
            return nullptr;
    }
    discovery->finalize();
    return discovery;
}

void ClockPlugin::onConnect(MQTTClient *client)
{
    _debug_println();
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_set)));
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_color_set)));
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_brightness_set)));
    publishState(client);
}

void ClockPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    __LDBG_printf("client=%p topic=%s payload=%s", client, topic, payload);

    _resetAlarm();

    if (strstr_P(topic, SPGM(_brightness_set))) {
        _brightness = atoi(payload);
        _setBrightness();
    }
    else if (strstr_P(topic, SPGM(_color_set))) {
        char *r, *g, *b;
        const char *comma = ",";
        r = strtok(payload, comma);
        if (r) {
            g = strtok(nullptr, comma);
            if (g) {
                b = strtok(nullptr, comma);
                if (b) {
                    _setColor(Color(atoi(r), atoi(g), atoi(b)));
                }
            }
        }
    }
    else if (strstr_P(topic, SPGM(_set))) {
        auto value = atoi(payload);
        if (value) {
            if (_color == 0) {
                _color = _savedColor;
            }
            _setAnimation(new Clock::FadingAnimation(*this, Color(0U), _color, 5.0));
        }
        else {
            if (_color != 0) {
                _savedColor = _color;
            }
            _setAnimation(new Clock::FadingAnimation(*this, _color, Color(0U), 5.0));
            _color = 0;
        }
    }
}

void ClockPlugin::_setColor(Color color)
{
    _color = color;
    _updateTimer = 0;
}

void ClockPlugin::getValues(JsonArray &array)
{
    auto obj = &array.addObject(3);
    obj->add(JJ(id), F("btn_colon"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _ui_colon);

    obj = &array.addObject(3);
    obj->add(JJ(id), F("btn_animation"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _ui_animation);

    obj = &array.addObject(3);
    obj->add(JJ(id), F("btn_color"));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _ui_color);

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(brightness));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _brightness);

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(light_sensor));
    obj->add(JJ(value), JsonNumber(_autoBrightness == AUTO_BRIGHTNESS_OFF ? NAN : _autoBrightnessValue * 100, 0));
    obj->add(JJ(state), _autoBrightness == AUTO_BRIGHTNESS_OFF ? false : true);
#endif
}

void ClockPlugin::publishState(MQTTClient *client)
{
    if (!client) {
        client = MQTTClient::getClient();
    }
    __LDBG_printf("client=%p color=%06x brightness=%u auto_brightness=%d", client, (int)Color(_colors), _brightness, _autoBrightness);
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(FSPGM(_state)), true, String(_color ? 1 : 0));
        client->publish(MQTTClient::formatTopic(FSPGM(_brightness_state)), true, String(_brightness));
        client->publish(MQTTClient::formatTopic(FSPGM(_color_state)), true, PrintString(F("%u,%u,%u"), _colors[0], _colors[1], _colors[2]));
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
        client->publish(MQTTClient::formatTopic(FSPGM(light_sensor)), true, _autoBrightness == AUTO_BRIGHTNESS_OFF ? String(FSPGM(Off)) : String(_autoBrightnessValue * 100, 0));
#endif
    }

    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    getValues(json.addArray(JJ(events)));
    WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
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
    __LDBG_printf("sync=%u syncing=%u", sync, _isSyncing);
    if (sync != (bool)_isSyncing) {
        _isSyncing = sync ? 1 : 0;
        _time = 0;
        _display.clear();
    }
}

void ClockPlugin::setBlinkColon(uint16_t value)
{
    if (value < kMinBlinkColonSpeed) {
        value = 0;
    }
    __LDBG_printf("blinkcolon=%u", value);
    if (_animation) {
        if (value && _updateRate > value) {
            _updateRate = value;
        }
    }
    else {
        if (value) {
            _updateRate = value;
        } else {
            _updateRate = kDefaultUpdateRate;
        }
    }
    _config.blink_colon_speed = value;
}

void ClockPlugin::setAnimation(AnimationEnum_t animation)
{
    __LDBG_printf("animation=%d", animation);
    _config.animation = animation;
    _display.setCallback(nullptr);
    switch(animation) {
        case AnimationEnum_t::FADE:
            _setAnimation(new Clock::FadingAnimation(*this, _color, Color().rnd(), _config.fading.speed, _config.fading.delay));
            break;
        case AnimationEnum_t::RAINBOW:
            _setAnimation(new Clock::RainbowAnimation(*this, _config.rainbow.speed, _config.rainbow.multiplier, _config.rainbow.factor.value));
            break;
        case AnimationEnum_t::FLASHING:
            _setAnimation(new Clock::FlashingAnimation(*this, _color, _config.flashing_speed));
            break;
        case AnimationEnum_t::NONE:
        default:
            _config.animation = AnimationEnum_t::NONE;
            setUpdateRate((_config.blink_colon_speed < kMinBlinkColonSpeed) ? kDefaultUpdateRate : _config.blink_colon_speed);
            break;
    }
}

void ClockPlugin::readConfig()
{
    _config = config._H_GET(Config().clock);
    if (!std::isnormal(_config.rainbow.multiplier) || _config.rainbow.multiplier == 0) {
        _config.rainbow.multiplier = 1.23;
    }

    _color = Color(_config.solid_color.value);
    _brightness = _config.brightness << 8;
    _autoBrightness = _config.auto_brightness;
    setAnimation(static_cast<AnimationEnum_t>(_config.animation));
    _display.setBrightness(_brightness);
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
        plugin._autoBrightness = AUTO_BRIGHTNESS_OFF;
        plugin._display.setBrightness(SevenSegmentDisplay::MAX_BRIGHTNESS);
        plugin.setAnimation(AnimationEnum_t::FLASHING);
        plugin._animationData.flashing.color = Color(255, 0, 0);
        plugin._updateRate = 50;

        Scheduler.addTimer(1000, false, [](EventScheduler::TimerPtr) {   // call restart if no reset occured
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
    __LDBG_printf("press=%u", _buttonCounter);
    if (_resetAlarm()) {
        return;
    }
    switch(_buttonCounter) {
        case 0:
            setAnimation(AnimationEnum_t::FAST_BLINK_COLON);
            break;
        case 1:
            setAnimation(AnimationEnum_t::BLINK_COLON);
            break;
        case 2:
            setAnimation(AnimationEnum_t::SOLID_COLON);
            break;
        case 3:
            setAnimation(AnimationEnum_t::RAINBOW);
            break;
        case 4:
            setAnimation(AnimationEnum_t::FLASHING);
            break;
        case 5:
            setAnimation(AnimationEnum_t::FLASHING);
            _updateRate = 500;
            _buttonCounter = -1;
            break;
    }
    _buttonCounter++;
}

#endif

void ClockPlugin::_setAnimation(Clock::Animation *animation)
{
    __LDBG_printf("animation=%p", animation);
    _deleteAnimaton();
    _animation = animation;
    _animation->begin();
}

void ClockPlugin::_setNextAnimation(Clock::Animation *nextAnimation)
{
    __LDBG_printf("next=%p", nextAnimation);
    if (_nextAnimation) {
        delete _nextAnimation;
    }
    _nextAnimation = nextAnimation;
}

void ClockPlugin::_deleteAnimaton()
{
    __LDBG_printf("animation=%p next=%p", _animation, _nextAnimation);
    if (_animation) {
        delete _animation;
        _animation = nullptr;
    }
    if (_nextAnimation) {
        _animation = _nextAnimation;
        _nextAnimation = nullptr;
        __LDBG_printf("begin next animation=%p", _animation);
        _animation->begin();
    }
    if (!_animation) {
        setAnimation(AnimationEnum_t::NONE);
    }
}

void ClockPlugin::_loop()
{
#if IOT_CLOCK_BUTTON_PIN
    _button.update();
#endif
    if (_isSyncing) {
        if (get_time_diff(_updateTimer, millis()) >= 100) {
            _updateTimer = millis();

#if IOT_CLOCK_PIXEL_SYNC_ANIMATION
            #error crashing

            // show syncing animation until the time is valid
            if (_pixelOrder) {
                if (++_isSyncing > IOT_CLOCK_PIXEL_ORDER_LEN) {
                    _isSyncing = 1;
                }
                for(uint8_t i = 0; i < SevenSegmentDisplay::getNumDigits(); i++) {
                    _display.rotate(i, _isSyncing - 1, _color, _pixelOrder.data(), _pixelOrder.size());
                }
            }
            else {
                if (++_isSyncing > SevenSegmentPixel_DIGITS_NUM_PIXELS) {
                    _isSyncing = 1;
                }
                for(uint8_t i = 0; i < _display->numDigits(); i++) {
                    _display->rotate(i, _isSyncing - 1, _color, nullptr, 0);
                }
            }
#endif
            _display.show();
        }
        return;
    }

    auto now = time(nullptr);
    if (get_time_diff(_updateTimer, millis()) >= _updateRate) {
        _updateTimer = millis();

        if (_animation) {
            _animation->loop(now);
            if (_animation->finished()) {
                __LDBG_printf("animation loop has finished");
                _deleteAnimaton();
            }
        }
        now = 0; // update display
    }
    if (_time != now) {
        uint8_t isColon = _config.blink_colon_speed >= kMinBlinkColonSpeed && ((_updateTimer / _config.blink_colon_speed) % 2 == 0);

        uint32_t color = _color;
        auto tm = localtime(&now);
        uint8_t hour = tm->tm_hour;
        if (!_config.time_format_24h) {
            hour = ((hour + 23) % 12) + 1;
        }

        _display.setDigit(0, hour / 10, color);
        _display.setDigit(1, hour % 10, color);
        _display.setDigit(2, tm->tm_min / 10, color);
        _display.setDigit(3, tm->tm_min % 10, color);
#if IOT_CLOCK_NUM_DIGITS == 6
        _display.setDigit(4, tm->tm_sec / 10, color);
        _display.setDigit(5, tm->tm_sec % 10, color);
#endif

        if (isColon) {
            _display.clearColon(0);
#if IOT_CLOCK_NUM_COLONS == 2
            _display.clearColon(1);
#endif
        }
        else {
            _display.setColon(0, SevenSegmentDisplay::BOTH, color);
#if IOT_CLOCK_NUM_COLONS == 2
            _display.setColon(1, SevenSegmentDisplay::BOTH, color);
#endif
        }

        _display.show();
    }
}

static const char pgm_digit_order[] PROGMEM = IOT_CLOCK_DIGIT_ORDER;
static const char pgm_segment_order[] PROGMEM = IOT_CLOCK_SEGMENT_ORDER;

void ClockPlugin::_setSevenSegmentDisplay()
{
    SevenSegmentDisplay::pixel_address_t addr = 0;
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
    auto oldUpdateRate = _updateRate;
    _updateRate = 25;
    _display.setBrightness(_getBrightness(), 2.5, [this, oldUpdateRate](uint16_t) {
        _updateRate = oldUpdateRate;
    });
}

#if IOT_ALARM_PLUGIN_ENABLED

void ClockPlugin::alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
{
    plugin._alarmCallback(mode, maxDuration);
}

void ClockPlugin::_alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
{
    if (maxDuration == Alarm::STOP_ALARM) {
        _resetAlarm();
        return;
    }

    if (!_resetAlarmFunc) {
        auto animation = static_cast<AnimationEnum_t>(_config.animation);
        auto brightness = _brightness;
        auto autoBrightness = _autoBrightness;

        __LDBG_printf("storing parameters brightness=%u auto_brightness=%d color=%06x animation=%u", _brightness, _autoBrightness, (int)_color, animation);
        _resetAlarmFunc = [this, animation, brightness, autoBrightness](EventScheduler::TimerPtr timer) {
            _autoBrightness = autoBrightness;
            _brightness = brightness;
            _display.setBrightness(brightness);
            setAnimation(animation);
            __LDBG_printf("restored parameters brightness=%u auto_brightness=%d color=%06x animation=%u timer=%u", _brightness, _autoBrightness, (int)_color, animation, (bool)timer);
            if (timer) {
                timer->detach();
            }
            else {
                _alarmTimer.remove();
            }
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer.active()) {
        __LDBG_printf("alarm brightness=%u auto_brightness=%d color=%06x", _brightness, _autoBrightness, (int)_color);
        _autoBrightness = AUTO_BRIGHTNESS_OFF;
        _brightness = SevenSegmentDisplay::MAX_BRIGHTNESS;
        _display.setBrightness(_brightness);
        _setAnimation(new Clock::FlashingAnimation(*this, _config.alarm.color.value, _config.alarm.speed));
    }

    if (maxDuration == 0) {
        maxDuration = 300; // limit time
    }
    // reset time if alarms overlap
    __LDBG_printf("alarm duration %u", maxDuration);
    _alarmTimer.add(maxDuration * 1000UL, false, _resetAlarmFunc);
}

bool ClockPlugin::_resetAlarm()
{
    __LDBG_printf("alarm_func=%u alarm_state=%u", _resetAlarmFunc ? 1 : 0, AlarmPlugin::getAlarmState());
    if (_resetAlarmFunc) {
        _resetAlarmFunc(nullptr);
        AlarmPlugin::resetAlarm();
        return true;
    }
    return false;
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "CLOCK"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKPX, "PX", "<led>,<r>,<g>,<b>", "Set level of a single pixel");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKP, "P", "<00[:.]00[:.]00>", "Display strings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKC, "C", "<r>,<g>,<b>", "Set color");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CLOCKTS, "TS", "<num>,<segment>", "Set segment for digit <num>");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CLOCKA, "A", "<num>[,<arguments>,...]", "Set animation", "Display available animations");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CLOCKD, "D", "Dump pixel addresses");

void ClockPlugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKPX), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKTS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKA), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CLOCKD), name);
}

bool ClockPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKTS))) {
        if (args.size() == 2) {
            enable(false);
            uint8_t digit = args.toInt(0);
            uint8_t segment = args.toInt(1);
            args.printf_P(PSTR("digit=%u segment=%c color=#%06x"), digit, _display.getSegmentChar(segment), _color.get());
            _display.setSegment(digit, segment, _color);
            _display.show();
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKP))) {
        enable(false);
        if (args.size() < 1) {
            args.print(F("clear"));
            _display.clear();
            _display.show();
        }
        else {
            auto text = args.get(0);
            args.printf_P(PSTR("'%s'"), text);
            _display.print(text, _color);
            _display.show();
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKA))) {
        if (args.isQueryMode()) {
            args.printf_P(PSTR("%u - rainbow animation (+CLOCKA=%u,<speed>,<multiplier>,<single>)"), AnimationEnum_t::RAINBOW, AnimationEnum_t::RAINBOW);
            args.printf_P(PSTR("%u - flashing"), AnimationEnum_t::FLASHING);
            args.printf_P(PSTR("%u - fade to color (+CLOCKA=%u,<r>,<g>,<b>)"), AnimationEnum_t::FADE, AnimationEnum_t::FADE);
            args.printf_P(PSTR("%u - blink colon speed"), (int)AnimationEnum_t::MAX);
            args.print(F("1000 = disable clock"));
            args.print(F("1001 = enable clock"));
            args.print(F("1002 = all pixels on"));
            args.print(F("1003 = test pixel animation order"));
        }
        else if (args.size() >= 1) {
            int value = args.toInt(0);
            if (value == (int)AnimationEnum_t::MAX) {
                setBlinkColon(args.toIntMinMax(1, 50U, 0xffffU, 1000U));
            }
            else if (value == 1000) {
                enable(false);
            }
            else if (value == 1001) {
                enable(true);
            }
            else if (value == 1002) {
                enable(false);
                _display.setColor(0xffffff);
            }
            else if (value == 1003) {
                int interval = args.toInt(1, 500);
                enable(false);
                size_t num = 0;
                Scheduler.addTimer(interval, true, [num, this](EventScheduler::TimerPtr timer) mutable {
                    __LDBG_printf("pixel=%u", num);
                    if (num == _pixelOrder.size()) {
                        _display.clear();
                        _display.show();
                        timer->detach();
                    }
                    else {
                        if (num) {
                            _display.setPixelColor(_pixelOrder[num - 1], 0);
                        }
                        _display.setColor(_pixelOrder[num++], 0x22);
                    }
                });
            }
            else if (value >= 0 && value < (int)AnimationEnum_t::MAX) {
                switch(static_cast<AnimationEnum_t>(value)) {
                    case AnimationEnum_t::RAINBOW:
                        _setAnimation(new Clock::RainbowAnimation(*this,
                            args.toIntMinMax(1, ClockPlugin::kMinRainbowSpeed, (uint16_t)0xfffe, _config.rainbow.speed),
                            args.toFloatMinMax(2, 0.1f, 100.0f, _config.rainbow.multiplier),
                            Color((uint8_t)args.toInt(3, _config.rainbow.factor.rgb[0]), (uint8_t)args.toInt(4, _config.rainbow.factor.rgb[1]), (uint8_t)args.toInt(5, _config.rainbow.factor.rgb[2]))
                        ));
                        break;
                    default:
                        setAnimation(static_cast<AnimationEnum_t>(value));
                        break;
                }
                // if (value == AnimationEnum_t::FADE) {
                //     _animationData.fade.speed = args.toIntMinMax(1, 5U, 1000U, 10U);
                //     if (args.size() >= 5) {
                //         _animationData.fade.toColor = Color(args.toInt(2, rand()), args.toInt(3, rand()), args.toInt(4, rand()));
                //     }
                //     else {
                //         _animationData.fade.toColor = ~_color & 0xffffff;
                //     }
                // }
                // else if (value == AnimationEnum_t::RAINBOW) {
                //     if (args.size() >= 2) {
                //         _config.rainbow.multiplier = args.toFloatMinMax(1, 0.01f, 100.0f);
                //         if (args.size() >= 3) {
                //             _config.rainbow.speed = args.toIntMinMax(2, 5, 255);
                //         }
                //     }
                // }
                // else if (value == AnimationEnum_t::FLASHING) {
                // }
                // setAnimation(static_cast<AnimationEnum_t>(value));
            }
        }
        else {
            setBlinkColon(0);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKC))) {
        if (args.size() == 1) {
            _color = Color(args.toNumber(0, 0xff0000), true);
        }
        else if (args.requireArgs(3, 3)) {
            _color = Color(args.toNumber(0, 0), args.toNumber(1, 0), args.toNumber(2, 0x80));
        }
        args.printf_P(PSTR("color=#%06x"), _color.get());
        for(size_t i = 0; i  < SevenSegmentDisplay::getTotalPixels(); i++) {
            if (_display.getPixelColor(i)) {
                _display.setPixelColor(i, _color);
            }
        }
        _display.show();
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKPX))) {
        if (args.requireArgs(4, 4)) {
            enable(false);

            int num = args.toInt(0);
            Color color(args.toInt(1), args.toInt(2), args.toInt(3));
            if (num < 0) {
                args.printf_P(PSTR("pixel=0-%u, color=#%06x"), _display.getTotalPixels(), color.get());
                _display.setColor(color);
            }
            else {
                args.printf_P(PSTR("pixel=%u, color=#%06x"), num, color.get());
                _display.setColor(num, color);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CLOCKD))) {
        _display.dump(args.getStream());
        return true;
    }
    return false;
}

#endif
