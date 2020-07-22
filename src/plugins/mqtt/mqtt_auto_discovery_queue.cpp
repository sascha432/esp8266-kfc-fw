/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventTimer.h>
#include <MicrosTimer.h>
#include "mqtt_client.h"

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

MQTTAutoDiscoveryQueue::MQTTAutoDiscoveryQueue(MQTTClient &client) : _client(client)
{
}

MQTTAutoDiscoveryQueue::~MQTTAutoDiscoveryQueue()
{
    if (_timer.active()) {
        _timer.remove();
    }
}

void MQTTAutoDiscoveryQueue::clear()
{
   _debug_printf_P(PSTR("clear size=%u left=%u\n"), _client._components.size(), std::distance(_next, _client._components.end()));
    _timer.remove();
}

void MQTTAutoDiscoveryQueue::publish()
{
    _debug_printf_P(PSTR("components=%u delay=%u\n"), _client._components.size(), MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY);
    if (!_client._components.empty()) {
#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
        _maxQueue = 0;
        _maxQueueSkipCounter = 0;
        _start = millis();
#endif
        _size = 0;
        _counter = 0;
        _discoveryCount = 0;
        _next = _client._components.begin();
        (*_next)->rewindAutoDiscovery();

        auto intialDelay = MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY + ((MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY / 10) == 0 ? 0 : (rand() % (MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY / 10)));
        _timer.add(intialDelay, true, [this](EventScheduler::TimerPtr timer) {
            _timerCallback(timer);
        });
    }
}

void MQTTAutoDiscoveryQueue::_timerCallback(EventScheduler::TimerPtr timer)
{
#if MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY != MQTT_AUTO_DISCOVERY_QUEUE_DELAY
    // rearm timer if the first call
    if (_counter++ == 0 && _next == _client._components.begin()) {
        _debug_printf_P(PSTR("rearm %u\n"), MQTT_AUTO_DISCOVERY_QUEUE_DELAY);
        timer->rearm(MQTT_AUTO_DISCOVERY_QUEUE_DELAY, true);
    }
#endif

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
    _maxQueue = std::max(_maxQueue, _client._queue.size());
#endif

    // check client's queue
    if (!_client._queue.empty()) {
#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
        _debug_printf_P(PSTR("client queue %u, skipping\n"), _client._queue.size());
        _maxQueueSkipCounter++;
#endif
        return;
    }

    _debug_printf_P(PSTR("component #%u count=%u queue=%u counter=%u\n"), std::distance(_client._components.begin(), _next), (*_next)->getAutoDiscoveryCount() + 1, _client._queue.size(), _counter);

    // skip components without auto discovery
    if ((*_next)->getAutoDiscoveryCount() == 0) {
        do {
            if (++_next == _client._components.end()) {
                _publishDone();
                timer->detach();
                return;
            }
            _debug_printf_P(PSTR("component #%u count=%u\n"), std::distance(_client._components.begin(), _next), (*_next)->getAutoDiscoveryCount());
        }
        while((*_next)->rewindAutoDiscovery() == false);
    }

    auto component = *_next;
    auto discovery = component->nextAutoDiscovery(MQTTAutoDiscovery::FormatType::JSON, component->getAutoDiscoveryNumber());
    if (discovery) {
        // do we have enough space to send?
        auto msgSize = discovery->getMessageSize();
        if (msgSize < _client.getClientSpace()) {
#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
            _debug_printf_P(PSTR("topic=%s size=%u/%u\n"), discovery->getTopic().c_str(), msgSize, _client.getClientSpace());
            _debug_println(printable_string(discovery->getPayload().c_str(), discovery->getPayload().length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN));
#endif
            _discoveryCount++;
            _size += msgSize;
            _client.publish(discovery->getTopic(), true, discovery->getPayload());

        } else {
#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
            _maxQueueSkipCounter++;
            _debug_printf_P(PSTR("tcp buffer full: %u > %u\n"), msgSize, _client.getClientSpace());
#endif
            if (!MQTTClient::_isMessageSizeExceeded(msgSize, discovery->getTopic().c_str())) {
                // retry if size not exceeded
                component->_autoDiscoveryNum--;
            }
        }
        delete discovery;
        discovery = nullptr;
        return;
    }

    if (++_next == _client._components.end()) {
        _publishDone();
        timer->detach();
    } else {
        (*_next)->rewindAutoDiscovery();
    }
}

void MQTTAutoDiscoveryQueue::_publishDone()
{
#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
    auto dur = get_time_diff(_start, millis());
    _debug_printf_P(PSTR("done components=%u discovery=%u size=%u time=%.4fs max_queue=%u queue_skip=%u iterations=%u\n"), _client._components.size(), _discoveryCount, _size, dur / 1000.0, _maxQueue, _maxQueueSkipCounter, _counter);
#endif
    Logger_notice(F("MQTT auto discovery published [components=%u, size=%s]"), _discoveryCount, formatBytes(_size).c_str());
    _client._autoDiscoveryQueue.reset();
}
