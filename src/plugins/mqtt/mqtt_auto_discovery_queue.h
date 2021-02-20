/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <lwipopts.h>
#include "mqtt_component.h"

#ifndef DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#define DEBUG_MQTT_AUTO_DISCOVERY_QUEUE                         0
#endif

#ifndef DEBUG_MQTT_CLIENT_PAYLOAD_LEN
#define DEBUG_MQTT_CLIENT_PAYLOAD_LEN                           128
#endif

// initial delay after connect
// +-10%
#ifndef MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY
#define MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY                 30000
#endif

// delay between sending auto discovery
#ifndef MQTT_AUTO_DISCOVERY_QUEUE_DELAY
#define MQTT_AUTO_DISCOVERY_QUEUE_DELAY                         1500
#endif

class MQTTClient;

class MQTTAutoDiscoveryQueue {
private:
    struct __attribute__((packed)) StateFileType {
        bool _valid;
        uint16_t _crc;
        uint32_t _lastAutoDiscoveryTimestamp;
        StateFileType(uint16_t crc = 0, uint32_t lastAutoDiscoveryTimestamp = 0) : _valid(IS_TIME_VALID(lastAutoDiscoveryTimestamp) && crc && crc != ~0U), _crc(crc), _lastAutoDiscoveryTimestamp(lastAutoDiscoveryTimestamp) {}
    };

public:
    MQTTAutoDiscoveryQueue(MQTTClient &client);
    ~MQTTAutoDiscoveryQueue();

    // clear queue and set last state to invalid
    void clear();

    // publish queue
    void publish(bool force = false);

    static bool isUpdateScheduled();

private:
    void _timerCallback(Event::CallbackTimerPtr timer);
    void _publishDone(bool success = true, uint32_t delay = 0);

    static void _setState(const StateFileType &state);
    static StateFileType _getState();

private:
    friend MQTTClient;

    MQTTClient &_client;
    Event::Timer _timer;
    MQTTComponent::Vector::iterator _next;
    uint32_t _counter;
    uint32_t _size;
    uint32_t _discoveryCount;
    uint16_t _crc;
    bool _lastFailed;
#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
    uint32_t _start;
    size_t _maxQueue;
    uint32_t _maxQueueSkipCounter;
#endif
};
