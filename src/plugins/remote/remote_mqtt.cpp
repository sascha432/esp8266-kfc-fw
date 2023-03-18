/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <BitsToStr.h>
#include "remote.h"

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using Plugins = KFCConfigurationClasses::PluginsType;

static inline String getButtonName(uint8_t n)
{
    return PrintString(F("button_%u"), n + 1);
}

static void addSubtypeTypePayload(MQTT::AutoDiscovery::EntityPtr discovery, uint8_t button, const __FlashStringHelper *subType)
{
    auto name = getButtonName(button);
    __LDBG_printf("discovery name=%s i=%u sub_type=%s", name.c_str(), button, subType);
    discovery->addPayloadAndSubType(name, subType, F("button_"));
}

MQTT::AutoDiscovery::EntityPtr MqttRemote::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    if (num == _autoDiscoveryCount - 1) {
        if (!discovery->create(ComponentType::BINARY_SENSOR, String(MQTT_LAST_WILL_TOPIC), format)) {
            return discovery;
        }
        auto baseTopic = MQTT::Client::getBaseTopicPrefix();
        discovery->addStateTopic(MQTT::Client::formatTopic(MQTT_LAST_WILL_TOPIC));
        discovery->addName(F("Device State"));
        discovery->addIcon(F("mdi:sleep"));
        discovery->addPayloadOn(MQTT_LAST_WILL_TOPIC_ONLINE);
        discovery->addPayloadOff(MQTT_LAST_WILL_TOPIC_OFFLINE);
        discovery->addObjectId(baseTopic + MQTT_LAST_WILL_TOPIC);
        return discovery;
    }

    if (!discovery->create(ComponentType::DEVICE_AUTOMATION, PrintString(F("remote%02x"), num), format)) {
        return discovery;
    }
    discovery->addAutomationType();
    discovery->addTopic(MQTT::AutoDiscovery::Entity::getTriggersTopic());
    auto cfg = Plugins::RemoteControl::getConfig();

    uint8_t count = 0;
    for(uint8_t i = 0; i < Plugins::RemoteControl::kButtonCount && count <= num; i++) {
        auto mqtt = cfg.actions[i].mqtt;
        mqtt.event_bits &= cfg.enabled.event_bits;
        if (mqtt.event_down) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("down"));
                break;
            }
        }
        if (mqtt.event_up) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("up"));
                break;
            }
        }
        if (mqtt.event_press) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("short_press"));
                break;
            }
        }
        if (mqtt.event_single_click) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("single_click"));
                break;
            }
        }
        if (mqtt.event_double_click) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("double_click"));
                break;
            }
        }
        if (mqtt.event_multi_click) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("repeated_click"));
                break;
            }
        }
        if (mqtt.event_long_press) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("long_press"));
                break;
            }
        }
        if (mqtt.event_hold) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("hold_repeat"));
                break;
            }
        }
        if (mqtt.event_hold_released) {
            if (count++ == num) {
                addSubtypeTypePayload(discovery, i, F("hold_release"));
                break;
            }
        }
    }
    return discovery;
}

void MqttRemote::_updateAutoDiscoveryCount()
{
    _autoDiscoveryCount = 1; // for awake sensor
    auto cfg = Plugins::RemoteControl::getConfig();
    if (!cfg.mqtt_enable) {
        return;
    }
    for(uint8_t i = 0; i < Plugins::RemoteControl::kButtonCount; i++) {
        _autoDiscoveryCount += numberOfSetBits(static_cast<uint16_t>(cfg.actions[i].mqtt.event_bits & cfg.enabled.event_bits));
        // __LDBG_printf("button=%u event=%s mqtt=%s num=%u sum=%u", i, BitsToStr<9, true>(cfg.enabled.event_bits).c_str(), BitsToStr<9, true>(cfg.actions[i].mqtt.event_bits).c_str(), numberOfSetBits(static_cast<uint16_t>(cfg.actions[i].mqtt.event_bits & cfg.enabled.event_bits)),count);
    }
}

uint8_t MqttRemote::getAutoDiscoveryCount() const
{
    return _autoDiscoveryCount;
}
