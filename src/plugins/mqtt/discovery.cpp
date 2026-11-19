/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <fs_mapping.h>
#include "mqtt_client.h"
#include "logger.h"

#if DEBUG_MQTT_CLIENT
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace MQTT {

    namespace AutoDiscovery {

        static inline bool timeReached(uint32_t now, uint32_t time)
        {
            return static_cast<int32_t>(now - time) >= 0;
        }

        static inline uint32_t getMaxFreeBlock()
        {
            #if ESP8266
                return ESP.getMaxFreeBlockSize();
            #else
                return ESP.getMaxAllocHeap();
            #endif
        }

        static inline uint32_t getTopicHash(const String &topic)
        {
            return crc32b(topic.c_str(), topic.length());
        }

        void MemoryStats::start()
        {
            heapStart = ESP.getFreeHeap();
            heapMin = heapStart;
            blockStart = getMaxFreeBlock();
            blockMin = blockStart;
        }

        void MemoryStats::sample()
        {
            heapMin = std::min<uint32_t>(heapMin, ESP.getFreeHeap());
            blockMin = std::min<uint32_t>(blockMin, getMaxFreeBlock());
        }

        Discovery::Discovery(Client &client, uint32_t delayMillis, StatusCallback callback) :
            _client(client),
            _callback(callback),
            _start(millis()),
            _deadline(_start + delayMillis),
            _quiet(0),
            _componentIndex(0),
            _entityIndex(0),
            _phase(PhaseType::WAIT),
            _collecting(false),
            _gap(false)
        {
            __LDBG_printf("delay=%u", delayMillis);
            _client._discoveryStats = Stats();
            if (_callback) {
                _callback(StatusType::STARTED);
            }
        }

        Discovery::~Discovery()
        {
            if (_phase == PhaseType::COLLECT && _client.isConnected()) {
                _subscribe(false);
            }
            if (_phase != PhaseType::DONE) {
                if (_phase != PhaseType::WAIT) {
                    Logger_notice(F("MQTT auto discovery aborted"));
                }
                _client._discoveryStats.result = StatusType::FAILURE;
                if (_callback) {
                    _callback(StatusType::FAILURE);
                }
            }
        }

        const __FlashStringHelper *Discovery::getPhaseName(PhaseType phase)
        {
            switch (phase) {
                case PhaseType::WAIT:
                    return F("wait");
                case PhaseType::COLLECT:
                    return F("collect");
                case PhaseType::REMOVE:
                    return F("remove");
                case PhaseType::PUBLISH:
                    return F("publish");
                case PhaseType::DONE:
                    break;
            }
            return F("done");
        }

        MemoryStats *Discovery::_getMemoryStats()
        {
            switch (_phase) {
                case PhaseType::COLLECT:
                    return &_client._discoveryStats.collect;
                case PhaseType::REMOVE:
                    return &_client._discoveryStats.remove;
                case PhaseType::PUBLISH:
                    return &_client._discoveryStats.publish;
                default:
                    break;
            }
            return nullptr;
        }

        void Discovery::sample()
        {
            auto stats = _getMemoryStats();
            if (stats) {
                stats->sample();
            }
        }

        void Discovery::_subscribe(bool subscribe)
        {
            auto type = subscribe ? Packet::Type::SUBSCRIBE : Packet::Type::UNSUBSCRIBE;
            _client._enqueue(type, Entity::getConfigWildcardTopic(), nullptr, 0, QosType::AUTO_DISCOVERY, false);
            _client._enqueue(type, Entity::getConfig2ndLevelWildcardTopic(), nullptr, 0, QosType::AUTO_DISCOVERY, false);
        }

        void Discovery::loop(uint32_t now)
        {
            // pause after each message, the client calls loop() after it has been acknowledged
            if (_gap) {
                _gap = false;
                _deadline = now + MQTT_DISCOVERY_GAP_MS;
                return;
            }
            switch (_phase) {
                case PhaseType::WAIT:
                    if (timeReached(now, _deadline)) {
                        _collect(now);
                    }
                    break;
                case PhaseType::COLLECT:
                    if (!_collecting) {
                        // both subscriptions have been acknowledged
                        _collecting = true;
                        _quiet = now + MQTT_DISCOVERY_COLLECT_QUIET_MS;
                        _deadline = now + MQTT_DISCOVERY_COLLECT_MAX_MS;
                    }
                    else if (timeReached(now, _quiet) || timeReached(now, _deadline)) {
                        _subscribe(false);
                        decltype(_topics)().swap(_topics);
                        __LDBG_printf("dead topics=%u", _dead.size());
                        _phase = PhaseType::REMOVE;
                        _deadline = now;
                        _client._discoveryStats.remove.start();
                    }
                    break;
                case PhaseType::REMOVE:
                    if (timeReached(now, _deadline)) {
                        _remove();
                    }
                    break;
                case PhaseType::PUBLISH:
                    if (timeReached(now, _deadline)) {
                        _publish();
                    }
                    break;
                case PhaseType::DONE:
                    break;
            }
        }

        void Discovery::_collect(uint32_t now)
        {
            _start = now;
            _phase = PhaseType::COLLECT;
            _client._discoveryStats.collect.start();

            // the topics of the current entities
            auto &components = Client::getComponents();
            for (auto component : components) {
                auto count = component->getAutoDiscoveryCount();
                for (uint8_t i = 0; i < count; i++) {
                    std::unique_ptr<Entity> entity(component->getAutoDiscovery(FormatType::TOPIC, i));
                    if (entity) {
                        _topics.push_back(getTopicHash(entity->getTopic()));
                    }
                }
            }
            std::sort(_topics.begin(), _topics.end());
            sample();

            // the server sends the retained auto discovery of this device after subscribing
            _subscribe(true);
            sample();
        }

        void Discovery::onTopic(const char *topic)
        {
            if (_phase != PhaseType::COLLECT) {
                return;
            }
            _quiet = millis() + MQTT_DISCOVERY_COLLECT_QUIET_MS;
            String str = topic;
            if (!std::binary_search(_topics.begin(), _topics.end(), getTopicHash(str))) {
                __LDBG_printf("dead topic=%s", topic);
                _dead.emplace_back(std::move(str));
                sample();
            }
        }

        void Discovery::_remove()
        {
            if (_dead.empty()) {
                decltype(_dead)().swap(_dead);
                _phase = PhaseType::PUBLISH;
                _client._discoveryStats.publish.start();
                return;
            }
            // an empty retained message removes the entity
            if (_client.publish(_dead.back(), true, emptyString, QosType::AUTO_DISCOVERY)) {
                __LDBG_printf("remove topic=%s", _dead.back().c_str());
                _client._discoveryStats.removed++;
                _gap = true;
            }
            sample();
            _dead.pop_back();
        }

        void Discovery::_publish()
        {
            auto &components = Client::getComponents();
            auto &stats = _client._discoveryStats;
            while (_componentIndex < components.size()) {
                auto component = components[_componentIndex];
                if (_entityIndex >= component->getAutoDiscoveryCount()) {
                    _componentIndex++;
                    _entityIndex = 0;
                    continue;
                }
                std::unique_ptr<Entity> entity(component->getAutoDiscovery(FormatType::JSON, _entityIndex++));
                if (!entity) {
                    stats.skipped++;
                    continue;
                }
                entity->finalize();
                sample();
                // false if the message is too big
                if (_client.publish(entity->getTopic(), true, entity->getPayload(), QosType::AUTO_DISCOVERY)) {
                    stats.entities++;
                    sample();
                    _gap = true;
                    return;
                }
                stats.skipped++;
            }

            auto now = millis();
            _phase = PhaseType::DONE;
            stats.finished = now;
            stats.duration = now - _start;
            stats.result = StatusType::SUCCESS;
            Logger_notice(F("MQTT auto discovery published: %u entities, %u removed, %u skipped, %.1f seconds"), stats.entities, stats.removed, stats.skipped, stats.duration / 1000.0);
            if (_callback) {
                _callback(StatusType::SUCCESS);
            }
        }

    }

}
