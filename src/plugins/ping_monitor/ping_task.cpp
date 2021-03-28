/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#include <kfc_fw_config.h>
// #include <web_server.h>
#include "ping_monitor.h"

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

// ------------------------------------------------------------------------
// class PingHost

PingMonitorTask::PingHost::PingHost(const String &host) : _host(host)
{
    __LDBG_printf("host=%s", _host.c_str());
}

String PingMonitorTask::PingHost::getHostname() const
{
    return _host;
}

PingMonitorTask::PingStatistics &PingMonitorTask::PingHost::getStats()
{
    return _stats;
}

const PingMonitorTask::PingStatistics &PingMonitorTask::PingHost::getStats() const
{
    return _stats;
}

// ------------------------------------------------------------------------
// class PingStatistics

PingMonitorTask::PingStatistics::PingStatistics() : _received(0), _lost(0), _success(0), _failure(0), _averageResponseTime(0)
{
}

void PingMonitorTask::PingStatistics::clear()
{
    *this = PingStatistics();
}

uint32_t PingMonitorTask::PingStatistics::getAvgResponseTime() const
{
    return _averageResponseTime;
}


uint32_t PingMonitorTask::PingStatistics::getSuccessCount() const
{
    return _success;
}

uint32_t PingMonitorTask::PingStatistics::getFailureCount() const
{
    return _failure;
}

uint32_t PingMonitorTask::PingStatistics::getReceivedCount() const
{
    return _received;
}

uint32_t PingMonitorTask::PingStatistics::getLostCount() const
{
    return _lost;
}

void PingMonitorTask::PingStatistics::addReceived(uint32_t millis)
{
    // we need double precision before storing the result
    auto tmp = (_averageResponseTime * (double)_received) + millis;
    _averageResponseTime = tmp / ++_received;
}

void PingMonitorTask::PingStatistics::addLost()
{
    _lost++;
}

void PingMonitorTask::PingStatistics::addSuccess()
{
    _success++;
}

void PingMonitorTask::PingStatistics::addFailure()
{
    _failure++;
}

bool PingMonitorTask::PingStatistics::hasData() const
{
    return _success || _failure;
}

float PingMonitorTask::PingStatistics::getSuccessRatio() const
{
    if ((_success + _failure) == 0) {
        return NAN;
    }
    return (_success * 100.0) / (_success + _failure);
}

float PingMonitorTask::PingStatistics::getReceivedRatio() const
{
    if ((_received + _lost) == 0) {
        return NAN;
    }
    return (_received * 100.0) / (_received + _lost);
}

String PingMonitorTask::PingStatistics::getRatioString(float value, unsigned char digits) const
{
    if (isnan(value)) {
        return String(FSPGM(n_a));
    }
    return String(value, digits) + '%';
}

// ------------------------------------------------------------------------
// class PingMonitorTask

PingMonitorTask::PingMonitorTask(uint16_t interval, uint8_t count, uint16_t timeout) : _count(count), _successFlag(0), _interval(interval), _timeout(timeout)
{
    __LDBG_printf("interval=%u count=%u timeout=%u", interval, count, timeout);
}

PingMonitorTask::~PingMonitorTask()
{
    __LDBG_println();
    stop();
}

void PingMonitorTask::addHost(String host)
{
    __LDBG_printf("host=%s", host.c_str());
    host.trim();
    if (host.length()) {
        _pingHosts.emplace_back(host);
    }
}

void PingMonitorTask::_addAnswer(bool answer, uint32_t time)
{
    auto &host = _pingHosts.at(_currentServer);
    __LDBG_printf("answer=%d server=%d host=%s", answer, _currentServer, host.getHostname().c_str());
    if (answer) {
        _stats.addReceived(time);
        host.getStats().addReceived(time);
        _successFlag = true;
    } else {
        _stats.addLost();
        host.getStats().addLost();
    }
}

void PingMonitorTask::_next(bool error)
{
    // add results from last ping
    auto &host = _pingHosts.at(_currentServer);
    if (_successFlag) {
        _stats.addSuccess();
        host.getStats().addSuccess();
    }
    else {
        _stats.addFailure();
        host.getStats().addFailure();
    }
    // select next server
    _currentServer = (_currentServer + 1) % _pingHosts.size();
    _successFlag = false;
    uint32_t interval = error ? 10000U : _interval * 60000U;

    _Timer(_nextTimer).add(interval, false, [this](Event::CallbackTimerPtr timer) {
        _begin();
    });
    __LDBG_printf("server=%d error=%u interval=%u", _currentServer, error, interval);
}

void PingMonitorTask::_begin()
{
    String host = _pingHosts[_currentServer].getHostname();
    _pingMonitorResolveHostVariables(host);
    _successFlag = false;
    __LDBG_printf("host=%s (%s)", host.c_str(), host.c_str());
    if (host.length() == 0) {
        __LDBG_printf("skipping empty host");
        _next(true);
    }
    else {
        _ping->cancel();

        if (!_ping->begin(host.c_str(), _count, _timeout)) {
            Logger_notice(FSPGM(ping_monitor_ping_for_hostname_failed), host.c_str());
            _next(true);
        }
    }
}

void PingMonitorTask::printStats(Print &out)
{
    out.printf_P(PSTR("Interval %d minute(s), timeout %dms, repeat ping %d" HTML_S(br) HTML_S(hr)), _interval, _timeout, _count);
    for(const auto &host: _pingHosts) {
        auto stats = host.getStats();
        out.print(host.getHostname());
        out.printf_P(PSTR(": %u (%s) packets received, %u lost, avg. response %ums, %u (%s) ping succeded, %u failed" HTML_S(br)),
            stats.getReceivedCount(),
            stats.getRatioString(stats.getReceivedRatio()).c_str(),
            stats.getLostCount(),
            stats.getAvgResponseTime(),
            stats.getSuccessCount(),
            stats.getRatioString(stats.getSuccessRatio()).c_str(),
            stats.getFailureCount()
        );
    }
    if (hasStats()) {
        auto stats = getStats();
        out.printf_P(PSTR(HTML_S(hr) "Average response time: %ums" HTML_S(br) "Received packets: %u (%s)" HTML_S(br) "Lost packets: %u" HTML_S(br) "Success: %u (%s)" HTML_S(br) "Failure: %u"),
            stats.getAvgResponseTime(),
            stats.getReceivedCount(),
            stats.getRatioString(stats.getReceivedRatio()).c_str(),
            stats.getLostCount(),
            stats.getSuccessCount(),
            stats.getRatioString(stats.getSuccessRatio()).c_str(),
            stats.getFailureCount()
        );
    }
}

void PingMonitorTask::addToJson(MQTT::Json::UnnamedObjectWriter &json)
{
    using namespace MQTT::Json;

    if (hasStats()) {
        auto stats = getStats();

        json.append(NamedUint32(F("avg_resp_time"), stats.getAvgResponseTime()));
        json.append(NamedUint32(F("rcvd_pkts"), stats.getReceivedCount()));
        json.append(NamedUint32(F("lost_pkts"), stats.getLostCount()));
        json.append(NamedUint32(F("success"), stats.getSuccessCount()));
        json.append(NamedUint32(F("failure"), stats.getFailureCount()));

        // auto &json = obj.addObject(FSPGM(ping_monitor));
        // json.add(F("avg_resp_time"), stats.getAvgResponseTime());
        // json.add(F("rcvd_pkts"), stats.getReceivedCount());
        // json.add(F("lost_pkts"), stats.getLostCount());
        // json.add(F("success"), stats.getSuccessCount());
        // json.add(F("failure"), stats.getFailureCount());
    }
}

void PingMonitorTask::start()
{
    __LDBG_printf("hosts=%u", _pingHosts.size());
    _cancelPing();
    if (_pingHosts.size()) {
        _currentServer = 0;
        _ping.reset(new AsyncPing());
        // response callback
        _ping->on(true, [this](const AsyncPingResponse &response) {
            __LDBG_AsyncPingResponse(true, response);
            _addAnswer(response.answer, response.time);
            return false; // return false to continue, true to cancel
        });
        // ping done callback
        _ping->on(false, [this](const AsyncPingResponse &response) {
            __LDBG_AsyncPingResponse(false, response);
            _next();
            return true;
        });

        if (WiFi.isConnected()) {
            __LDBG_print("wifi already connected");
            _begin();
        }
        else {
            __LDBG_print("waiting for wifi connection");
            WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType events, void *payload) {
                WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, this);
                _begin();
            }, this);
        }

        Logger_notice(FSPGM(ping_monitor_service_status), FSPGM(started));
    } else {
        __LDBG_printf("no hosts");
    }
}

void PingMonitorTask::stop()
{
    __LDBG_printf("hosts=%u", _pingHosts.size());
    _cancelPing();
    _pingHosts.clear();
    Logger_notice(FSPGM(ping_monitor_service_status), FSPGM(stopped));
}

void PingMonitorTask::_cancelPing()
{
    __LDBG_printf("ping=%p timer=%u", _ping.get(), _nextTimer.active());
    _nextTimer.remove();
    _pingMonitorSafeDeletePing(_ping);
}
