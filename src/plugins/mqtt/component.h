/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <EventScheduler.h>
#include "mqtt_base.h"
#include "mqtt_strings.h"
#include "auto_discovery.h"

namespace MQTT {

    class Component {
    public:

        Component(ComponentType type);
        virtual ~Component();

#if MQTT_AUTO_DISCOVERY
        virtual AutoDiscoveryPtr nextAutoDiscovery(FormatType format, uint8_t num) = 0;
        virtual uint8_t getAutoDiscoveryCount() const = 0;

        uint8_t rewindAutoDiscovery();
        uint8_t getAutoDiscoveryNumber() const {
            return _autoDiscoveryNum;
        }
        void nextAutoDiscovery() {
            _autoDiscoveryNum++;
        }
        void retryAutoDiscovery() {
            _autoDiscoveryNum--;
        }
#endif

        virtual void onConnect(Client *client);
        virtual void onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason);
        virtual void onMessage(Client *client, char *topic, char *payload, size_t len);

        NameType getName() const {
            return getNameByType(_type);
        }
        ComponentType getType() const {
            return _type;
        }

        static NameType getNameByType(ComponentType type);

    private:
        ComponentType _type;
#if MQTT_AUTO_DISCOVERY
        friend AutoDiscoveryQueue;
        uint8_t _autoDiscoveryNum;
#endif
    };

}

class MQTTComponent : public MQTT::Component {
public:
    using MQTT::Component::Component;
    using Component = MQTT::Component;
    using ComponentPtr = MQTT::ComponentPtr;
    using ComponentType = MQTT::ComponentType;
    using FormatType = MQTT::FormatType;
    using AutoDiscoveryPtr = MQTT::AutoDiscoveryPtr;
    using AutoDiscovery = MQTT::AutoDiscovery;
    using AutoDiscoveryQueue = MQTT::AutoDiscoveryQueue;

    using MQTTAutoDiscovery = MQTT::AutoDiscovery;
};
