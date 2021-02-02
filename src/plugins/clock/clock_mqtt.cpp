/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
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


MQTTComponent::MQTTAutoDiscoveryPtr ClockPlugin::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0: {
            discovery->create(this, MQTT_NAME, format);
            discovery->addStateTopicAndPayloadOnOff(MQTTClient::formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(MQTTClient::formatTopic(FSPGM(_set)));
            discovery->addBrightnessStateTopic(MQTTClient::formatTopic(FSPGM(_brightness_state)));
            discovery->addBrightnessCommandTopic(MQTTClient::formatTopic(FSPGM(_brightness_set)));
            discovery->addBrightnessScale(SevenSegmentDisplay::kMaxBrightness);
            discovery->addRGBStateTopic(MQTTClient::formatTopic(FSPGM(_color_state)));
            discovery->addRGBCommandTopic(MQTTClient::formatTopic(FSPGM(_color_set)));
        }
        break;
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
        case 1: {
            discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(light_sensor), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(light_sensor)));
            discovery->addUnitOfMeasurement(String('%'));
        }
        break;
#endif
    }
    discovery->finalize();
    return discovery;
}

uint8_t ClockPlugin::getAutoDiscoveryCount() const
{
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
    return 2;
#else
    return 1;
#endif
}

void ClockPlugin::onConnect(MQTTClient *client)
{
    __LDBG_println();
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_set)));
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_color_set)));
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_brightness_set)));
    _publishState(client);
}

void ClockPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    __LDBG_printf("client=%p topic=%s payload=%s", client, topic, payload);

    _resetAlarm();

    if (!strcmp_end_P(topic, SPGM(_brightness_set))) {
        if (len) {
            auto value = strtoul(payload, nullptr, 0);
            setBrightness(value);
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
            auto red = (uint8_t)strtoul(payload, &endptr, 10);
            if (endptr && *endptr++ == ',') {
                auto green = (uint8_t)strtoul(endptr, &endptr, 10);
                if (endptr && *endptr++ == ',') {
                    auto blue = (uint8_t)strtoul(endptr, nullptr, 10);
                    setColorAndRefresh(Color(red, green, blue));
                    _saveStateDelayed();
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
    __DBG_printf("client=%p color=%s brightness=%u auto=%d", client, getColor().implode(',').c_str(), _targetBrightness, _autoBrightness);
#else
    __DBG_printf("client=%p color=%s brightness=%u", client, getColor().implode(',').c_str(), _targetBrightness);
#endif
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(FSPGM(_state)), true, String(_targetBrightness != 0));
        client->publish(MQTTClient::formatTopic(FSPGM(_brightness_state)), true, String(_targetBrightness));
        client->publish(MQTTClient::formatTopic(FSPGM(_color_state)), true, getColor().implode(','));
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
        client->publish(MQTTClient::formatTopic(FSPGM(light_sensor)), true, String(_autoBrightnessValue * 100, 0));
#endif
    }

    _broadcastWebUI();
}
