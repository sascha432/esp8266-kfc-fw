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

MQTT::AutoDiscovery::EntityPtr ClockPlugin::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0: {
            if (!discovery->create(this, MQTT_NAME, format)) {
                return discovery;
            }
            discovery->addStateTopicAndPayloadOnOff(MQTTClient::formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(MQTTClient::formatTopic(FSPGM(_set)));
            discovery->addBrightnessStateTopic(MQTTClient::formatTopic(FSPGM(_brightness_state)));
            discovery->addBrightnessCommandTopic(MQTTClient::formatTopic(FSPGM(_brightness_set)));
            discovery->addBrightnessScale(kMaxBrightness);
            discovery->addRGBStateTopic(MQTTClient::formatTopic(FSPGM(_color_state)));
            discovery->addRGBCommandTopic(MQTTClient::formatTopic(FSPGM(_color_set)));
            discovery->addEffectStateTopic(MQTTClient::formatTopic(FSPGM(_effect_state)));
            discovery->addEffectCommandTopic(MQTTClient::formatTopic(FSPGM(_effect_set)));
            discovery->addEffectList(Clock::Config_t::getAnimationNamesJsonArray());
        }
        break;
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
        case 1: {
            if (!discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(light_sensor), format)) {
                return discovery;
            }
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(light_sensor)));
            discovery->addUnitOfMeasurement(String('%'));
        }
        break;
#endif
#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
        case 1 IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR( + 1): {
            if (!discovery->create(MQTTComponent::ComponentType::SENSOR, F("power"), format)) {
                return discovery;
            }
            discovery->addStateTopic(MQTTClient::formatTopic(F("power")));
            discovery->addUnitOfMeasurement(String('W'));
            discovery->addDeviceClass(F("power"));
        }
        break;
#endif
#if IOT_CLOCK_HAVE_MOTION_SENSOR
    case 1 IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR( + 1) IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION( + 1): {
            if (!discovery->create(MQTTComponent::ComponentType::BINARY_SENSOR, F("motion"), format)) {
                return discovery;
            }
            discovery->addStateTopic(MQTTClient::formatTopic(F("motion")));
            discovery->addPayloadOnOff();
        }
        break;
#endif
    }
    return discovery;
}

uint8_t ClockPlugin::getAutoDiscoveryCount() const
{
    return 1
        IF_IOT_CLOCK_HAVE_MOTION_SENSOR( + 1)
        IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR( + 1)
        IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION( + 1);
}

void ClockPlugin::onConnect(MQTTClient *client)
{
    __LDBG_printf("client=%p", client);
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_set)));
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_color_set)));
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_brightness_set)));
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_effect_set)));
    _publishState(client);
}

void ClockPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    __LDBG_printf("client=%p topic=%s payload=%s", client, topic, payload);

    _resetAlarm();

    if (!strcmp_end_P(topic, SPGM(_effect_set))) {
        auto animation = _getAnimationType(payload);
        if (animation != AnimationType::MAX) {
            setAnimation(static_cast<AnimationType>(animation));
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_brightness_set))) {
        if (len) {
            auto value = strtoul(payload, nullptr, 0);
            setBrightness(std::clamp<uint8_t>(value, 0, kMaxBrightness));
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_color_set))) {
        if (*payload == '#') {
            // rgb color code #FFEECC
            setColorAndRefresh(Color::fromString(payload));
            IF_IOT_CLOCK_SAVE_STATE(
                _saveStateDelayed();
            )
        }
        else {
            // red,green,blue
            char *endptr = nullptr;
            auto red = (uint8_t)strtoul(payload, &endptr, 10);
            if (endptr && *endptr++ == ',') {
                auto green = (uint8_t)strtoul(endptr, &endptr, 10);
                if (endptr && *endptr++ == ',') {
                    auto blue = (uint8_t)strtoul(endptr, nullptr, 10);
                    setColorAndRefresh(Color(red, green, blue));
                    IF_IOT_CLOCK_SAVE_STATE(
                        _saveStateDelayed();
                    )
                }
            }
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_set))) {
        auto res = MQTTClient::toBool(payload);
        if (res >= 0) {
            _setState(res);
        }
    }
}

void ClockPlugin::_publishState(MQTTClient *client)
{
    if (!client) {
        client = MQTTClient::getClient();
    }
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    // __LDBG_printf("client=%p color=%s brightness=%u auto=%d", client, getColor().implode(',').c_str(), _targetBrightness, _autoBrightness);
#else
    // __LDBG_printf("client=%p animation=%u (%p) color=%s brightness=%u", client, _config.animation, _animation, getColor().implode(',').c_str(), _targetBrightness);
#endif
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(FSPGM(_state)), true, String(_getEnabledState()));
        client->publish(MQTTClient::formatTopic(FSPGM(_brightness_state)), true, String(_targetBrightness == 0 ? _savedBrightness : _targetBrightness));
        client->publish(MQTTClient::formatTopic(FSPGM(_color_state)), true, getColor().implode(','));
        client->publish(MQTTClient::formatTopic(FSPGM(_effect_state)), true, Clock::Config_t::getAnimationName(_config.getAnimation()));
        IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(
            client->publish(MQTTClient::formatTopic(FSPGM(light_sensor)), true, String(_autoBrightnessValue * 100, 0));
        )
        IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(
            client->publish(MQTTClient::formatTopic(F("power")), true, String(_getPowerLevel(), 2));
        )
    }

    _broadcastWebUI();
}
