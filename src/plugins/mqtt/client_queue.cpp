/**
 * Author: sascha_lammers@gmx.de
 */
#include <Arduino_compat.h>
#include "mqtt_client.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace MQTT;

uint16_t MQTTClient::subscribe(ComponentPtr component, const String &topic, QosType qos)
{
    PacketQueue queue(0, millis());
    if (_queue.empty()) {
        queue = subscribeWithId(component, topic, qos);
    }
    if (queue._addToLocalQueue()) {
        _addQueue(QueueType::SUBSCRIBE, component, queue, topic, qos);
    }
    return queue.getInternalId();
}

uint16_t MQTTClient::unsubscribe(ComponentPtr component, const String &topic)
{
    PacketQueue queue(0, millis());
    if (_queue.empty()) {
        queue = unsubscribeWithId(component, topic);
    }
    if (queue._addToLocalQueue()) {
        _addQueue(QueueType::UNSUBSCRIBE, component, queue, topic, QosType::AT_LEAST_ONCE/*unsubscribe has not QoS but packets get acknowledged*/);
    }
    return queue.getInternalId();
}

uint16_t MQTTClient::publish(ComponentPtr component, const String &topic, bool retain, const String &payload, QosType qos)
{
    PacketQueue queue(0, millis());
    if (_queue.empty()) {
        queue = publishWithId(component, topic, retain, payload, qos);
    }
    if (queue._addToLocalQueue()) {
        _addQueue(QueueType::PUBLISH, component, queue, topic, qos, retain, payload);
    }
    return queue.getInternalId();
}

uint16_t MQTTClient::publish(const String &topic, bool retain, const String &payload, QosType qos)
{
    return publish(nullptr, topic, retain, payload, qos);
}

PacketQueue MQTTClient::subscribeWithId(ComponentPtr component, const String &topic, QosType qos, uint16_t internalPacketId)
{
    if (!isConnected() || !topic.length()) {
        __DBG_assert_printf(topic.length() != 0, "topic length is 0");
        return PacketQueue();
    }
    __LDBG_printf("component=%p topic=%s qos=%d conn=%s", component, topic.c_str(), qos, _connection());
    auto result = recordId(_client->subscribe(topic.c_str(), MQTTClient::_translateQosType(qos)), _getDefaultQos(qos), internalPacketId);
    if (result && component) {
        _topics.emplace_back(topic, component);
    }
    return result;
}

PacketQueue MQTTClient::unsubscribeWithId(ComponentPtr component, const String &topic, uint16_t internalPacketId)
{
    if (!isConnected() || !topic.length()) {
        __DBG_assert_printf(topic.length() != 0, "topic length is 0");
        return PacketQueue();
    }
    __LDBG_printf("component=%p topic=%s in_use=%d conn=%s", component, topic.c_str(), _topicInUse(component, topic), _connection());
    if (_topicInUse(component, topic)) {
        return PacketQueue(1, millis(), true); // another component is using the topic, mark as sent and delivered
    }
    auto result = recordId(_client->unsubscribe(topic.c_str()), QosType::AT_LEAST_ONCE, internalPacketId); // unsubscribe has no QoS but packets get acknowledged
    if (result && component) {
        _topics.erase(std::remove_if(_topics.begin(), _topics.end(), [component, topic](const ClientTopic &clientTopic) {
            return clientTopic.match(component, topic);
        }), _topics.end());
    }
    return result;
}

PacketQueue MQTTClient::publishWithId(ComponentPtr component, const String &topic, bool retain, const String &payload, QosType qos, uint16_t internalPacketId)
{
    if (!isConnected() || !topic.length()) {
        __DBG_assert_printf(topic.length() != 0, "topic length is 0 payload=%s", printable_string(payload.c_str(), payload.length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
        return PacketQueue();
    }
    __LDBG_printf("topic=%s qos=%d retain=%d payload=%s conn=%s", topic.c_str(), qos, retain, printable_string(payload.c_str(), payload.length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), _connection());
    return recordId(_client->publish(topic.c_str(), MQTTClient::_translateQosType(qos), retain, payload.c_str(), payload.length()), _getDefaultQos(qos), internalPacketId);
}

PacketQueue MQTTClient::recordId(int packetId, QosType qos, uint16_t internalPacketId)
{
    if (internalPacketId) {
        __LDBG_printf("packet_id=%u qos=%u internal=%u", packetId, qos, internalPacketId);
        auto queue = _packetQueue.find(PacketQueueInternalId(internalPacketId));
        if (queue) {
            __LDBG_printf("uppdating packet id %s", queue->toString().c_str());
            queue->update(packetId, millis());
            return *queue;
        }
    }
    if (packetId <= 0) {
        auto result = PacketQueue(0, millis(), false, internalPacketId);
        __LDBG_printf("invalid packet id %s", result.toString().c_str());
        return result;
    }
    if (qos == QosType::AT_MOST_ONCE) {
        auto result = PacketQueue(packetId, millis(), true, internalPacketId); // there is no tracking fuer QoS 0
        __LDBG_printf("sending packet without ack %s", result.toString().c_str());
        return result;
    }
    _packetQueue.emplace_back(packetId, millis(), false, internalPacketId);
    _packetQueueStartTimer();
    // __LDBG_printf("adding to packet queue(%u) %s", _packetQueue.size(), _packetQueue.back().toString().c_str());
    return _packetQueue.back();
}

PacketQueue MQTTClient::getQueueStatus(int packetId)
{
    auto iterator = std::find_if(_packetQueue.begin(), _packetQueue.end(), [packetId](const PacketQueue &queue) {
        return queue.getInternalId() == packetId;
    });
    if (iterator != _packetQueue.end()) {
        return PacketQueue();
    }
    return *iterator;
}

uint16_t MQTTClient::getNextInternalPacketId()
{
    return _mqttClient->_packetQueue.getNextPacketId();
}

void MQTTClient::_addQueue(QueueType type, ComponentPtr component, PacketQueue &queue, const String &topic, QosType qos, bool retain, const String &payload, uint16_t timeout)
{
    __LDBG_printf("type=%u topic=%s", type, topic.c_str());

    if (MQTTClient::_isMessageSizeExceeded(topic.length() + payload.length(), topic.c_str())) {
        queue = PacketQueue();
        return;
    }

    _queue.emplace_back(type, component, queue.getInternalId(), topic, qos, retain, payload, millis(), timeout);
    _packetQueue.setTimeout(queue.getInternalId(), kDefaultQueueTimeout + queue.getTimeout());
    _queueStartTimer();
}

void MQTTClient::_queueStartTimer()
{
    if (!_queue.getTimer()) {
        _Timer(_queue.getTimer()).add(kDeliveryQueueRetryDelay, true, [this](Event::CallbackTimerPtr timer) {
            _queueTimerCallback();
        });
    }
}

void MQTTClient::_packetQueueStartTimer()
{
    if (!_packetQueue.getTimer()) {
        _Timer(_packetQueue.getTimer()).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr timer) {
            _packetQueueTimerCallback();
        });
    }
}

void MQTTClient::_packetQueueTimerCallback()
{
    auto count = _packetQueue.size();
    _packetQueue.erase(std::remove_if(_packetQueue.begin(), _packetQueue.end(), [this](const PacketQueue &packet) {
        if (packet.canDelete()) {
#if DEBUG_MQTT_CLIENT
            if (!packet.isTimeout() && !packet.isDelivered()) {
                __DBG_printf_E("NOT delivered NO timeout %s", packet.toString().c_str());
            }
#endif
            if (packet.isTimeout() || !packet.isDelivered()) {
                _onErrorPacketAck(packet.getInternalId(), PacketAckType::TIMEOUT);
            }
        }
        return packet.canDelete();
    }), _packetQueue.end());
    if (count != _packetQueue.size()) {
        // __LDBG_printf("removed %u packets from queue", count - _packetQueue.size());
    }
}

void MQTTClient::_queueTimerCallback()
{
    // process queue
    for(auto iterator = _queue.begin(); iterator != _queue.end(); ++iterator) {
        bool success;
        auto &queue = *iterator;

        __LDBG_printf("queue=%u topic=%s space=%u timeout=%d", _queue.size(), queue.getTopic().c_str(), getClientSpace(), queue.getTimeout());
        if (queue.getRequiredSize() > getClientSpace()) {
            // to keep the order of messages being sent we need to wait and cannot try to send smaller ones
            success = false;
        }
        else {
            PacketQueue packetQueue;
            switch(queue.getType()) {
                case QueueType::SUBSCRIBE:
                    packetQueue = subscribeWithId(queue.getComponent(), queue.getTopic(), queue.getQos(), queue.getInternalPacketId());
                    break;
                case QueueType::UNSUBSCRIBE:
                    packetQueue = unsubscribeWithId(queue.getComponent(), queue.getTopic(), queue.getInternalPacketId());
                    break;
                case QueueType::PUBLISH:
                    packetQueue = publishWithId(queue.getComponent(), queue.getTopic(), queue.getRetain(), queue.getPayload(), queue.getQos(), queue.getInternalPacketId());
                    break;
            }
            if (packetQueue.isSent()) {
                success = true;
            }
            else if (!packetQueue.isValid()) {
                // something went wrong, return... connection might be lost
                __LDBG_printf_E("queueTimer aborted, retrying...");
                _queue.eraseBefore(iterator);
                return;
            }
            else {
                success = false;
            }
        }
        if (!success && !queue.isExpired()) {
            // error occured but message not expired
            // exit and retry to keep the sent order
            _queue.eraseBefore(iterator);
            return;
        }
        __LDBG_printf_E("queue id=%u has timed out", queue.getInternalPacketId());
    }

    __LDBG_print("delivery queue done");
    _queue.clear();
}
void MQTTClient::_onPacketAck(uint16_t packetId, PacketAckType type)
{
    auto queue = _packetQueue.find(PacketQueueId(packetId));
    // __LDBG_printf("type=%u packet=%u queue=%s", type, packetId, (queue ? queue->toString() : emptyString).c_str());
    if (queue) {
        queue->setDelivered();
        for(auto component: _components) {
            component->onPacketAck(queue->getInternalId(), type);
        }
    }
}

void MQTTClient::_onErrorPacketAck(uint16_t internalId, PacketAckType type)
{
#if DEBUG_MQTT_CLIENT
    auto queue = _packetQueue.find(PacketQueueInternalId(internalId));
    __DBG_printf_E("type=%u internal=%u queue=%s", type, internalId, (queue ? queue->toString() : emptyString).c_str());
#endif
    for(auto component: _components) {
        component->onPacketAck(internalId, type);
    }
}

bool MQTTClient::_isMessageSizeExceeded(size_t len, const char *topic)
{
    if (len > MQTT_MAX_MESSAGE_SIZE) {
        Logger_error(F("MQTT maximum message size exceeded: %u/%u: %s"), len, MQTT_MAX_MESSAGE_SIZE, topic);
        return true;
    }
    return false;
}

#if MQTT_GROUP_TOPIC

void MQTTClient::subscribeWithGroup(ComponentPtr component, const String &topic, QosType qos)
{
    if (!_queue.empty() || subscribeWithId(component, topic, qos) == 0) {
        _addQueue(QueueType::SUBSCRIBE, component, topic, qos, 0, String());
    }
    auto groupBaseTopic = ClientConfig::getGroupTopic();
    if (*groupBaseTopic) {
        String groupTopic;
        if (_getGroupTopic(component, groupBaseTopic, groupTopic)) {
            if (!_queue.empty() || subscribeWithId(component, groupTopic, qos) == 0) {
                _addQueue(QueueType::SUBSCRIBE, component, groupTopic, qos, 0, String());
            }
        }
    }
}

void MQTTClient::unsubscribeWithGroup(ComponentPtr component, const String &topic)
{
    if (!_queue.empty() || unsubscribeWithId(component, topic) == 0) {
        _addQueue(QueueType::UNSUBSCRIBE, component, topic, getDefaultQos(), 0, String());
    }
    auto groupBaseTopic = ClientConfig::getGroupTopic();
    if (*groupBaseTopic) {
        String groupTopic;
        if (_getGroupTopic(component, groupBaseTopic, groupTopic)) {
            if (!_queue.empty() || unsubscribeWithId(component, groupTopic) == 0) {
                _addQueue(QueueType::UNSUBSCRIBE, component, groupTopic, getDefaultQos(), 0, String());
            }
        }
    }
}

#endif

