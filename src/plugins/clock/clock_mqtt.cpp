/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <stl_ext/algorithm.h>
#include "../src/plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_LED_MATRIX
#define MQTT_NAME "rgb_matrix"
#else
#define MQTT_NAME "clock"
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
    switch(static_cast<AutoDiscoverySwitchEnum>(num)) {
        case AutoDiscoverySwitchEnum::BRIGHTNESS: {
            if (!discovery->create(this, MQTT_NAME, format)) {
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
            #if MQTT_AUTO_DISCOVERY_USE_NAME
                #if IOT_LED_MATRIX
                    discovery->addName(MQTT::Client::getAutoDiscoveryName(F("LED Matrix")));
                #else
                    discovery->addName(MQTT::Client::getAutoDiscoveryName(F("Clock")));
                #endif
            #endif
            #if IOT_LED_MATRIX_HEXAGON_PANEL
                discovery->addIcon(F("mdi:hexagon-multiple-outline"));
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
            #if MQTT_AUTO_DISCOVERY_USE_NAME
                discovery->addName(name + F("Estimated Power"));
            #endif
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
            #if MQTT_AUTO_DISCOVERY_USE_NAME
                discovery->addName(MQTT::Client::getAutoDiscoveryName(F("Fan")));
            #endif
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
        if (animation != AnimationType::MAX) {
            setAnimation(static_cast<AnimationType>(animation));
            _saveStateDelayed();
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_brightness_set))) {
        if (len) {
            auto value = strtoul(payload, nullptr, 0);
            setBrightness(std::clamp<uint8_t>(value, 0, kMaxBrightness));
            _saveStateDelayed();
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_color_set))) {
        if (*payload == '#') {
            // rgb color code #FFEECC
            setColorAndRefresh(Color::fromString(payload));
            _saveStateDelayed();
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
                    _saveStateDelayed();
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
        publish(MQTT::Client::formatTopic(FSPGM(_state)), true, MQTT::Client::toBoolOnOff(_getEnabledState()));
        publish(MQTT::Client::formatTopic(FSPGM(_brightness_state)), true, String(_targetBrightness == 0 ? _savedBrightness : _targetBrightness));
        publish(MQTT::Client::formatTopic(FSPGM(_color_state)), true, getColor().implode(','));
        publish(MQTT::Client::formatTopic(FSPGM(_effect_state)), true, KFCConfigurationClasses::Plugins::ClockConfigNS::ClockConfigType::getAnimationName(_config.getAnimation()));
        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
            publish(MQTT::Client::formatTopic(F("power")), true, String(_getPowerLevel(), 2));
        #endif
        #if IOT_LED_MATRIX_FAN_CONTROL
            publish(MQTT::Client::formatTopic(F("/fan/state")), true, MQTT::Client::toBoolOnOff(_fanSpeed >= _config.min_fan_speed));
        #endif
    }
    _broadcastWebUI();
}
