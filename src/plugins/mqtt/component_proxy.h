/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "mqtt_base.h"
#include "mqtt_client.h"
#include "auto_discovery_list.h"

#if DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace MQTT {

    // base class that subscribes to a list of topics
    class ComponentProxy : public Component {
    public:
        enum class ErrorType : uint8_t {
            SUCCESS = 0,
            NONE,
            UNKNOWN,
            TIMEOUT,
            DISCONNECT,
            ABORTED,
            PUBLISH,
            SUBSCRIBE,
        };

    public:
        ComponentProxy(ComponentType type, Client *client, const StringVector &wildcards) :
            Component(type),
            _client(client),
            _wildcards(wildcards),
            _iterator(_wildcards.begin()),
            _packetId(0),
            _subscribe(true),
            _canDestroy(true)
        {
            init();
        }

        ComponentProxy(ComponentType type, Client *client, StringVector &&wildcards) :
            Component(type),
            _client(client),
            _wildcards(std::move(wildcards)),
            _iterator(_wildcards.begin()),
            _packetId(0),
            _subscribe(true),
            _canDestroy(true)
        {
            init();
        }

        virtual ~ComponentProxy();

        void init();
        void unsubscribe();

        #if MQTT_AUTO_DISCOVERY

            virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) {
                return nullptr;
            }

            virtual uint8_t getAutoDiscoveryCount() const {
                return 0;
            }
            
        #endif

        virtual void onDisconnect(AsyncMqttClientDisconnectReason reason);

        virtual void onBegin();
        virtual void onEnd(ErrorType error);
        virtual void onPacketAck(uint16_t packetId, PacketAckType type);

        // abort does not execute end() and onEnd() in the same context but inside the main loop
        // code running directly after abort will be executed before end()/onEnd()
        // if executed inside a timer, the timer should be disarmed to avoid getting called again
        void abort(ErrorType error);
        void begin();
        void end();
        bool canDestroy() const;
        void stop(ComponentProxy *ptr);

    protected:
        friend AutoDiscovery::Queue;

        Client *_client;
        StringVector _wildcards;
    private:
        void _runNext();

        StringVector::iterator _iterator;
        uint16_t _packetId;
        bool _subscribe; // _runNext(): true = subscribe, false = unsubscribe
        bool _canDestroy;
    };

    // class that collects (retained) messages for wildcard topics
    class CollectTopicsComponent : public ComponentProxy {
    public:
        // timeout for the first message
        static constexpr uint32_t kInitialWaitTime = 20000;   // milliseconds
        // timeout after receiving the last message
        static constexpr uint32_t kOnMessageWaitTime = 5000;   // milliseconds

        using Callback = std::function<void(ErrorType error, AutoDiscovery::CrcVector &crcs)>;

        CollectTopicsComponent(Client *client, StringVector &&wildcards, Callback callback = nullptr);
        virtual ~CollectTopicsComponent() {
            if (_callback) {
                abort(ErrorType::ABORTED);
            }
        }

        void begin(Callback callback) {
            __LDBG_printf("collect topics");
            _callback = callback;
            ComponentProxy::begin();
        }

        void stop(std::unique_ptr<CollectTopicsComponent> &uniquePtr) {
            auto ptr = uniquePtr.release();
            if (ptr) {
                ComponentProxy::stop(ptr);
            }
        }

        virtual void onBegin() override;
        virtual void onEnd(ErrorType error) override;
        virtual void onMessage(const char *topic, const char *payload, size_t payloadLength) override;

    private:
        AutoDiscovery::CrcVector _crcs;
        Callback _callback;
        Event::Timer _timer;
    };

    class RemoveTopicsComponent : public ComponentProxy {
    public:
        using Callback = std::function<void(ErrorType error)>;

        // timeout for the first message
        static constexpr uint32_t kInitialTimeout = CollectTopicsComponent::kInitialWaitTime + CollectTopicsComponent::kOnMessageWaitTime;   // milliseconds
        // timeout per messaeg
        static constexpr uint32_t kOnMessageTimeout = CollectTopicsComponent::kOnMessageWaitTime;   // milliseconds

        RemoveTopicsComponent(Client *client, StringVector &&wildcards, AutoDiscovery::CrcVector &&crcs, Callback callback = nullptr);
        virtual ~RemoveTopicsComponent() {
            if (_callback) {
                abort(ErrorType::ABORTED);
            }
        }

        void begin(Callback callback) {
            __LDBG_printf("remove topics");
            _callback = callback;
            ComponentProxy::begin();
        }

        void stop(std::unique_ptr<RemoveTopicsComponent> &uniquePtr) {
            auto ptr = uniquePtr.release();
            if (ptr)  {
                ComponentProxy::stop(ptr);
            }
        }

        virtual void onBegin() override;
        virtual void onEnd(ErrorType error) override;
        virtual void onMessage(const char *topic, const char *payload, size_t len) override;
        virtual void onPacketAck(uint16_t packetId, PacketAckType type) override;

    private:
        AutoDiscovery::CrcVector _crcs;
        Callback _callback;
        std::vector<uint16_t> _packets;
        Event::Timer _timer;
    };

}

#include <debug_helper_disable.h>
