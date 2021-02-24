/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <fs_mapping.h>
#include "./stl_ext/non_std.h"
#include "mqtt_base.h"
#include "component.h"

namespace MQTT {

    namespace AutoDiscovery {

        // stores crc16 of topic and payload to compare against another set
        class CrcVector : public std::vector<uint32_t> {
        public:
            using std::vector<uint32_t>::vector;
            using Type = std::vector<uint32_t>;

            uint32_t insertSorted(const char *topic, size_t topicLength, const char *payload, size_t payloadLength) {
                uint32_t crc =(crc16_update(topic, topicLength) << 16) | (crc16_update(payload, payloadLength));
                auto iterator = std::upper_bound(begin(), end(), crc);
                insert(iterator, crc);
                return crc;
            }

            uint32_t insertSorted(const String &topic, const String &payload) {
                return insertSorted(topic.c_str(), topic.length(), payload.c_str(), payload.length());
            }

            iterator findTopic(const char *topic, size_t length)
            {
                auto crc16 = crc16_update(topic, length);
                for(auto iterator = begin(); iterator != end(); ++iterator) {
                    if (*iterator >> 16 == crc16) {
                        return iterator;
                    }
                }
                return end();
            }

            iterator findTopic(const String &topic)
            {
                return findTopic(topic.c_str(), topic.length());
            }

            // returns true if a topic has been added or update
            // false if removed (empty payload)
            bool update(const char *topic, size_t topicLength, const char *payload, size_t payloadLength)
            {
                auto iterator = findTopic(topic, topicLength);
                if (iterator != end()) {
                    if (payloadLength) {
                        *iterator = (*iterator & 0xffff0000) | crc16_update(payload, payloadLength);
                        return true;
                    }
                    else {
                        erase(iterator);
                    }
                }
                else if (payloadLength) {
                    insertSorted(topic, topicLength, payload, payloadLength);
                    return true;
                }
                return false;
            }

            bool update(const String &topic, const String &payload)
            {
                return update(topic.c_str(), topic.length(), payload.c_str(), payload.length());
            }

            bool operator==(const CrcVector &list) const
            {
                return size() == list.size() && memcmp(data(), list.data(), size() * sizeof(*data())) == 0;
            }

            uint32_t crc32b() const
            {
                return ::crc32b(data(), size() * sizeof(*data()));
            }
        };

        // provides iterators for all entities
        class List {
        public:
            using iterator = ComponentIterator;

            List() : _components(nullptr) {}
            List(ComponentVector &components, FormatType format) : _components(&components), _format(format) {}

            ComponentIterator begin();
            ComponentIterator end();
            ComponentIterator begin() const {
                return const_cast<List *>(this)->begin();
            }
            ComponentIterator end() const {
                return const_cast<List *>(this)->end();
            }

            CrcVector crc() const {
                CrcVector list;
                for(auto entity: *this) {
                    list.insertSorted(entity->getTopic(), entity->getPayload());
                }
                return list;
            }

            bool empty() const {
                return _components ? _components->empty() : true;
            }

            size_t size() const {
                return _components ? size(*_components) : 0;
            }

            static size_t size(ComponentVector &components) {
                size_t count = 0;
                for(auto &component: components) {
                    count += component->size();
                }
                return count;
            }

        private:
            ComponentVector *_components;
            FormatType _format;
        };
    }

}

