/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include "./plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


MQTTComponent::MQTTAutoDiscoveryPtr ClockPlugin::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = MQTTAutoDiscoveryPtr(new MQTTAutoDiscovery());
    switch(num) {
        case 0: {
            discovery->create(this, F("clock"), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(MQTTClient::formatTopic(FSPGM(_set)));
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            discovery->addBrightnessStateTopic(MQTTClient::formatTopic(FSPGM(_brightness_state)));
            discovery->addBrightnessCommandTopic(MQTTClient::formatTopic(FSPGM(_brightness_set)));
            discovery->addBrightnessScale(SevenSegmentDisplay::kMaxBrightness);
            discovery->addRGBStateTopic(MQTTClient::formatTopic(FSPGM(_color_state)));
            discovery->addRGBCommandTopic(MQTTClient::formatTopic(FSPGM(_color_set)));
        }
        break;
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
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
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
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
        setBrightness(atoi(payload));
    }
    else if (!strcmp_end_P(topic, SPGM(_color_set))) {
        char *r, *g, *b;
        const char *comma = ",";
        r = strtok(payload, comma);
        if (r) {
            g = strtok(nullptr, comma);
            if (g) {
                b = strtok(nullptr, comma);
                if (b) {
                    setColorAndRefresh(Color(atoi(r), atoi(g), atoi(b)));
                }
            }
        }
    }
    else if (!strcmp_end_P(topic, SPGM(_set))) {
        auto value = atoi(payload);
        if (value) {
            if (_brightness == 0) {
                if (_savedBrightness) {
                    setBrightness(_savedBrightness);
                }
                else {
                    setBrightness(_config.brightness);
                }
            }
        }
        else {
            setBrightness(0);
        }
    }
}

void ClockPlugin::_publishState(MQTTClient *client)
{
    if (!client) {
        client = MQTTClient::getClient();
    }
    __LDBG_printf("client=%p color=%s brightness=%u auto_brightness=%d", client, _color.implode(',').c_str(), _brightness, _autoBrightness);
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(FSPGM(_state)), true, String(_color ? 1 : 0));
        client->publish(MQTTClient::formatTopic(FSPGM(_brightness_state)), true, String(_brightness));
        client->publish(MQTTClient::formatTopic(FSPGM(_color_state)), true, _color.implode(','));
#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
        client->publish(MQTTClient::formatTopic(FSPGM(light_sensor)), true, String(_autoBrightnessValue * 100, 0));
#endif
    }

    _broadcastWebUI();
}
