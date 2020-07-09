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
#define MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY                 5000
#endif

// delay between sending auto discovery
#ifndef MQTT_AUTO_DISCOVERY_QUEUE_DELAY
#define MQTT_AUTO_DISCOVERY_QUEUE_DELAY                         1000
#endif

class MQTTClient;

class MQTTAutoDiscoveryQueue {
public:
    MQTTAutoDiscoveryQueue(MQTTClient &client);
    ~MQTTAutoDiscoveryQueue();

    void clear();
    void publish();

private:
    void _timerCallback(EventScheduler::TimerPtr timer);
    void _publishDone();

private:
    MQTTClient &_client;
    EventScheduler::Timer _timer;
    MQTTComponent::Vector::iterator _next;
    uint16_t _counter;
    uint32_t _size;
    uint32_t _discoveryCount;
#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
    uint32_t _start;
    size_t _maxQueue;
    uint32_t _maxQueueSkipCounter;
#endif
};
