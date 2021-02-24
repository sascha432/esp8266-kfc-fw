/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "mqtt_base.h"
#include "mqtt_client.h"
#include "component_proxy.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace MQTT;

void ComponentProxy::init()
{
    _client->registerComponent(this);
}

ComponentProxy::~ComponentProxy()
{
    __LDBG_printf("dtor this=%p", this);
    end();
    _client->unregisterComponent(this);
}

void ComponentProxy::onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason)
{
    __LDBG_printf("onDisconnect=%p", this);
    abort(ErrorType::DISCONNECT);
}

void ComponentProxy::onMessage(Client *client, char *topic, char *payload, size_t payloadLength)
{
    // __LDBG_printf("topic=%s payload=%s len=%u queue=%u", topic, printable_string(payload, payloadLength, 32).c_str(), payloadLength, _packetId);
    onMessage(topic, payload, payloadLength);
}

void ComponentProxy::onPacketAck(uint16_t packetId, PacketAckType type)
{
    __LDBG_printf("packet=%u<->%u type=%u can_yield=%u", _packetId, packetId, type, can_yield());
    if (_packetId == packetId) {
        __LDBG_printf("packet=%u type=%u", packetId, type);
        if (type == PacketAckType::TIMEOUT) {
            abort(ErrorType::TIMEOUT);
        }
        else {
            ++_iterator;
            _runNext();
        }
    }
}

AutoDiscovery::EntityPtr ComponentProxy::nextAutoDiscovery(FormatType format, uint8_t num)
{
    return nullptr;
}

uint8_t ComponentProxy::getAutoDiscoveryCount() const
{
    return 0;
}

void ComponentProxy::abort(ErrorType error)
{
    __LDBG_printf("abort=%u", error);

    // unsubscribe
    _packetId = 0;
    for(const auto &wildcard: _wildcards) {
        _client->unsubscribe(this, wildcard);
    }
    _wildcards.clear();

    LoopFunctions::callOnce([this, error]() {
        if (error != ErrorType::NONE) {
            onEnd(error);
        }
    });
}

void ComponentProxy::_runNext()
{
    // _packetId = 0;
    if (_iterator == _wildcards.end()) {
        __LDBG_printf("topic=<end>");
        if (_subscribe) {
            onBegin();
        }
        else {
            onEnd(ErrorType::SUCCESS);
        }
        return;
    }
    _packetId = _client->getNextInternalPacketId();
    __LDBG_printf("%ssubscribe topic=%s packet=%u", _subscribe ? PSTR("") : PSTR("un"), _iterator->c_str(), _packetId);
    auto queue = _subscribe ? _client->subscribeWithId(this, *_iterator, QosType::AUTO_DISCOVERY, _packetId) : _client->unsubscribeWithId(this, *_iterator, _packetId);
    if (!queue) {
        _packetId = 0;
        __LDBG_printf_E("queue is invalid queue=%s", queue.toString().c_str());
        abort(ErrorType::SUBSCRIBE);
        return;
    }
    // _packetId = queue.getInternalId();
    // __LDBG_printf("packet id=%u", _packetId);
}

void ComponentProxy::begin()
{
    __LDBG_printf("begin topics=%u", _wildcards.size());
    _runNext();
}

void ComponentProxy::end()
{
    if (!_wildcards.empty()) {
        __LDBG_printf("end topics=%u", _wildcards.size());

        // switch to unsubscribe
        _iterator = _wildcards.begin();
        _subscribe = false;
        _runNext();
    }
    else {
        __LDBG_printf("end");
    }
}

void ComponentProxy::onBegin()
{
}

void ComponentProxy::onEnd(ErrorType error)
{
}

void ComponentProxy::onMessage(char *topic, char *payload, size_t len)
{
}


CollectTopicsComponent::CollectTopicsComponent(Client *client, StringVector &&wildcards, Callback callback) :
    ComponentProxy(ComponentType::AUTO_DISCOVERY, client, std::move(wildcards)),
    _callback(callback)
{
}

void CollectTopicsComponent::onBegin()
{
    __LDBG_printf("waiting for messages. delay=%u", kSubscribeWaitTime);
    // add delay after subscribing
    _Timer(_timer).add(Event::milliseconds(kSubscribeWaitTime), false, [this](Event::CallbackTimerPtr timer) {
        end();
    });
}

void CollectTopicsComponent::onEnd(ErrorType error)
{
    __LDBG_printf("error=%u callback=%u timer=%u", error, (bool)_callback, (bool)_timer);
    _timer.remove();
    if (_callback) {
        __LDBG_printf("callback crcs=%u", _crcs.size());
        auto tmp = std::move(_callback); // make sure the callback is removed. the object might get destroyed inside the callback
        tmp(error, _crcs);
    }
}

void CollectTopicsComponent::onMessage(char *topic, char *payload, size_t payloadLength)
{
    // __LDBG_printf("topic=%s payload=%u retain=%u", topic, payloadLength, _client->getMessageProperties().retain);
    if (!_client->getMessageProperties().retain) { // ignore all messages that are not retained
        return;
    }
    _crcs.update(topic, strlen(topic), payload, payloadLength);
}


RemoveTopicsComponent::RemoveTopicsComponent(Client *client, StringVector &&wildcards, AutoDiscovery::CrcVector &&crcs, Callback callback) :
    ComponentProxy(ComponentType::AUTO_DISCOVERY, client, std::move(wildcards)),
    _crcs(std::move(crcs)),
    _callback(callback)
{
}

void RemoveTopicsComponent::onEnd(ErrorType error)
{
    __LDBG_printf("error=%u callback=%u packets=%u timer=%u", error, (bool)_callback, _packets.size(), (bool)_timer);
    _timer.remove();
    if (_callback) {
        __LDBG_printf("callback=%u", error);
        auto tmp = std::move(_callback); // make sure the callback is removed. the object might get destroyed inside the callback
        tmp(error);
    }
}

void RemoveTopicsComponent::onPacketAck(uint16_t packetId, PacketAckType type)
{
    ComponentProxy::onPacketAck(packetId, type);

    auto iterator = std::find(_packets.begin(), _packets.end(), packetId);
    if (iterator != _packets.end()) {
        __LDBG_printf("packetid=%u type=%u", packetId, type);
        if (type == PacketAckType::TIMEOUT) {
            abort(ErrorType::TIMEOUT);
            return;
        }
        else {
            _packets.erase(iterator);
            _Timer(_timer).add(Event::seconds(5), false, [this](Event::CallbackTimerPtr timer) {
                if (_packets.empty()) {
                    end();
                }
            });
        }
    }
}

void RemoveTopicsComponent::onMessage(char *topic, char *payload, size_t len)
{
    // __LDBG_printf("remove topic=%s payload=%s len=%u retained=%u", topic, printable_string(payload, len, 32).c_str(), len, _client->getMessageProperties().retain);
    if (!_client->getMessageProperties().retain || len == 0) { // ignore all messages that are not retained or empty
        return;
    }
    auto topicStr = String(topic);
    auto iterator = _crcs.findTopic(topicStr);
    if (iterator != _crcs.end()) {
        __LDBG_printf("topic found crc=%04x", (*iterator) >> 16);
        return;
    }
    auto packetId = _client->publish(nullptr, topicStr, false, String(), QosType::AUTO_DISCOVERY);
    if (!packetId) {
        abort(ErrorType::PUBLISH);
        return;
    }
    // collect packet ids
    _packets.emplace_back(packetId);
}
