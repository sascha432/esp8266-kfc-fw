/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <stl_ext/algorithm.h>
#include "../src/plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#if IOT_LED_MATRIX_HEXAGON_PANEL
#    define MQTT_NAME "rgb_hexagon_panel"
#elif IOT_LED_STRIP
#    define MQTT_NAME "rgb_led_strip"
#elif IOT_LED_MATRIX
#    define MQTT_NAME "rgb_matrix"
#elif IOT_CLOCK
#    define MQTT_NAME "clock"
#else
#    error Unknown
#endif

enum class AutoDiscoverySwitchEnum {
    BRIGHTNESS = 0,
    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        POWER_CONSUMPTION,
    #endif
    #if IOT_LED_MATRIX_FAN_CONTROL
        FAN_CONTROL,
    #endif
    MAX
};

MQTT::AutoDiscovery::EntityPtr ClockPlugin::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new AutoDiscovery::Entity();
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(static_cast<AutoDiscoverySwitchEnum>(num)) {
        case AutoDiscoverySwitchEnum::BRIGHTNESS: {
            if (!discovery->create(this, F(MQTT_NAME), format)) {
                return discovery;
            }
            discovery->addStateTopicAndPayloadOnOff(MQTT::Client::formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(MQTT::Client::formatTopic(FSPGM(_set)));
            discovery->addBrightnessStateTopic(MQTT::Client::formatTopic(FSPGM(_brightness_state)));
            discovery->addBrightnessCommandTopic(MQTT::Client::formatTopic(FSPGM(_brightness_set)));
            discovery->addBrightnessScale(kMaxBrightness);
            discovery->addRGBStateTopic(MQTT::Client::formatTopic(FSPGM(_color_state)));
            discovery->addRGBCommandTopic(MQTT::Client::formatTopic(FSPGM(_color_set)));
            discovery->addEffectStateTopic(MQTT::Client::formatTopic(FSPGM(_effect_state)));
            discovery->addEffectCommandTopic(MQTT::Client::formatTopic(FSPGM(_effect_set)));
            discovery->addEffectList(KFCConfigurationClasses::Plugins::ClockConfigNS::ClockConfigType::getAnimationNamesJsonArray());
            #if IOT_LED_MATRIX_HEXAGON_PANEL
                discovery->addName(F("Hexagon Panel"));
                discovery->addObjectId(baseTopic + F("hexagon_panel"));
                discovery->addIcon(F("mdi:hexagon-multiple-outline"));
            #elif IOT_LED_STRIP
                discovery->addName(F("LED Strip"));
                discovery->addObjectId(baseTopic + F("led_strip"));
            #elif IOT_LED_MATRIX
                discovery->addName(F("LED Matrix"));
                discovery->addObjectId(baseTopic + F("led_matrix"));
            #elif IOT_CLOCK
                discovery->addName(F("Clock"));
                discovery->addObjectId(baseTopic + F("clock"));
            #else
                #error Unknown type
            #endif
        }
        break;
#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        case AutoDiscoverySwitchEnum::POWER_CONSUMPTION: {
            if (!discovery->create(MQTTComponent::ComponentType::SENSOR, F("power"), format)) {
                return discovery;
            }
            discovery->addStateTopic(MQTT::Client::formatTopic(F("power")));
            discovery->addUnitOfMeasurement(String('W'));
            discovery->addDeviceClass(F("power"));
            discovery->addName(F("Estimated Power"));
            discovery->addObjectId(baseTopic + F("Estimated Power"));
        }
        break;
#endif
#if IOT_LED_MATRIX_FAN_CONTROL
        case AutoDiscoverySwitchEnum::FAN_CONTROL: {
            if (!discovery->create(MQTTComponent::ComponentType::FAN, F("fan"), format)) {
                return discovery;
            }
            discovery->addStateTopicAndPayloadOnOff(MQTT::Client::formatTopic(F("/fan/on/state")));
            discovery->addCommandTopic(MQTT::Client::formatTopic(F("/fan/on/set")));
            discovery->addPercentageStateTopic(MQTT::Client::formatTopic(F("/fan/speed/state")));
            discovery->addPercentageCommandTopic(MQTT::Client::formatTopic(F("/fan/speed/set")));
            discovery->addSpeedRangeMin(_config.min_fan_speed);
            discovery->addSpeedRangeMax(_config.max_fan_speed);
            discovery->addName(F("Fan"));
            discovery->addObjectId(baseTopic + F("Fan"));
        }
        break;
#endif
        case AutoDiscoverySwitchEnum::MAX:
            break;
    }
    return discovery;
}

uint8_t ClockPlugin::getAutoDiscoveryCount() const
{
    return static_cast<uint8_t>(AutoDiscoverySwitchEnum::MAX);
}

void ClockPlugin::onConnect()
{
    _publishedValues = Clock::PublishedStateType();
    subscribe(MQTT::Client::formatTopic(FSPGM(_set)));
    subscribe(MQTT::Client::formatTopic(FSPGM(_color_set)));
    subscribe(MQTT::Client::formatTopic(FSPGM(_brightness_set)));
    subscribe(MQTT::Client::formatTopic(FSPGM(_effect_set)));
    #if IOT_LED_MATRIX_FAN_CONTROL
        subscribe(MQTT::Client::formatTopic(F("/fan/on/set")));
        subscribe(MQTT::Client::formatTopic(F("/fan/speed/set")));
    #endif
    _publishState();
}

void ClockPlugin::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("topic=%s payload=%s", topic, payload);

    #if IOT_ALARM_PLUGIN_ENABLED
        _resetAlarm();
    #endif

    #if IOT_LED_MATRIX_FAN_CONTROL
        if (!strcmp_end_P(topic, PSTR("/fan/on/set"))) {
            __DBG_printf("/fan/on/set %s", payload);
        }
        else if (!strcmp_end_P(topic, PSTR("/fan/speed/set"))) {
            __DBG_printf("/fan/speed/set %s", payload);
        } else
    #endif
    if (!strcmp_end_P(topic, SPGM(_effect_set))) {
        auto animation = _getAnimationType(FPSTR(payload));
        if (animation < AnimationType::LAST) {
            setAnimation(static_cast<AnimationType>(animation));
            _saveState();
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_brightness_set))) {
        if (len) {
            auto value = strtoul(payload, nullptr, 0);
            setBrightness(std::clamp<uint8_t>(value, 0, kMaxBrightness));
            _saveState();
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_color_set))) {
        if (*payload == '#') {
            // rgb color code #FFEECC
            setColorAndRefresh(Color::fromString(payload));
            _saveState();
        }
        else {
            // red,green,blue
            char *endptr = nullptr;
            auto red = static_cast<uint8_t>(strtoul(payload, &endptr, 10));
            if (endptr && *endptr++ == ',') {
                auto green = static_cast<uint8_t>(strtoul(endptr, &endptr, 10));
                if (endptr && *endptr++ == ',') {
                    auto blue = static_cast<uint8_t>(strtoul(endptr, nullptr, 10));
                    setColorAndRefresh(Color(red, green, blue));
                    _saveState();
                }
            }
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_set))) {
        auto res = MQTT::Client::toBool(payload);
        if (res >= 0) {
            _setState(res);
        }
    }
}

void ClockPlugin::_publishState()
{
    if (isConnected()) {
        if (_publishedValues.enabled != _getEnabledState()) {
            _publishedValues.enabled = _getEnabledState();
            publish(MQTT::Client::formatTopic(FSPGM(_state)), true, MQTT::Client::toBoolOnOff(_publishedValues.enabled));
        }
        int brightness = _targetBrightness == 0 ? _savedBrightness : _targetBrightness;
        if (_publishedValues.brightness != brightness) {
            _publishedValues.brightness = brightness;
            publish(MQTT::Client::formatTopic(FSPGM(_brightness_state)), true, String(brightness));
        }
        if (_publishedValues.color != int(getColor())) {
            _publishedValues.color = int(getColor());
            publish(MQTT::Client::formatTopic(FSPGM(_color_state)), true, getColor().implode(','));
        }
        if (_publishedValues.animation != int(_config.getAnimation())) {
            _publishedValues.animation = int(_config.getAnimation());
            publish(MQTT::Client::formatTopic(FSPGM(_effect_state)), true, KFCConfigurationClasses::Plugins::ClockConfigNS::ClockConfigType::getAnimationName(_config.getAnimation()));
        }
        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
            auto level = _getPowerLevel();
            if (_publishedValues.powerLevel != level) {
                _publishedValues.powerLevel = level;
                publish(MQTT::Client::formatTopic(F("power")), true, String(level, 2));
            }
        #endif
        #if IOT_LED_MATRIX_FAN_CONTROL
            int fanOn = _fanSpeed >= _config.min_fan_speed;
            if (_publishedValues.fanOn != fanOn) {
                _publishedValues.fanOn = fanOn;
                publish(MQTT::Client::formatTopic(F("/fan/state")), true, MQTT::Client::toBoolOnOff(fanOn));
            }
        #endif
    }
    _broadcastWebUI();
}
