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

    namespace AutoDiscovery {

        struct __attribute__((packed)) StateFileType {
            bool _valid;
            uint32_t _crc32b;
            uint32_t _lastAutoDiscoveryTimestamp;

            StateFileType() :
                _valid(false),
                _crc32b(0),
                _lastAutoDiscoveryTimestamp(0)
            {}

            StateFileType(uint32_t crc32, uint32_t lastAutoDiscoveryTimestamp = 0) :
                _valid(IS_TIME_VALID(lastAutoDiscoveryTimestamp) && crc32 != 0 && crc32 != 0xffffffff),
                _crc32b(crc32),
                _lastAutoDiscoveryTimestamp(lastAutoDiscoveryTimestamp)
            {}

            StateFileType(const AutoDiscovery::CrcVector &crcs, uint32_t lastAutoDiscoveryTimestamp = 0) :
                StateFileType(crcs.crc32b(), lastAutoDiscoveryTimestamp)
            {}

        };

        class Queue : public Component {
        public:
            Queue(Client &client);
            ~Queue();

            virtual AutoDiscovery::EntityPtr nextAutoDiscovery(FormatType format, uint8_t num);
            virtual uint8_t getAutoDiscoveryCount() const;

            // virtual void onConnect(Client *client) override;
            virtual void onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason) override;
            // virtual void onMessage(Client *client, char *topic, char *payload, size_t len) override;
            virtual void onPacketAck(uint16_t packetId, PacketAckType type) override;

            // clear queue and set last state to invalid
            void clear();

            // publish queue
            static constexpr uint32_t kPublishDefaultDelay = ~0;
            static constexpr uint32_t kPublishForce = true;

            void publish(bool force = false) {
                publish(force ? Event::milliseconds(0) : Event::milliseconds(kAutoDiscoveryInitialDelay));
            }

            void publish(Event::milliseconds delay);

            static bool isUpdateScheduled();
            static bool isEnabled();

        private:
            void _publishNextMessage();
            void _publishDone(bool success = true, uint16_t onErrorDelay = 15);

            static void _setState(const StateFileType &state);
            static StateFileType _getState();

        private:
            friend Client;

            Client &_client;
            Event::Timer _timer;
            uint32_t _size;
            uint32_t _discoveryCount;
            CrcVector _crcs;
            List _entities;
            List::iterator _iterator;
            uint16_t _packetId;
        };

    }

}
