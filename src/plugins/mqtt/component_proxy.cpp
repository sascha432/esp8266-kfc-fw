/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "mqtt_base.h"
#include "mqtt_client.h"
#include "component_proxy.h"

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace MQTT;

void ComponentProxy::init()
{
    __LDBG_printf("ctor this=%p client=%p", this, _client);
    MQTTClient::registerComponent(this);
}

void ComponentProxy::unsubscribe()
{
    // unsubscribe
    _packetId = 0;
    for(const auto &wildcard: _wildcards) {
        _client->unsubscribe(nullptr, wildcard);
    }
    _wildcards.clear();
}

ComponentProxy::~ComponentProxy()
{
    __LDBG_printf("dtor this=%p", this);
    if (!_canDestroy) {
        __DBG_panic("object removed before clean");
    }
    MQTTClient::unregisterComponent(this);
}

void ComponentProxy::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    __LDBG_printf("onDisconnect=%p", this);
    abort(ErrorType::DISCONNECT);
}

void ComponentProxy::onPacketAck(uint16_t packetId, PacketAckType type)
{
    // __DBG_printf("this=%p", this);
    __LDBG_printf("packet=%u<->%u type=%u can_yield=%u", _packetId, packetId, type, can_yield());
    if (_packetId == packetId) {
        __LDBG_printf("packet=%u type=%u", packetId, type);
        if (type == PacketAckType::TIMEOUT) {
            LoopFunctions::callOnce([this]() {
                abort(ErrorType::TIMEOUT);
            });
        }
        else {
            ++_iterator;
            LoopFunctions::callOnce([this]() {
                _runNext();
            });
        }
    }
}

bool ComponentProxy::canDestroy() const
{
    return _canDestroy;
}

void ComponentProxy::stop(ComponentProxy *proxy)
{
    __DBG_printf("waiting for proxy to end %p", proxy);
    proxy->abort(ComponentProxy::ErrorType::ABORTED);
    _Scheduler.add(Event::seconds(1), true, [proxy](Event::CallbackTimerPtr timer) {
        if (proxy->canDestroy()) {
            __DBG_printf("delete proxy %p", proxy);
            delete proxy;
            timer->disarm();
        }
    });
}

void ComponentProxy::abort(ErrorType error)
{
    __LDBG_printf("abort=%u", error);

    unsubscribe();
    _client->unregisterComponent(this);

    LoopFunctions::callOnce([this, error]() {
        _canDestroy = true;
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
            _canDestroy = false;
            onBegin();
        }
        else {
            _canDestroy = true;
            onEnd(ErrorType::SUCCESS);
        }
        return;
    }
    _packetId = _client->getNextInternalPacketId();
    __LDBG_printf("%ssubscribe topic=%s packet=%u", _subscribe ? PSTR("") : PSTR("un"), _iterator->c_str(), _packetId);
    auto queue = _subscribe ? _client->subscribeWithId(this, *_iterator, QosType::AUTO_DISCOVERY, _packetId) : _client->unsubscribeWithId(this, *_iterator, _packetId);
    if (!queue) {
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
    _canDestroy = false;
    _runNext();
}

void ComponentProxy::end()
{
    if (!_wildcards.empty()) {
        __LDBG_printf("end topics=%u", _wildcards.size());
        _canDestroy = false;

        // switch to unsubscribe
        _iterator = _wildcards.begin();
        _subscribe = false;
        _runNext();
    }
    else {
        __LDBG_printf("end");
        _canDestroy = true;
    }
}

void ComponentProxy::onBegin()
{
}

void ComponentProxy::onEnd(ErrorType error)
{
}

CollectTopicsComponent::CollectTopicsComponent(Client *client, StringVector &&wildcards, IF_MQTT_AUTO_DISCOVERY_LOG2FILE(File &log, )Callback callback) :
    ComponentProxy(ComponentType::AUTO_DISCOVERY, client, IF_MQTT_AUTO_DISCOVERY_LOG2FILE(log, )std::move(wildcards)),
    _callback(callback)
{
}

void CollectTopicsComponent::onBegin()
{
    __LDBG_printf("waiting for messages. delay=%u", kInitialWaitTime);
    // add delay after subscribing
    _Timer(_timer).add(Event::milliseconds(kInitialWaitTime), false, [this](Event::CallbackTimerPtr timer) {
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

void CollectTopicsComponent::onMessage(const char *topic, const char *payload, size_t payloadLength)
{
    __LDBG_printf("topic=%s payload=%u retain=%u", topic, payloadLength, _client->getMessageProperties().retain);
    if (!_client->getMessageProperties().retain) { // ignore all messages that are not retained
        return;
    }
#if MQTT_AUTO_DISCOVERY_LOG2FILE
    if (_log) {
        _log.print(F("collect topic="));
        _log.print(topic);
        _log.print(F(" payload="));
        _log.print(payload);
        _log.print('\n');
    }
#endif
    _crcs.update(topic, strlen(topic), payload, payloadLength);
    if (_timer) {
        __LDBG_IF( auto result = )
        _timer->updateInterval(Event::milliseconds(kOnMessageWaitTime));
        __LDBG_IF(
            if (result) {
                __DBG_printf("changed timeout to %u", kOnMessageWaitTime);
            }
        )
    }
}


RemoveTopicsComponent::RemoveTopicsComponent(Client *client, StringVector &&wildcards, AutoDiscovery::CrcVector &&crcs, IF_MQTT_AUTO_DISCOVERY_LOG2FILE(File &log, )Callback callback) :
    ComponentProxy(ComponentType::AUTO_DISCOVERY, client, IF_MQTT_AUTO_DISCOVERY_LOG2FILE(log, )std::move(wildcards)),
    _crcs(std::move(crcs)),
    _callback(callback)
{
}

void RemoveTopicsComponent::onBegin()
{
    __LDBG_printf("waiting for messages. timeout=%u", kInitialTimeout);
    // add delay after subscribing
    _Timer(_timer).add(Event::milliseconds(kInitialTimeout), false, [this](Event::CallbackTimerPtr timer) {
        end();
#if MQTT_AUTO_DISCOVERY_LOG2FILE
        if (_log) {
            _log.printf_P(PSTR("no messages received within %u milliseconds\n"), kInitialTimeout);
        }
#endif
    });
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
    // __DBG_printf("this=%p", this);
    ComponentProxy::onPacketAck(packetId, type);

    auto iterator = std::find(_packets.begin(), _packets.end(), packetId);
    if (iterator != _packets.end()) {
        __LDBG_printf("packetid=%u type=%u", packetId, type);
        if (type == PacketAckType::TIMEOUT) {
#if MQTT_AUTO_DISCOVERY_LOG2FILE
            if (_log) {
                _log.printf_P(PSTR("timeout [%u]\n"), packetId);
            }
#endif
            LoopFunctions::callOnce([this]() {
                abort(ErrorType::TIMEOUT);
            });
            return;
        }
        else {
            _packets.erase(iterator);
            // wait additional 5 seconds after the last message
            _Timer(_timer).add(Event::milliseconds(kOnMessageTimeout), false, [this](Event::CallbackTimerPtr timer) {
                if (_packets.empty()) {
                    LoopFunctions::callOnce([this]() {
                        end();
                    });
                }
            });
        }
    }
}

void RemoveTopicsComponent::onMessage(const char *topic, const char *payload, size_t len)
{
    if (_timer) {
        _timer->updateInterval(Event::milliseconds(kOnMessageTimeout));
    }
    __LDBG_printf("remove topic=%s payload=%s len=%u retained=%u", topic, printable_string(payload, len, 32).c_str(), len, _client->getMessageProperties().retain);
    if (!_client->getMessageProperties().retain || len == 0) { // ignore all messages that are not retained or empty
        return;
    }
    auto topicStr = String(topic);
    auto iterator = _crcs.findTopic(topicStr);
    if (iterator != _crcs.end()) {
        __LDBG_printf("topic found crc=%04x topc=%s", (*iterator) >> 16, topic);
#if MQTT_AUTO_DISCOVERY_LOG2FILE
        if (_log) {
            _log.print(F("remove [skipped] topic="));
            _log.print(topic);
            _log.print('\n');
        }
#endif
        return;
    }
    auto packetId = _client->getNextInternalPacketId();
    _packets.emplace_back(packetId);
    __LDBG_printf("packetid=%u topic=%u", packetId, topic);
#if MQTT_AUTO_DISCOVERY_LOG2FILE
    if (_log) {
        _log.printf_P(PSTR("remove [%u] topic="), packetId);
        _log.print(topic);
        _log.print(F(" payload="));
        _log.print(payload);
        _log.print('\n');
    }
#endif
    auto queue = _client->publishWithId(nullptr, topicStr, true, String(), QosType::AUTO_DISCOVERY, packetId);
    if (!queue) {
#if MQTT_AUTO_DISCOVERY_LOG2FILE
        if (_log) {
            _log.printf_P(PSTR("error [%u]\n"), packetId);
        }
#endif
        abort(ErrorType::PUBLISH);
        return;
    }
}
