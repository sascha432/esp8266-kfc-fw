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
        ComponentListIterator(ComponentListIterator *iterator, FormatType format = FormatType::JSON) : _components(nullptr), _format(format)
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
            // return _components ? *_iterator : _component;
            return _components ? (isEnd() ? nullptr : *_iterator) : _component;
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
            return static_cast<ComponentPtr>(_component);
        }

        bool operator==(const iterator &iter) const {
            // __DBG_printf("compare index=%u %u component=%p %p", _index, iter._index, _component, iter._component);
            return _index == iter._index && _component == iter._component;
        }

        bool operator!=(const iterator &iter) const {
            return _index != iter._index || _component != iter._component;
        }

        size_t size() const;
        bool empty() const;

    private:
        ComponentPtr _component;
        uint8_t _index;
        ComponentListIterator _iterator;
    };


    class Component {
    public:
        Component(ComponentType type);
        virtual ~Component();

        // subscribe to all topics in the onConnect callback
        virtual void onConnect(Client *client);
        // after a disconnect, all subscribed topics and messages in the queue are discarded
        virtual void onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason);
        virtual void onMessage(Client *client, char *topic, char *payload, size_t len);

        virtual void onPacketAck(uint16_t packetId, PacketAckType type);

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

        size_t size() const {
            return getAutoDiscoveryCount();
        }

        bool empty() const {
            return size() == 0;
        }

#if MQTT_AUTO_DISCOVERY
        virtual AutoDiscovery::EntityPtr nextAutoDiscovery(FormatType format, uint8_t num) = 0;
        virtual uint8_t getAutoDiscoveryCount() const = 0;

        // uint8_t rewindAutoDiscovery();
        // uint8_t getAutoDiscoveryNumber() const {
        //     return _autoDiscoveryNum;
        // }
        // void nextAutoDiscovery() {
        //     _autoDiscoveryNum++;
        // }
        // void retryAutoDiscovery() {
        //     _autoDiscoveryNum--;
        // }
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

    private:
        ComponentType _type;
// #if MQTT_AUTO_DISCOVERY
//         friend AutoDiscovery::Queue;
//         uint8_t _autoDiscoveryNum;
// #endif
    };

}

class MQTTComponent : public MQTT::Component {
public:
    using MQTT::Component::Component;
    using Component = MQTT::Component;
    using ComponentPtr = MQTT::ComponentPtr;
    using ComponentType = MQTT::ComponentType;
    using FormatType = MQTT::FormatType;
    using AutoDiscoveryPtr = MQTT::AutoDiscovery::EntityPtr;
    using AutoDiscovery = MQTT::AutoDiscovery::Entity;
    using AutoDiscoveryQueue = MQTT::AutoDiscovery::Queue;

    using MQTTAutoDiscovery = MQTT::AutoDiscovery::Entity;
    using MQTTAutoDiscoveryPtr = MQTT::AutoDiscovery::EntityPtr;
};
