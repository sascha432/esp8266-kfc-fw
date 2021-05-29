/**
 * Author: sascha_lammers@gmx.de
 */

#include "mqtt_base.h"
#include "auto_discovery_list.h"
#include "component.h"

using namespace MQTT;

ComponentIterator AutoDiscovery::List::begin()
{
    if (!_components || _components->empty()) {
        return ComponentIterator();
    }
    auto iterator = ComponentListIterator(*_components, _components->begin(), _format);
    return _components->front()->begin(&iterator);
}

ComponentIterator AutoDiscovery::List::end()
{
    if (!_components || _components->empty()) {
        return ComponentIterator();
    }
    auto iterator = ComponentListIterator(*_components, _components->end(), _format);
    return _components->back()->end(&iterator);
}
