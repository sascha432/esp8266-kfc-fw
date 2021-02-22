/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <lwipopts.h>
#include "mqtt_base.h"
#include "component.h"
#include "auto_discovery_list.h"

namespace MQTT {

    struct __attribute__((packed)) StateFileType {
        bool _valid;
        uint16_t _crc;
        uint32_t _lastAutoDiscoveryTimestamp;
        StateFileType(uint16_t crc = 0, uint32_t lastAutoDiscoveryTimestamp = 0) : _valid(IS_TIME_VALID(lastAutoDiscoveryTimestamp) && crc && crc != ~0U), _crc(crc), _lastAutoDiscoveryTimestamp(lastAutoDiscoveryTimestamp) {}
    };

    class AutoDiscoveryQueue {
    public:
        AutoDiscoveryQueue(Client &client);
        ~AutoDiscoveryQueue();

        // clear queue and set last state to invalid
        void clear();

        // publish queue
        void publish(bool force = false);

        void list(Print &output, bool crc);

        static bool isUpdateScheduled();

    private:
        void _timerCallback(Event::CallbackTimerPtr timer);
        void _publishDone(bool success = true, uint32_t delay = 0);

        static void _setState(const StateFileType &state);
        static StateFileType _getState();

    private:
        friend Client;

        Client &_client;
        Event::Timer _timer;
        ComponentVector::iterator _next;
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

}
