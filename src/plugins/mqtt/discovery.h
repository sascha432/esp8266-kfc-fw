/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "mqtt_base.h"

namespace MQTT {

    namespace AutoDiscovery {

        // free heap and largest free block at the start of a phase and the lowest values sampled during it
        struct MemoryStats {
            uint32_t heapStart;
            uint32_t heapMin;
            uint32_t blockStart;
            uint32_t blockMin;

            MemoryStats() :
                heapStart(0),
                heapMin(0),
                blockStart(0),
                blockMin(0)
            {
            }

            void start();
            void sample();
        };

        // result of the last auto discovery
        struct Stats {
            MemoryStats collect;
            MemoryStats remove;
            MemoryStats publish;
            uint32_t finished;      // millis() when it has finished, 0 = never
            uint32_t duration;      // milliseconds
            uint16_t entities;
            uint16_t removed;
            uint16_t skipped;
            StatusType result;

            Stats() :
                finished(0),
                duration(0),
                entities(0),
                removed(0),
                skipped(0),
                result(StatusType::STARTED)
            {
            }
        };

        // one auto discovery run, deleted when it has finished or the connection is lost
        //
        // wait     random delay after connecting, a short one if requested
        // collect  subscribe to the auto discovery topics of this device and record the retained ones that do not
        //          belong to any current entity
        // remove   publish an empty retained message to each of them, Home Assistant removes the entity
        // publish  publish the auto discovery of every entity, one at a time
        //
        // the client calls loop() only if no message is waiting to be sent or acknowledged, each message
        // is followed by a pause of MQTT_DISCOVERY_GAP_MS
        class Discovery {
        public:
            enum class PhaseType : uint8_t {
                WAIT,
                COLLECT,
                REMOVE,
                PUBLISH,
                DONE,
            };

            Discovery(Client &client, uint32_t delayMillis, StatusCallback callback);
            ~Discovery();

            void loop(uint32_t now);
            // a retained auto discovery topic of this device has been received
            void onTopic(const char *topic);
            // records the memory usage of the current phase
            void sample();

            bool isDone() const {
                return _phase == PhaseType::DONE;
            }

            PhaseType getPhase() const {
                return _phase;
            }

            static const __FlashStringHelper *getPhaseName(PhaseType phase);

        private:
            void _collect(uint32_t now);
            void _remove();
            void _publish();
            void _subscribe(bool subscribe);
            MemoryStats *_getMemoryStats();

            Client &_client;
            StatusCallback _callback;
            std::vector<uint32_t> _topics;  // crc32 of the topics of the current entities
            std::vector<String> _dead;      // retained topics without entity
            uint32_t _start;
            uint32_t _deadline;
            uint32_t _quiet;
            uint8_t _componentIndex;
            uint8_t _entityIndex;
            PhaseType _phase;
            bool _collecting;
            bool _gap;
        };

    }

}
