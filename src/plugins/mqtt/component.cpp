/**
 * Author: sascha_lammers@gmx.de
 */

#include "component.h"
#include "mqtt_client.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace MQTT;

ComponentIterator::ComponentIterator() :
    _component(nullptr),
    _index(0),
    _iterator(static_cast<ComponentListIterator *>(nullptr))
{
}

ComponentIterator::ComponentIterator(ComponentPtr component, uint8_t index, ComponentListIterator *iterator) :
    _component(component),
    _index(index),
    _iterator(iterator)
{
    // find first component that is not empty
    while(!_iterator.isEnd() && _iterator->empty()) {
        ++_iterator;
        _index = 0;
    }

    if (_iterator.isEnd()) {
        _component = nullptr;
    }
    else {
        _component = *_iterator;
    }
}

ComponentIterator::pointer ComponentIterator::get(FormatType format) const
{
    return AutoDiscovery::EntitySharedPtr(_component->nextAutoDiscovery(format, _index));
}

ComponentIterator &ComponentIterator::operator++()
{
    if (_iterator.isValid()) {
        if (_iterator.isEnd()) {
            __DBG_panic("cannot increment beyond end()");
        }
        if (_index >= _iterator->getAutoDiscoveryCount() - 1) {
            _index = 0;
            do {
                ++_iterator;
                if (_iterator.isEnd()) {
                    _component = nullptr;
                    break;
                }
                _component = *_iterator;
            }
            while (_iterator->getAutoDiscoveryCount() == 0);
            return *this;
        }
    }
    ++_index;
    return *this;
}

ComponentIterator &ComponentIterator::operator--()
{
    if (_iterator.isValid()) {
        if (_index == 0) {
            if (_iterator.isBegin()) {
                __DBG_panic("cannot decrement beyond begin()");
            }
            do {
                --_iterator;
                _index = _iterator->getAutoDiscoveryCount();
            }
            while (!_iterator.isBegin() && _index);
            _component = *_iterator;
        }
    }
    --_index;
    return *this;
}

size_t ComponentIterator::size() const
{
    return _component->size();
}

bool ComponentIterator::empty() const
{
    return size() == 0;
}


Component::Component(ComponentType type) : _type(type)
{
}

Component::~Component()
{
}

void Component::onConnect(Client *client)
{
}

void Component::onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason)
{
}

void Component::onMessage(Client *client, char *topic, char *payload, size_t len)
{
}

void Component::onPacketAck(uint16_t packetId, PacketAckType type)
{
}

NameType Component::getNameByType(ComponentType type)
{
    switch(type) {
        case ComponentType::LIGHT:
            return FSPGM(mqtt_component_light);
        case ComponentType::SENSOR:
            return FSPGM(mqtt_component_sensor);
        case ComponentType::BINARY_SENSOR:
            return FSPGM(mqtt_component_binary_sensor);
        case ComponentType::DEVICE_AUTOMATION:
            return FSPGM(mqtt_component_device_automation);
        case ComponentType::AUTO_DISCOVERY:
            return F("auto_discovery");
        case ComponentType::SWITCH:
        default:
            return FSPGM(mqtt_component_switch);
    }
}
