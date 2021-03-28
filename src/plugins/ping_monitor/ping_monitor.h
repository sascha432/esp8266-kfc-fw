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
#define DEBUG_PING_MONITOR                  0
#endif

#define __LDBG_AsyncPingResponse(type, response) \
    __LDBG_printf("on=%u response=%p mac=%p time=%u answer=%u ttl=%u addr=%s", type, &response, response.mac, response.time, response.answer, response.ttl, response.addr.toString().c_str());

using AsyncPingPtr = std::shared_ptr<AsyncPing>;

void _pingMonitorValidateValues(int &count, int &timeout);
void _pingMonitorSafeCancelPing(AsyncPingPtr &ping);
void _pingMonitorSafeDeletePing(AsyncPingPtr &ping);
void _pingMonitorSetupWebHandler();
bool _pingMonitorResolveHost(const String &host, IPAddress &addr, PrintString &errorMessage);
bool _pingMonitorBegin(const AsyncPingPtr &ping, const String &host, IPAddress &addr, int count, int timeout, PrintString &message);
void _pingMonitorResolveHostVariables(String &host);
void _pingMonitorShutdownWebSocket();

class WsPingClient : public WsClient {
public:
    WsPingClient(AsyncWebSocketClient *client);
    virtual ~WsPingClient();

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onText(uint8_t *data, size_t len) override;
    virtual void onDisconnect(uint8_t *data, size_t len) override;
    virtual void onError(WsErrorType type, uint8_t *data, size_t len) override;

    AsyncPingPtr &getPing();

private:
    void _cancelPing();
    AsyncPingPtr _ping;
};

class PingMonitorTask {
public:
    using PingMonitorTaskPtr = std::shared_ptr<PingMonitorTask>;

    class PingStatistics {
    public:
        PingStatistics();

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
        friend PingMonitorTask;

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

    class PingHost {
    public:
        PingHost(const String &host);

        String getHostname() const;
        PingStatistics &getStats();
        const PingStatistics &getStats() const;

    private:
        String _host;
        PingStatistics _stats;
    };


    typedef std::vector<PingHost> PingVector;

    PingMonitorTask(uint16_t interval, uint8_t count, uint16_t timeout);
    ~PingMonitorTask();

    // empty hosts are ignored
    void addHost(String host);

    // start service
    void start();
    // stop service
    void stop();

    // print statistics
    void printStats(Print &out);

    static bool hasStats();
    static PingStatistics getStats();
    static void addToJson(MQTT::Json::UnnamedObjectWriter &obj);

private:
    void _addAnswer(bool answer, uint32_t time);
    // ping current host
    void _begin();
    // store results from last ping, select next server and schedule next _begin() call
    // if error is set, the delay is reduced to 10 seconds
    void _next(bool error = false);
    // cancel current ping operation and remove timer
    void _cancelPing();

    uint8_t _currentServer;
    uint8_t _count : 7;
    uint8_t _successFlag : 1;
    uint16_t _interval;
    uint16_t _timeout;
    Event::Timer _nextTimer;
    PingVector _pingHosts;
    AsyncPingPtr _ping;
    PingStatistics _stats;
};
