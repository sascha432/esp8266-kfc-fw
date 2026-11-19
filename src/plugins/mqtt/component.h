/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include "mqtt_base.h"
#include "mqtt_strings.h"
#include "auto_discovery.h"

namespace MQTT {

    // base class for everything that publishes, subscribes or has auto discovery entities. a component is added
    // with Client::registerComponent() and removed with Client::unregisterComponent()
    //
    // all callbacks are called by the main loop
    class Component {
    public:
        Component(ComponentType type);
        virtual ~Component() {}

        // the connection has been established. subscriptions end with the connection, subscribe here
        virtual void onConnect() {}
        // the connection is lost or the client has been removed (MQTT disabled or reconfigured)
        virtual void onDisconnect(AsyncMqttClientDisconnectReason reason) {}
        // a message for a topic this component subscribed to
        virtual void onMessage(const char *topic, const char *payload, size_t len) {}

        // return an auto discovery entity, num = 0 - (getAutoDiscoveryCount() - 1). the caller deletes the object
        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) {
            return nullptr;
        }
        // this method should return quickly
        virtual uint8_t getAutoDiscoveryCount() const {
            return 0;
        }

        NameType getName() const {
            return getNameByType(_type);
        }

        ComponentType getType() const {
            return _type;
        }

        static NameType getNameByType(ComponentType type);

    protected:
        // the client exists if MQTT is enabled
        bool hasClient() const;
        bool isConnected() const;
        // nullptr if MQTT is disabled
        Client *getClient() const;
        // check hasClient() before using it outside a callback
        Client &client() const;

        // false if not connected or the outbox is full
        bool subscribe(const String &topic, QosType qos = QosType::DEFAULT);
        bool unsubscribe(const String &topic);
        bool publish(const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);

    private:
        ComponentType _type;
    };

}

// class for compatiblity
class MQTTComponent : public MQTT::Component {
public:
    using MQTT::Component::Component;
    using Component = MQTT::Component;
    using ComponentPtr = MQTT::ComponentPtr;
    using ComponentType = MQTT::ComponentType;
    using FormatType = MQTT::FormatType;
    struct AutoDiscovery {
        using EntityPtr = MQTT::AutoDiscovery::EntityPtr;
        using Entity = MQTT::AutoDiscovery::Entity;
    };

    using Component::getAutoDiscoveryCount;
    using Component::getAutoDiscovery;
    using Component::onConnect;
    using Component::onDisconnect;
    using Component::onMessage;
    using Component::isConnected;
    using Component::subscribe;
    using Component::unsubscribe;
    using Component::publish;
};
