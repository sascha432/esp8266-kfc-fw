/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <lwipopts.h>
#include "mqtt_base.h"
#include "component.h"
#include "component_proxy.h"
#include "auto_discovery_list.h"

namespace MQTT {

    namespace AutoDiscovery {

        class Queue : public Component {
        public:
            Queue(Client &client);
            ~Queue();

            // virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num);
            // virtual uint8_t getAutoDiscoveryCount() const;

            // virtual void onConnect() override;
            virtual void onDisconnect(AsyncMqttClientDisconnectReason reason) override;
            // virtual void onMessage(const char *topic, const char *payload, size_t len) override;
            virtual void onPacketAck(uint16_t packetId, PacketAckType type) override;

            // clear queue and set last state to invalid
            void clear();

            // publish queue
            void publish(bool force = false) {
                runPublish(force ? 1000 : (KFCConfigurationClasses::Plugins::MQTTClient::getConfig().auto_discovery_delay * 1000U));
            }

            void runPublish(uint32_t delayMillis);

            void setForceUpdate(bool forceUpdate) {
                _forceUpdate = forceUpdate;
            }

            static bool isUpdateScheduled();
            static bool isEnabled(bool force = false);

        private:
            void _publishNextMessage();
            void _publishDone(bool success = true, uint16_t onErrorDelay = 15);

        private:
            friend Client;

            Client &_client;
            Event::Timer _timer;
            CrcVector::Diff _diff;
            CrcVector _crcs;
            std::unique_ptr<CollectTopicsComponent> _collect;
            std::unique_ptr<RemoveTopicsComponent> _remove;
            List _entities;
            List::iterator _iterator;
            uint16_t _packetId;
            bool _forceUpdate;
        };

    }

}
