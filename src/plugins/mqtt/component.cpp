/**
 * Author: sascha_lammers@gmx.de
 */

#include "component.h"
#include "mqtt_client.h"
#include "mqtt_strings.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace MQTT;

ComponentIterator::ComponentIterator() :
    _component(nullptr),
    _iterator(nullptr),
    _index(0),
    _size(0)
{
}

ComponentIterator::ComponentIterator(ComponentPtr component, uint8_t index, ComponentListIterator *iterator) :
    _component(component),
    _iterator(iterator),
    _index(index),
    _size(0)
{
    // find first component that is not empty
    while (_iterator.isValid() && !_iterator.isEnd() && (_size = _iterator->size()) == 0) {
        ++_iterator;
        _index = 0; // reset index if the iterator changes
    }

    if (_iterator.isValid() && !_iterator.isEnd()) {
        // _size and _index were updated in the loop
        _component = *_iterator;
    }
    else {
        // reset index, size and component ptr
        _component = nullptr;
        _index = 0;
        _size = 0;
    }
}

ComponentIterator::pointer ComponentIterator::get(FormatType format) const
{
    return AutoDiscovery::EntitySharedPtr(_component->_getAutoDiscovery(format, _index));
}

ComponentIterator &ComponentIterator::operator++()
{
    __DBG_assertf(_iterator.isValid(), "iterator invalid component=%p index=%u size=%u components=%p (%u)", _iterator._component, _index, _size, _iterator._components, _iterator._components ? _iterator._components->size() : 0);
    if (_iterator.isValid()) {
        if (_iterator.isEnd()) {
            __DBG_panic("cannot increment beyond end()");
        }
        if (_index >= _size - 1) {
            do {
                ++_iterator;
                if (!_iterator.isValid() || _iterator.isEnd()) {
                    _component = nullptr;
                    _size = 0;
                    break;
                }
                _component = *_iterator;
                // update size after changing _iterator
                _size = _iterator->size();
            }
            while (_size == 0);
            _index = 0;
            return *this;
        }
    }
    ++_index;
    return *this;
}

ComponentIterator &ComponentIterator::operator--()
{
    __DBG_assertf(_iterator.isValid(), "iterator invalid component=%p index=%u size=%u components=%p (%u)", _iterator._component, _index, _size, _iterator._components, _iterator._components ? _iterator._components->size() : 0);
    if (_iterator.isValid()) {
        if (_index == 0) {
            if (_iterator.isBegin()) {
                __DBG_panic("cannot decrement beyond begin()");
            }
            do {
                --_iterator;
                if (!_iterator.isValid()) {
                    _size = 0;
                    _index = 0;
                    _component = nullptr;
                    return *this;
                }
                // update size after changing _iterator
                _size = _iterator->size();
                if (_iterator.isBegin()) {
                    break;
                }
            }
            while (_size == 0);
            _component = *_iterator;
            _index = _size; // gets decremented at the end of the method
        }
    }
    --_index;
    return *this;
}

Component::Component(ComponentType type) :
    ComponentBase(),
    _type(type)
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
        case ComponentType::FAN:
            return FSPGM(mqtt_component_fan);
        case ComponentType::AUTO_DISCOVERY:
            return F("auto_discovery");
        case ComponentType::SWITCH:
        default:
            return FSPGM(mqtt_component_switch);
    }
}
