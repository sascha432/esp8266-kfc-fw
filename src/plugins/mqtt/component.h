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

    class ComponentListIterator : public non_std::iterator<std::bidirectional_iterator_tag, Component, int8_t> {
    public:
        ComponentListIterator(nullptr_t, FormatType format = FormatType::JSON) :
            _components(nullptr),
            _component(nullptr),
            _format(format)
        {
        }

        ComponentListIterator(ComponentListIterator *iterator, FormatType format = FormatType::JSON) : ComponentListIterator(nullptr, format)
        {
            if (iterator) {
                _components = iterator->_components;
                _iterator = iterator->_iterator;
            }
        }

        ComponentListIterator(const ComponentVector &components, const ComponentVector::iterator &iterator, FormatType format = FormatType::JSON) :
            _components(const_cast<ComponentVector *>(&components)),
            _iterator(iterator),
            _component(nullptr),
            _format(format)
        {
        }

        ComponentListIterator(const ComponentVector &components, const ComponentPtr component, FormatType format = FormatType::JSON) :
            _components(const_cast<ComponentVector *>(&components)),
            _iterator(std::find_if(_components->begin(), _components->end(), [component](const ComponentPtr item) {
                return item == component;
            })),
            _component(_iterator == _components->end() ? component : nullptr),
            _format(format)
        {
        }

        ComponentListIterator(ComponentPtr component, FormatType format = FormatType::JSON) :
            _components(nullptr),
            _component(component),
            _format(format)
        {
        }

        inline __attribute__((__always_inline__))
        bool isValid() const {
            return _components != nullptr;
        }

        inline __attribute__((__always_inline__))
        bool isBegin() const {
            return _components && (_iterator == _components->begin());
        }

        inline __attribute__((__always_inline__))
        bool isEnd() const {
            return !_components || (_iterator == _components->end());
        }

        ComponentPtr operator->() const {
            return _components ?
                (isEnd() ? nullptr : *_iterator) :      // return *iterator if components are attached
                _component;
        }

        inline __attribute__((__always_inline__))
        ComponentPtr operator*() const {
            return operator->();
        }

        inline __attribute__((__always_inline__))
        ComponentListIterator &operator++() {
            ++_iterator;
            return *this;
        }

        inline __attribute__((__always_inline__))
        ComponentListIterator &operator--() {
            --_iterator;
            return *this;
        }

        inline __attribute__((__always_inline__))
        void setFormat(FormatType format) {
            _format = format;
        }

        inline __attribute__((__always_inline__))
        FormatType getFormat() const {
            return _format;
        }

    private:
        friend ComponentIterator;

        ComponentVector *_components;
        ComponentVector::iterator _iterator;
        ComponentPtr _component;
        FormatType _format;
    };

    class ComponentIterator : public non_std::iterator<std::bidirectional_iterator_tag, AutoDiscovery::Entity, int8_t, AutoDiscovery::EntitySharedPtr, AutoDiscovery::EntitySharedPtr> {
    public:
        using iterator = ComponentIterator;

        ComponentIterator();
        ComponentIterator(ComponentPtr component, uint8_t index, ComponentListIterator *iterator);

        iterator &operator++();
        iterator &operator--();

        pointer get(FormatType format) const;

        inline __attribute__((__always_inline__))
        pointer operator->() const {
            return get(_iterator.getFormat());
        }

        inline __attribute__((__always_inline__))
        reference operator*() const {
            return get(_iterator.getFormat());
        }

        inline __attribute__((__always_inline__))
        operator ComponentPtr() const {
            return _component;
        }

        bool operator==(const iterator &iter) const {
            return _index == iter._index && _component == iter._component;
        }

        bool operator!=(const iterator &iter) const {
            return _index != iter._index || _component != iter._component;
        }

        bool isValid() const;

        size_t size() const;
        bool empty() const;

    private:
        ComponentPtr _component;
        ComponentListIterator _iterator;
        uint8_t _index;
        uint8_t _size;
    };

    inline __attribute__((__always_inline__))
    bool ComponentIterator::isValid() const {
        return _component != nullptr;
    }

    inline __attribute__((__always_inline__))
    size_t ComponentIterator::size() const
    {
        return _size;
    }

    inline __attribute__((__always_inline__))
    bool ComponentIterator::empty() const
    {
        return _size == 0;
    }

    class ComponentBase {
    public:
        ComponentBase() : _timerInterval(0) {
        }
        virtual ~ComponentBase() {}

    protected:
        inline __attribute__((__always_inline__))
        bool hasClient() const {
            return _client != nullptr;
        }

        bool isConnected() const;

        // check return value for nullptr before using
        // inside callbacks the client is always valid
        inline __attribute__((__always_inline__))
        MQTT::Client *getClient() {
            return _client;
        }

        // before using client() check if hasClient() is true
        // inside callbacks the client is always valid
        inline __attribute__((__always_inline__))
        MQTT::Client &client() {
            return *_client;
        }

    protected:
        void registerTimer(uint32_t intervalMillis) {
            _timerCounter = 0;
            _lastTimerCallback = 0;
            _timerInterval = intervalMillis;
        }

        inline __attribute__((__always_inline__))
        void unregisterTimer() {
            _timerInterval = 0;
        }

    private:
        friend MQTT::Client;

        inline __attribute__((__always_inline__))
        void setClient(MQTT::ClientPtr client) {
            _client = client;
        }

        MQTT::ClientPtr _client;
        uint32_t _timerCounter;
        uint32_t _timerInterval;
        uint32_t _lastTimerCallback;
    };

    class Component : public ComponentBase {
    public:
        Component(ComponentType type);

        // inside any on*** callback, client() can be used to call the client
        // if the MQTT client needs to be called outside, hasClient() or getClient() != nullptr must be checked before

        // startup is called when a new client has been setup
        virtual void onStartup() {}
        // shutdown is called when the client is destroyed
        //
        // if the client is reconfigured following events occur in sequence
        // onDisconnect() (if connected, reason TCP_DISCONNECTED)
        // onShutdown()
        // onStartup()
        // onConnect() (after connecting)
        virtual void onShutdown() {}

        // subscribe to all topics in the onConnect callback
        virtual void onConnect() {}
        // after a disconnect, all subscribed topics and messages in the queue are discarded
        virtual void onDisconnect(AsyncMqttClientDisconnectReason reason) {}
        // subscribing to a topic will enable the onMessage callback
        virtual void onMessage(const char *topic, const char *payload, size_t len) {}
        // any message that is sent with QoS != 0 receives an ack packet with the id the publish/subscribe/unsubscribe method returned
        virtual void onPacketAck(uint16_t packetId, PacketAckType type) {}
        // registering a timer to update topics avoids terminating it onShutdown/onStartup and checking if the client is valid
        // the timer is not accurate, expect +-250ms depending how many timers are running and if they are sending any data
        // this prevents the TCP buffers from being filled up, which results in messages being queued and consuming much more memory/CPU power
        virtual void onTimer(uint32_t time, uint32_t timeMillis, uint32_t callCounter, Event::CallbackTimerPtr timer) {}

#if MQTT_AUTO_DISCOVERY
        // return auto discovery entity, num = 0 - (getAutoDiscoveryCount() - 1)
        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) {
            return nullptr;
        }
        // this method should return quickly
        virtual uint8_t getAutoDiscoveryCount() const {
            return 0;
        }

        inline __attribute__((__always_inline__))
        ComponentIterator begin() {
            return ComponentIterator(this, 0, nullptr);
        }

        inline __attribute__((__always_inline__))
        ComponentIterator end() {
            return ComponentIterator(this, size(), nullptr);
        }

        inline __attribute__((__always_inline__))
        ComponentIterator begin(ComponentListIterator *iterator) {
            return ComponentIterator(*(*iterator), 0, iterator);
        }

        inline __attribute__((__always_inline__))
        ComponentIterator end(ComponentListIterator *iterator) {
            return ComponentIterator(*(*iterator), size(), iterator);
        }

        inline __attribute__((__always_inline__))
        size_t size() const {
            return getAutoDiscoveryCount();
        }

        inline __attribute__((__always_inline__))
        bool empty() const {
            return size() == 0;
        }

#elif DEBUG
        // if the code does not compile, put an #if arround the methods that cause the error
        // auto discovery is not available in this case+mqtt
        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) final {
            return nullptr;
        }
        virtual uint8_t getAutoDiscoveryCount() const final {
            return 0;
        }
#endif

        inline __attribute__((__always_inline__))
        NameType getName() const {
            return getNameByType(_type);
        }

        inline __attribute__((__always_inline__))
        ComponentType getType() const {
            return _type;
        }

        static NameType getNameByType(ComponentType type);

    protected:
        // before calling, check if isConnected() is true
        uint16_t subscribe(const String &topic, QosType qos = QosType::DEFAULT);
        uint16_t unsubscribe(const String &topic);
        uint16_t publish(const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);

#if MQTT_AUTO_DISCOVERY
    private:
        friend ComponentIterator;

        AutoDiscovery::EntityPtr _getAutoDiscovery(FormatType format, uint8_t num) {
            auto discovery = getAutoDiscovery(format, num);
            __DBG_assert_printf(discovery != nullptr, "discovery=nullptr");
            if (discovery) {
                discovery->finalize();
            }
            return discovery;
        }
#endif

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
    struct  AutoDiscovery {
        using EntityPtr = MQTT::AutoDiscovery::EntityPtr;
        using Entity = MQTT::AutoDiscovery::Entity;
    };

    using Component::getAutoDiscoveryCount;
    using Component::getAutoDiscovery;
    using Component::onStartup;
    using Component::onShutdown;
    using Component::onConnect;
    using Component::onDisconnect;
    using Component::onMessage;
    using Component::onPacketAck;
    using Component::onTimer;
    using ComponentBase::isConnected;
    using Component::subscribe;
    using Component::unsubscribe;
    using Component::publish;
};
