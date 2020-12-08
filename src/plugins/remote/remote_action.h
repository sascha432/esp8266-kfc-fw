/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "remote_def.h"
#include "../src/plugins/mqtt/mqtt_client.h"
#include <ArduinoJson.h>
#include <Buffer.h>

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

namespace RemoteControl {

    // storage containers for the payload
    //
    // constructors are all PROGMEM safe
    //
    //   Binary
    //   String (stored as binary without NUL termination)
    //   Json (basically String with constructors for ArudinoJson, the ArudinoJson object isn't required after creating)

    namespace Payload {
        class Binary {
        public:
            Binary(const Binary &) = delete;
            Binary &operator=(const Data &) = delete;

            Binary() : _data(nullptr), _size(0) {}

            // if data is null, the memory is allocated and filled with 0
            Binary(const uint8_t *data, size_t size) : _data(new uint8_t[size]), _size(size) {
                if (_data == nullptr) {
                    _size = 0;
                }
                else if (_size) {
                    if (data) {
                        memcpy_P(_data, data, _size);
                    }
                    else {
                        std::fill_n(_data, _size, 0);
                    }

                }
            }

            Binary(Binary &&move) noexcept :
                _data(std::exchange(move._data, nullptr)),
                _size(std::exchange(move._size, 0))
            {}

            ~Binary() {
                if (_data) {
                    delete[] _data;
                }
            }

            Binary &operator=(Binary &&move) noexcept {
                clear();
                _data = std::exchange(move._data, nullptr);
                _size = std::exchange(move._size, 0);
                return *this;
            }

            void clear() {
                if (_data) {
                    delete[] _data;
                    _data = nullptr;
                }
                _size = 0;
            }

            const uint8_t *data() const {
                return _data;
            }

            size_t size() const {
                return _size;
            }

            uint8_t *begin() {
                return _data;
            }

            uint8_t *end() {
                return _data + _size;
            }

        protected:
            Binary(String &&move) : _data(nullptr), _size(move.length()) {
                _data = reinterpret_cast<uint8_t *>(MoveStringHelper::move(std::move(move), nullptr));
                if (!_data) {
                    _size = 0;
                }
            }

        protected:
            uint8_t *_data;
            uint8_t _size;
        };

        class String : public Binary
        {
        public:
            String() {}
            String(const char *data, size_t size) : Binary(reinterpret_cast<const uint8_t *>(data), size) {}
            String(const __FlashStringHelper *data) : Binary(reinterpret_cast<const uint8_t *>(data), strlen_P(reinterpret_cast<PGM_P>(data))) {}
            String(const ::String &str) : String(str.c_str(), str.length()) {}
            String(::String &&str) : Binary(std::move(str)) {}
        };

        class Json : public String {
        public:
            using String::String;

            Json() {}
            // Json(const JsonDocument &doc) {
            //     _size = measureJson(doc);
            //     _data = new uint8_t[_size];
            //     if (_data) {
            //         serializeJson(doc, _data, _size);
            //     }
            //     else {
            //         _data = nullptr;
            //         _size = 0;
            //     }
            // }
            // Json(const JsonDocument &doc, bool pretty) {
            //     _size = measureJsonPretty(doc);
            //     _data = new uint8_t[_size];
            //     if (_data) {
            //         serializeJsonPretty(doc, _data, _size);
            //     }
            //     else {
            //         _data = nullptr;
            //         _size = 0;
            //     }
            // }

            static Binary serializeFromJson(const JsonDocument &doc)
            {
                Binary payload(nullptr, measureJson(doc));
                if (payload.size()) {
                    serializeJson(doc, payload.begin(), payload.size());
                }
                return payload;
            }
        };

    }

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

    private:
        ActionIdType _id;
        ActionProtocolType _type;
    };

    class ActionMQTT : public Action {
    public:
        using QosType = MQTTClient::QosType;

    public:
        ActionMQTT(ActionIdType id = 0) : Action(id, ActionProtocolType::MQTT), _qos(QosType::EXACTLY_ONCE) {}

    private:
        String _topic;
        QosType _qos;
    };

    class ActionRaw : public Action {
    public:
        ActionRaw(ActionIdType id, ActionProtocolType type) : Action(id, type), _payload(), _port(0) {}

        // if hostname is an IP address and address is invalid, hostname is set to String() and the IP is stored in address
        ActionRaw(ActionIdType id, ActionProtocolType type, Payload::Binary &&payload, const String &hostname, const IPAddress &address, uint16_t port) :
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

        void setPayload(Payload::Binary &&payload)
        {
            _payload = std::move(payload);
        }

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

    protected:
        Payload::Binary _payload;
        String _hostname;
        IPAddress _address;
        uint16_t _port;
    };

    class ActionTCP : public ActionRaw {
    public:
        ActionTCP(ActionIdType id = 0) : ActionRaw(id, ActionProtocolType::TCP) {}
        ActionTCP(ActionIdType id, Payload::Binary &&payload, const Payload::Binary &successResponse, const String &hostname, const IPAddress &address, uint16_t port) :
            ActionRaw(id, ActionProtocolType::TCP, std::move(payload), hostname, address, port),
            _successResponse(std::move(Payload::Binary(successResponse.data(), successResponse.size())))
        {}

    private:
        Payload::Binary _successResponse;
    };

    class ActionUDP : public ActionRaw {
    public:
        ActionUDP(ActionIdType id = 0) : ActionRaw(id, ActionProtocolType::UDP) {}
        ActionUDP(ActionIdType id, Payload::Binary &&payload, const String &hostname, const IPAddress &address, uint16_t port) :
            ActionRaw(id, ActionProtocolType::UDP, std::move(payload), hostname, address, port)
        {}
        ActionUDP(ActionIdType id, Payload::Binary &&payload, const IPAddress &address, uint16_t port) :
            ActionRaw(id, ActionProtocolType::UDP, std::move(payload), String(), address, port)
        {}

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
