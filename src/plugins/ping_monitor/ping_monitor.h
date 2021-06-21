/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <AsyncPing.h>
#include <web_socket.h>
#include "../src/plugins/mqtt/mqtt_json.h"

#ifndef DEBUG_PING_MONITOR
#define DEBUG_PING_MONITOR                  1
#endif

#define __LDBG_AsyncPingResponse(type, response) \
    __LDBG_printf("on=%u response=%p mac=%p time=%u answer=%u ttl=%u addr=%s", type, &response, response.mac, response.time, response.answer, response.ttl, response.addr.toString().c_str());

using AsyncPingPtr = std::shared_ptr<AsyncPing>;

extern WsClientAsyncWebSocket *wsPing;

namespace PingMonitor {

    using UnnamedObject = MQTT::Json::UnnamedObject;

    bool resolveHost(const String &host, IPAddress &addr, PrintString &errorMessage);
    bool begin(const AsyncPingPtr &ping, const String &host, IPAddress &addr, int count, int timeout, PrintString &message);
    void eventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    inline static void validateValues(int &count, int &timeout)
    {
        using PingConfig = KFCConfigurationClasses::Plugins::PingConfig::PingConfig_t;
        count = std::clamp<int>(count, PingConfig::kTypeMinValueFor_count, PingConfig::kMaxValueFor_count);
        timeout = std::clamp<int>(timeout, PingConfig::kMinValueFor_timeout, PingConfig::kMaxValueFor_timeout);
    }

    class WsPingClient : public WsClient {
    public:
        WsPingClient(AsyncWebSocketClient *client);
        virtual ~WsPingClient();

        static WsClient *getInstance(AsyncWebSocketClient *socket);

        virtual void onText(uint8_t *data, size_t len) override;
        virtual void onDisconnect(uint8_t *data, size_t len) override;
        virtual void onError(WsErrorType type, uint8_t *data, size_t len) override;

        AsyncPingPtr &getPing();

        static void getDefaultDeleter(AsyncPing *pingPtr);
        static void cancelPing(AsyncPing *pingPtr);

    private:
        static void _cancelPing(AsyncPing *pingPtr);
        static void _deletePingPtr(AsyncPing *pingPtr);
        void _cancelPing();

    private:
        AsyncPingPtr _ping;
    };

    inline __attribute__((always_inline)) void WsPingClient::getDefaultDeleter(AsyncPing *pingPtr)
    {
        _deletePingPtr(pingPtr);
    }

    inline __attribute__((always_inline)) void WsPingClient::cancelPing(AsyncPing *pingPtr)
    {
        _cancelPing(pingPtr);
    }

    class Task;

    class Statistics {
    public:
        Statistics();

        void clear();

        uint32_t getAvgResponseTime() const; // milliseconds
        uint32_t getSuccessCount() const;
        uint32_t getFailureCount() const;
        uint32_t getReceivedCount() const;
        uint32_t getLostCount() const;
        float getSuccessRatio() const;
        float getReceivedRatio() const;
        String getRatioString(float value, unsigned char digits = 1) const;

        bool hasData() const;

    private:
        friend Task;

        void addReceived(uint32_t millis);
        void addLost();
        void addSuccess();
        void addFailure();

    private:
        uint32_t _received;
        uint32_t _lost;
        uint32_t _success;
        uint32_t _failure;
        float _averageResponseTime;
    };

    class Host {
    public:
        Host(const String &host);
        // Host(String &&host);

        const String &getHostname() const;
        Statistics &getStats();
        const Statistics &getStats() const;

    private:
        String _host;
        Statistics _stats;
    };

    using TaskPtr = std::shared_ptr<Task>;
    using HostVector = std::vector<Host>;

    class Task {
    public:
        Task(uint16_t interval, uint8_t count, uint16_t timeout);
        ~Task();

        // empty hosts are ignored
        void addHost(String host);

        // start service
        void start();
        // stop service
        void stop();

        // print statistics
        void printStats(Print &out);

        static bool hasStats();
        static Statistics getStats();
        static void addToJson(UnnamedObject &obj);

    private:
        void _addAnswer(bool answer, uint32_t time);
        // ping current host
        void _begin();
        // store results from last ping, select next server and schedule next _begin() call
        // if error is set, the delay is reduced to 10 seconds
        void _next(bool error = false);
        // cancel current ping operation and remove timer
        void _cancelPing();

        Event::Timer _nextTimer;
        HostVector _pingHosts;
        AsyncPingPtr _ping;
        Statistics _stats;
        uint16_t _interval;
        uint16_t _timeout;
        bool _successFlag;
        uint8_t _currentServer;
        uint8_t _count;
    };

    static constexpr auto kTaskSize = sizeof(Task);

}

#include "ping_monitor.hpp"
