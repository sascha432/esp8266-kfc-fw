/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if MDNS_PLUGIN

#include <Arduino_compat.h>
#include <memory>
#if ESP8266
#include <ESP8266mDNS.h>
#elif ESP32
#include <ESPmDNS.h>
#endif

namespace MDNSResolver {

    class Query;

    enum class ResponseType {
        NONE = 0,
        TIMEOUT,
        RESOLVED,
    };

    using ResolvedCallback = std::function<void(const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, ResponseType type)>;

    using QueryPtr = std::shared_ptr<Query>;
    using Queries = std::list<QueryPtr>;

    #if ESP8266

        class MDNSServiceInfo : public MDNSResponder::MDNSServiceInfo {
        public:
            using ::MDNSResponder::MDNSServiceInfo::MDNSServiceInfo;

            IPAddress findIP4Address(const IPAddress &myAddress);
            char *findTxtValue(const String &key);
        };

        class Query {
        public:
            enum class StateType {
                NONE = 0,
                STARTED,
                FINISHED,
            };
        public:
            Query(const String &name, const String &service, const String &proto, const String &addressValue, const String &portValue, const String &fallback, uint16_t port, const String &prefix, const String &suffix, ResolvedCallback callback, uint16_t timeout = 5000);
            ~Query();

            void begin();
            void end();

            StateType getState() const;
            void checkTimeout();

            void serviceCallback(bool map, MDNSResolver::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent);

            void createZeroConf(Print &output) const;
            String createZeroConfString() const;

            static void dnsFoundCallback(const char *name, const ip_addr *ipaddr, void *arg);

        private:
            static constexpr uint8_t DATA_COLLECTED_NONE = 0x00;
            static constexpr uint8_t DATA_COLLECTED_PORT = 0x01;
            static constexpr uint8_t DATA_COLLECTED_ADDRESS = 0x02;
            static constexpr uint8_t DATA_COLLECTED_HOSTNAME = 0x04;

            static constexpr uint32_t END_TIME_NOT_STARTED = 0;
            static constexpr uint32_t END_TIME_FINISHED = ~0;

            String _name;
            uint32_t _endTime;
            String _service;
            String _proto;
            String _addressValue;
            String _portValue;
            String _fallback;
            String _hostname;
            IPAddress _address;
            String _prefix;
            String _suffix;
            ResolvedCallback _callback;
            MDNSResponder::hMDNSServiceQuery _serviceQuery;
            uint16_t _port;
            uint16_t _fallbackPort;
            uint16_t _timeout;
            uint8_t _dataCollected;
            bool _resolved;
            bool _isAddress;
            bool _isPort;
        };

    #elif ESP32

        class MDNSServiceInfo /*: public MDNSResponder::MDNSServiceInfo*/ {
        public:
            // using ::MDNSResponder::MDNSServiceInfo::MDNSServiceInfo;

            IPAddress findIP4Address(const IPAddress &myAddress);
            char *findTxtValue(const String &key);
        };

        class Query {
        public:
            enum class StateType {
                NONE = 0,
                STARTED,
                FINISHED,
            };
        public:
            Query(const String &name, const String &service, const String &proto, const String &addressValue, const String &portValue, const String &fallback, uint16_t port, const String &prefix, const String &suffix, ResolvedCallback callback, uint16_t timeout = 5000);
            ~Query();

            void begin();
            void end();

            StateType getState() const;
            void checkTimeout();

            // void serviceCallback(bool map, MDNSResolver::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent);

            void createZeroConf(Print &output) const;
            String createZeroConfString() const;

            static void dnsFoundCallback(const char *name, const ip_addr *ipaddr, void *arg);

        private:
            static constexpr uint8_t DATA_COLLECTED_NONE = 0x00;
            static constexpr uint8_t DATA_COLLECTED_PORT = 0x01;
            static constexpr uint8_t DATA_COLLECTED_ADDRESS = 0x02;
            static constexpr uint8_t DATA_COLLECTED_HOSTNAME = 0x04;

            static constexpr uint32_t END_TIME_NOT_STARTED = 0;
            static constexpr uint32_t END_TIME_FINISHED = ~0;

            String _name;
            uint32_t _endTime;
            String _service;
            String _proto;
            String _addressValue;
            String _portValue;
            String _fallback;
            String _hostname;
            IPAddress _address;
            String _prefix;
            String _suffix;
            ResolvedCallback _callback;
            #if ESP32
                void *_serviceQuery;
            #else
                MDNSResponder::hMDNSServiceQuery _serviceQuery;
            #endif
            uint16_t _port;
            uint16_t _fallbackPort;
            uint16_t _timeout;
            uint8_t _dataCollected;
            bool _resolved;
            bool _isAddress;
            bool _isPort;
        };


    #endif

}

#endif
