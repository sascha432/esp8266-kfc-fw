/**
 * Author: sascha_lammers@gmx.de
 */

#include "mqtt_persistant_storage.h"
#include "mqtt_client.h"

#if DEBUG_MQTT_PERSISTANT_STORAGE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

bool MQTTPersistantStorageComponent::_active = false;

MQTTPersistantStorageComponent::MQTTPersistantStorageComponent(VectorPtr data, Callback callback) : MQTTComponent(ComponentTypeEnum_t::STORAGE), _data(data), _callback(callback), _ignoreMessages(false)
{
    _active = true;
}

MQTTPersistantStorageComponent::~MQTTPersistantStorageComponent()
{
    auto client = MQTTClient::getClient();
    if (client) {
        client->unregisterComponent(this);
        client->unsubscribe(nullptr, _topic);
    }
    _active = false;
}

void MQTTPersistantStorageComponent::onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason)
{
    // on disconnect topics are unsubscribed automatically
    delete this;
}

void MQTTPersistantStorageComponent::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    _debug_printf_P(PSTR("topic=%s payload=%s len=%u ignore=%u\n"), topic, payload, len, _ignoreMessages);
    if (!_ignoreMessages) {
        auto target = _unserialize(payload);
        if (_data->size()) {
            _merge(target, _data);
        }
        if (_callback) {
            _callback(target);
        }
        _ignoreMessages = true;
        _timer.remove();
        client->publish(_topic, MQTTClient::getDefaultQos(), true, _serialize(target));
        delete this;
    }
}

bool MQTTPersistantStorageComponent::_begin(MQTTClient *client)
{
    _topic = MQTTClient::formatTopic(MQTTClient::NO_ENUM, F("/persistant_storage"));
    // if we cannot subscribe, report an error
    for(uint8_t i = 0; i < 3; i++) {
        if (client->subscribeWithId(this, _topic.c_str(), MQTTClient::getDefaultQos())) {
            _timer.add(MQTT_PERSISTANT_STORAGE_TIMEOUT, false, [this](EventScheduler::TimerPtr) {
                if (!_data->empty()) {
                    auto client = MQTTClient::getClient();
                    if (client) {
                        _ignoreMessages = true;
                        client->publish(_topic, MQTTClient::getDefaultQos(), true, _serialize(_data));
                    }
                }
                _callback(VectorPtr(nullptr));
                delete this;
            });
            return true;
        }
        delay(50);
    }
    if (_callback) {
        _callback(VectorPtr(nullptr));
    }
    return false;
}

String MQTTPersistantStorageComponent::_serialize(VectorPtr data)
{
    StringVector list;
    for(const auto &item: *data) {
        list.emplace_back(item._key + '=' + item._value);
    }
    auto str = implode(',', list);
    _debug_printf_P(PSTR("str=%s\n"), str.c_str());
    return str;
}

MQTTPersistantStorageComponent::VectorPtr MQTTPersistantStorageComponent::_unserialize(const char *str)
{
    StringVector list;
    auto data = VectorPtr(new Vector());
    _debug_printf_P(PSTR("str=%s\n"), str);
    explode(str, ',', list);
    for(auto &item: list) {
        int pos = item.indexOf('=');
        if (pos != -1) {
            data->emplace_back(item.substring(0, pos), item.substring(pos + 1));
        }
        else {
            data->emplace_back(item, String());
        }
    }
    return data;
}

void MQTTPersistantStorageComponent::_merge(VectorPtr target, VectorPtr data)
{
    _debug_printf_P(PSTR("in=%s data=%s\n"), _serialize(target).c_str(), _serialize(data).c_str());

    for(const auto &item: *data) {
        bool found = false;
        for(auto &item2: *target) {
            if (item._key == item2._key) {
                item2._value = item._value;
                found = true;
                break;
            }
        }
        if (!found) {
            target->push_back(item);
        }
    }
    _debug_printf_P(PSTR("out=%s\n"), _serialize(target).c_str());
}

bool MQTTPersistantStorageComponent::create(MQTTClient *client, VectorPtr data, Callback callback)
{
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

bool MQTTPersistantStorageComponent::isActive()
{
    return _active;
}
