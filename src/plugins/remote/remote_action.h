/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "remote_def.h"
#if MQTT_SUPPORT
#include "../src/plugins/mqtt/mqtt_client.h"
#endif
#include <ArduinoJson.h>
#include <Buffer.h>
#include "payload.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

namespace RemoteControl {

    class Action;

    using ActionProtocolType = Plugins::RemoteControl::ActionProtocolType;
    using ActionIdType = Plugins::RemoteControl::ActionIdType;
    using ActionPtr = Action *;

    class Action {
    public:
        using Callback = std::function<void(bool)>;

    public:
        Action(ActionIdType id = 0, ActionProtocolType type = ActionProtocolType::NONE) :
            _id(id),
            _type(type)
        {
        }
        virtual ~Action() {}

        Action(const Action &) = delete;
        Action &operator=(const Action &) = delete;

        ActionIdType getId() const {
            return _id;
        }

        ActionProtocolType getType() const {
            return _type;
        }

        operator bool() const {
            return (_type != ActionProtocolType::NONE) && (_id != 0);
        }

        virtual size_t read(Stream &stream) {
            return 0;
        }

        virtual size_t write(Stream &stream) const {
            return 0;
        }

        // resolve marked hostnames to avoid DNS lookups
        // currently only supported by ActionTCP and ActionUDP
        // this allows to store the hostname but use the IP directly to avoid lookups
        // the IP is resolved once before storing the configuration in the flash memory
        virtual void resolve() {
        }

        virtual void execute(Callback callback) {
            callback(false);
        }

        virtual bool canSend() {
            return config.getWiFiUp() > 10;
        }

    private:
        ActionIdType _id;
        ActionProtocolType _type;
    };

    class ActionMQTT : public Action {
    public:
        using QosType = MQTT::QosType;

    public:
        ActionMQTT(ActionIdType id, String &&payload, QosType qos = QosType::EXACTLY_ONCE) : Action(id, ActionProtocolType::MQTT), _payload(std::move(payload)), _qos(qos) {}

        virtual void execute(Callback callback) override;

        virtual bool canSend() override {
            return Action::canSend() && MQTTClient::safeIsConnected();
        }

        const String &getPayload() const {
            return _payload;
        }

    private:
        String _payload;
        QosType _qos;
    };

    class ActionRaw : public Action {
    public:
        ActionRaw(ActionIdType id, ActionProtocolType type) : Action(id, type), _payload(), _port(0) {}

        // if hostname is an IP address and address is invalid, hostname is set to String() and the IP is stored in address
        template<typename _Ta = Payload::Binary>
        ActionRaw(ActionIdType id, ActionProtocolType type, _Ta &&payload, const String &hostname, const IPAddress &address, uint16_t port) :
            Action(id, type),
            _payload(std::move(payload)),
            _hostname(hostname),
            _address(address),
            _port(port)
        {
            if (!IPAddress_isValid(_address)) {
                IPAddress addr;
                if (addr.fromString(_hostname) && IPAddress_isValid(addr) && addr.toString() == _hostname) {
                    _address = addr;
                    _hostname = String();
                }
            }
        }

        // void setPayload(Payload::Binary &&payload)
        // {
        //     _payload = std::move(payload);
        // }

        // set to true to resolve hostname before storing the record
        // setting a hostname with a leading '!' has the same effect
        void setResolveOnce(bool resolveOnce) {
            if (_hostname.length()) {
                if (resolveOnce == true && _hostname.charAt(0) != '!') {
                    _hostname = '!' + _hostname;
                }
                else if (resolveOnce == false && _hostname.charAt(0) == '!') {
                    _hostname.remove(0 ,1);
                }
            }
        }

        const char *getHostname() const {
            auto str = _hostname.c_str();
            if (*str == '!') {
                str++;
            }
            return str;
        }

        bool isResolveOnce() const {
            return (_hostname.length() && _hostname.charAt(0) == '!');
        }

        IPAddress getAddress() const {
            return _address;
        }

        // resolve hostname or return stored IP address
        IPAddress getResolved() {
            if (_hostname.length() && (isResolveOnce() == false || !IPAddress_isValid(_address))) {
                if (WiFi.hostByName(getHostname(), _address) == 0 || !IPAddress_isValid(_address)) {
                    return IPAddress();
                }
            }
            return _address;
        }

        uint16_t getPort() const {
            return _port;
        }

        // resolve hostname and store in IP field to avoid DNS lookups
        virtual void resolve() override {
            if (isResolveOnce()) {
                _address = getResolved();
            }
        }

        virtual bool canSend() override {
            return Action::canSend() && IPAddress_isValid(getResolved());
        }

        // Payload::Binary

    protected:
        Payload::Binary _payload;
        String _hostname;
        IPAddress _address;
        uint16_t _port;
    };

    class ActionTCP : public ActionRaw {
    public:
        ActionTCP(ActionIdType id = 0) : ActionRaw(id, ActionProtocolType::TCP) {}

        template<typename _Ta = Payload::Binary>
        ActionTCP(ActionIdType id, _Ta &&payload, const Payload::Binary &successResponse, const String &hostname, const IPAddress &address, uint16_t port) :
            ActionRaw(id, ActionProtocolType::TCP, std::move(payload), hostname, address, port),
            _successResponse(std::move(Payload::Binary(successResponse.data(), successResponse.size())))
        {}

    private:
        Payload::Binary _successResponse;
    };

    class ActionUDP : public ActionRaw {
    public:
        ActionUDP(ActionIdType id = 0) : ActionRaw(id, ActionProtocolType::UDP) {}

        template<typename _Ta = Payload::Binary>
        ActionUDP(ActionIdType id, _Ta &&payload, const String &hostname, const IPAddress &address, uint16_t port) :
            ActionRaw(id, ActionProtocolType::UDP, std::move(payload), hostname, address, port)
        {
        }

        virtual void execute(Callback callback) override;
    };

    class ActionREST : public Action {
    public:
        ActionREST(ActionIdType id = 0) : Action(id, ActionProtocolType::REST) {}

    private:
        String _url;
        String _post;
        String _header;
    };

}
