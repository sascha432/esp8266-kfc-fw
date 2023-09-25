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

            struct Diff {
                uint16_t add;
                uint16_t remove;
                uint16_t modify;
                uint16_t unchanged;
                bool equal;

                Diff() :
                    add(0),
                    remove(0),
                    modify(0),
                    unchanged(0),
                    equal(false)
                {
                }

                void clear()
                {
                    *this = Diff();
                }
            };

            struct Data {
                union {
                    uint32_t value;
                    struct {
                        uint16_t payload;
                        uint16_t topic;
                    };
                };
                Data(uint32_t aCrc) : value(aCrc) {}
                Data(uint16_t aTopic, uint16_t aPayload) : payload(aPayload), topic(aTopic) {}
            };

            static_assert(sizeof(Data) == sizeof(uint32_t), "size does not match");

            void invalidate() {
                std::fill(begin(), end(), ~0U);
            }

            uint32_t insertSorted(const char *topic, size_t topicLength, const char *payload, size_t payloadLength) {
                //uint32_t crc = (crc16_update(topic, topicLength) << 16) | (crc16_update(payload, payloadLength));
                auto crc = Data(crc16_update(topic, topicLength), crc16_update(payload, payloadLength));
                auto iterator = std::upper_bound(begin(), end(), crc.value);
                insert(iterator, crc.value);
                return crc.value;
            }

            inline uint32_t insertSorted(const String &topic, const String &payload)
            {
                return insertSorted(topic.c_str(), topic.length(), payload.c_str(), payload.length());
            }

            iterator findTopic(uint16_t crc16)
            {
                for(auto iterator = begin(); iterator != end(); ++iterator) {
                    if (reinterpret_cast<Data &>(*iterator).topic == crc16) {
                        return iterator;
                    }
                }
                return end();
            }

            inline iterator findTopic(const char *topic, size_t length)
            {
                return findTopic(crc16_update(topic, length));
            }

            inline iterator findTopic(const String &topic)
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
                        // *reinterpret_cast<uint16_t *>(&(*iterator)) = crc16_update(payload, payloadLength);
                        // *iterator = (*iterator & 0xffff0000) | crc16_update(payload, payloadLength);
                        reinterpret_cast<Data &>(*iterator).payload = crc16_update(payload, payloadLength);
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

            inline bool update(const String &topic, const String &payload)
            {
                return update(topic.c_str(), topic.length(), payload.c_str(), payload.length());
            }

            inline bool operator==(const CrcVector &list) const
            {
                return size() == list.size() && memcmp(data(), list.data(), size() * sizeof(*data())) == 0;
            }

            inline bool operator!=(const CrcVector &list) const
            {
                return !operator==(list);
            }

            inline Diff difference(const CrcVector &list) const
            {
                Diff diff;
                if (*this == list) {
                    diff.unchanged = size();
                    diff.equal = true;
                    return diff;
                }
                for(auto iterator1 = begin(); iterator1 != end(); ++iterator1) {
                    auto iterator2 = const_cast<CrcVector &>(list).findTopic(reinterpret_cast<const Data &>(*iterator1).topic);
                    if (iterator2 != list.end()) {
                        if (*iterator1 == *iterator2) { // same
                            diff.unchanged++;
                        }
                        else { // payload changed
                            diff.modify++;
                        }
                    }
                    else { // not found
                        diff.add++;
                    }

                }
                for(auto iterator = list.begin(); iterator != list.end(); ++iterator) {
                    if (const_cast<CrcVector *>(this)->findTopic(reinterpret_cast<const Data &>(*iterator).topic) == end()) {
                        diff.remove++;
                    }
                }
                return diff;
            }

            inline uint32_t crc32b() const
            {
                return ::crc32b(data(), size() * sizeof(*data()));
            }
        };

        // provides iterators for all entities
        class List {
        public:
            using iterator = ComponentIterator;

            // NOTE: std:distance(end(), begin()) / std:distance(begin(), <invalid iterator>) crashes
            List() :
                _components(nullptr),
                _payloadSize(0)
            {
            }

            List(ComponentVector &components, FormatType format) :
                _components(&components),
                _format(format),
                _payloadSize(0)
            {
            }

            ComponentIterator begin();
            ComponentIterator end();

            ComponentIterator begin() const
            {
                return const_cast<List *>(this)->begin();
            }

            ComponentIterator end() const
            {
                return const_cast<List *>(this)->end();
            }

            CrcVector crc()
            {
                CrcVector list;
                _payloadSize = 0;
                for(const auto &entity: *this) {
                    if (!entity) {
                        __DBG_printf_E("entity=nullptr components=%p size=%u num=%u", _components, _components ? _components->size() : 0, list.size() + 1);
                        list.push_back(~0U);
                    }
                    else {
                        list.insertSorted(entity->getTopic(), entity->getPayload());
                        _payloadSize += entity->getPayload().length();
                    }
                }
                return list;
            }

            inline bool empty() const
            {
                return _components ? _components->empty() : true;
            }

            inline size_t size() const
            {
                return _components ? size(*_components) : 0;
            }

            inline size_t payloadSize() const
            {
                return _payloadSize;
            }

            static size_t size(ComponentVector &components)
            {
                size_t count = 0;
                for(auto &component: components) {
                    count += component->size();
                }
                return count;
            }

        private:
            ComponentVector *_components;
            FormatType _format;
            size_t _payloadSize;
        };
    }

}

