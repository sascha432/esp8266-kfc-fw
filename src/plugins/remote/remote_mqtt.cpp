/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <BitsToStr.h>
#include "remote.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

static inline String get_button_name(uint8_t n)
{
    return PrintString(F("button_%u"), n + 1);
}

static void add_subtype_type_payload(MQTT::AutoDiscovery::EntityPtr discovery, uint8_t button, const __FlashStringHelper *subType)
{
    __LDBG_printf("discovery i=%u sub_type=%s", button, subType);
    auto name = get_button_name(button);
    discovery->addSubType(name);
    discovery->addPayloadAndType(name, subType);
}

MqttRemote::AutoDiscoveryPtr MqttRemote::nextAutoDiscovery(FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(AutoDiscovery);
    if (!discovery->create(ComponentType::DEVICE_AUTOMATION, PrintString(F("remote%02x"), num), format)) {
        return discovery;
    }
    discovery->addAutomationType();
    discovery->addTopic(getMQTTTopic());
    auto cfg = Plugins::RemoteControl::getConfig();

    uint8_t count = 0;
    for(uint8_t i = 0; i < Plugins::RemoteControl::kButtonCount && count <= num; i++) {
        auto mqtt = cfg.actions[i].mqtt;
        mqtt.event_bits &= cfg.enabled.event_bits;
        if (mqtt.event_down) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("down"));
                break;
            }
        }
        if (mqtt.event_up) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("up"));
                break;
            }
        }
        if (mqtt.event_press) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("short_press"));
                break;
            }
        }
        if (mqtt.event_single_click) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("single_click"));
                break;
            }
        }
        if (mqtt.event_double_click) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("double_click"));
                break;
            }
        }
        if (mqtt.event_multi_click) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("repeated_click"));
                break;
            }
        }
        if (mqtt.event_long_press) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("long_press"));
                break;
            }
        }
        if (mqtt.event_hold) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("hold_repeat"));
                break;
            }
        }
        if (mqtt.event_hold_released) {
            if (count++ == num) {
                add_subtype_type_payload(discovery, i, F("hold_release"));
                break;
            }
        }
    }
    discovery->finalize();
    return discovery;
}

uint8_t MqttRemote::getAutoDiscoveryCount() const
{
    auto cfg = Plugins::RemoteControl::getConfig();
    if (!cfg.mqtt_enable) {
        return 0;
    }
    uint8_t count = 0;
    for(uint8_t i = 0; i < Plugins::RemoteControl::kButtonCount; i++) {
        count += numberOfSetBits(static_cast<uint16_t>(cfg.actions[i].mqtt.event_bits & cfg.enabled.event_bits));
        // __LDBG_printf("button=%u event=%s mqtt=%s num=%u sum=%u", i, BitsToStr<9, true>(cfg.enabled.event_bits).c_str(), BitsToStr<9, true>(cfg.actions[i].mqtt.event_bits).c_str(), numberOfSetBits(static_cast<uint16_t>(cfg.actions[i].mqtt.event_bits & cfg.enabled.event_bits)),count);
    }
    return count;
}
