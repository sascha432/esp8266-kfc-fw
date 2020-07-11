/**
 * Author: sascha_lammers@gmx.de
 */

#include <LoopFunctions.h>
#include "mqtt_persistant_storage.h"
#include "mqtt_client.h"

#if DEBUG_MQTT_PERSISTANT_STORAGE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

bool MQTTPersistantStorageComponent::_active = false;

MQTTPersistantStorageComponent::MQTTPersistantStorageComponent(ContainerPtr data, Callback callback) : MQTTComponent(ComponentTypeEnum_t::STORAGE), _data(data), _callback(callback), _ignoreMessages(false)
{
    _debug_println();
    _active = true;
}

MQTTPersistantStorageComponent::~MQTTPersistantStorageComponent()
{
    _debug_println();
    _active = false;
}

void MQTTPersistantStorageComponent::onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason)
{
    _debug_println();
    _remove();
}

void MQTTPersistantStorageComponent::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    _debug_printf_P(PSTR("topic=%s payload=%s len=%u ignore=%u\n"), topic, payload, len, _ignoreMessages);
    if (!_ignoreMessages) {
        KeyValueStorage::Container storedData;
        storedData.unserialize(payload);
        if (_data->size()) {
            storedData.merge(*_data);
        }
        if (_callback) {
            _callback(storedData);
        }
        _ignoreMessages = true;
        _timer.remove();
        PrintString newData;
        storedData.serialize(newData);
        client->publish(_topic, MQTTClient::getDefaultQos(), true, newData);
        _remove();
    }
}

bool MQTTPersistantStorageComponent::_begin(MQTTClient *client)
{
    _topic = MQTTClient::formatTopic(MQTTClient::NO_ENUM, F("/persistant_storage"));
    _debug_printf_P(PSTR("topic=%s\n"), _topic.c_str());
    // if we cannot subscribe, report an error
    for(uint8_t i = 0; i < 3; i++) {
        if (client->subscribeWithId(this, _topic.c_str(), MQTTClient::getDefaultQos())) {
            _timer.add(MQTT_PERSISTANT_STORAGE_TIMEOUT, false, [this](EventScheduler::TimerPtr) {
                _debug_printf_P(PSTR("MQTTPersistantStorageComponent data=%u callback=%p\n"), _data->size(), &_callback);
                if (_callback) {
                    _callback(*_data);
                }
                if (!_data->empty()) {
                    auto client = MQTTClient::getClient();
                    if (client) {
                        _ignoreMessages = true;
                        PrintString newData;
                        _data->serialize(newData);
                        client->publish(_topic, MQTTClient::getDefaultQos(), true, newData);
                    }
                }
                _remove();
            });
            return true;
        }
        delay(50);
    }
    return false;
}

void MQTTPersistantStorageComponent::_end(MQTTClient *client)
{
    _debug_printf_P(PSTR("client=%p\n"), client);
    if (client && _topic.length()) {
        client->unsubscribe(nullptr, _topic);
    }
}

void MQTTPersistantStorageComponent::_remove()
{
    MQTTPersistantStorageComponent::remove(this);
    // _debug_println();
    // LoopFunctions::callOnce([this]() {
    //     MQTTPersistantStorageComponent::remove(this);
    // });
}

bool MQTTPersistantStorageComponent::create(MQTTClient *client, ContainerPtr data, Callback callback)
{
    _debug_printf_P(PSTR("client=%p active=%u\n"), client, isActive());
    if (isActive()) {
        return false;
    }
    auto component = new MQTTPersistantStorageComponent(data, callback);
    client->registerComponent(component);
    auto result = component->_begin(client);
    if (!result) {
        delete component;
    }
    return result;
}

void MQTTPersistantStorageComponent::remove(MQTTPersistantStorageComponent *component)
{
    auto client = MQTTClient::getClient();
    _debug_printf_P(PSTR("client=%p\n"), client);
    if (client) {
        component->_end(client);
        client->unregisterComponent(component);
    }
    delete component;
}

bool MQTTPersistantStorageComponent::isActive()
{
    return _active;
}
