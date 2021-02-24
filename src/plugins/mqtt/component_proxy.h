/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "mqtt_base.h"
#include "mqtt_client.h"
#include "auto_discovery_list.h"

#if DEBUG_MQTT_CLIENT
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
            _subscribe(true)
        {
            init();
        }

        ComponentProxy(ComponentType type, Client *client, StringVector &&wildcards) :
            Component(type),
            _client(client),
            _wildcards(std::move(wildcards)),
            _iterator(_wildcards.begin()),
            _packetId(0),
            _subscribe(true)
        {
            init();
        }

        virtual ~ComponentProxy();

        void init();

        virtual AutoDiscovery::EntityPtr nextAutoDiscovery(FormatType format, uint8_t num);
        virtual uint8_t getAutoDiscoveryCount() const;

        virtual void onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason);
        virtual void onMessage(Client *client, char *topic, char *payload, size_t len) final;
        virtual void onPacketAck(uint16_t packetId, PacketAckType type) override;

        // abort does not execute end() and onEnd() in the same context but inside the main loop
        // code running directly after abort will be executed before end()/onEnd()
        // if executed inside a timer, the timer should be disarmed to avoid getting called again
        void abort(ErrorType error);
        void begin();
        void end();

        virtual void onBegin();
        virtual void onEnd(ErrorType error);
        virtual void onMessage(char *topic, char *payload, size_t len);

    protected:
        friend AutoDiscovery::Queue;

        Client *_client;
        StringVector _wildcards;
    private:
        void _runNext();

        StringVector::iterator _iterator;
        uint16_t _packetId;
        bool _subscribe; // _runNext(): true = subscribe, false = unsubscribe
    };

    // class that collects (retained) messages for wildcard topics
    class CollectTopicsComponent : public ComponentProxy {
    public:
        // wait after subscribing to the last topic to give the network time to transfer all retained messages
        static constexpr uint32_t kSubscribeWaitTime = 30000;   // milliseconds

        using Callback = std::function<void(ErrorType error, AutoDiscovery::CrcVector &crcs)>;

        CollectTopicsComponent(Client *client, StringVector &&wildcards, Callback callback = nullptr);

        void begin(Callback callback) {
            __LDBG_printf("collect topics");
            _callback = callback;
            ComponentProxy::begin();
        }

        virtual void onBegin() override;
        virtual void onEnd(ErrorType error) override;
        virtual void onMessage(char *topic, char *payload, size_t payloadLength) override;

    private:
        AutoDiscovery::CrcVector _crcs;
        Callback _callback;
        Event::Timer _timer;
    };

    class RemoveTopicsComponent : public ComponentProxy {
    public:
        using Callback = std::function<void(ErrorType error)>;

        RemoveTopicsComponent(Client *client, StringVector &&wildcards, AutoDiscovery::CrcVector &&crcs, Callback callback = nullptr);

        void begin(Callback callback) {
            __LDBG_printf("remove topics");
            _callback = callback;
            ComponentProxy::begin();
        }

        virtual void onEnd(ErrorType error) override;
        virtual void onMessage(char *topic, char *payload, size_t len) override;
        virtual void onPacketAck(uint16_t packetId, PacketAckType type) override;

    private:
        AutoDiscovery::CrcVector _crcs;
        Callback _callback;
        std::vector<uint16_t> _packets;
        Event::Timer _timer;
    };

}

#include <debug_helper_disable.h>
