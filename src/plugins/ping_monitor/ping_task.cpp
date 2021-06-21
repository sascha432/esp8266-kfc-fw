/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#include <kfc_fw_config.h>
#include "ping_monitor.h"

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

// ------------------------------------------------------------------------
// class Task

namespace PingMonitor {

    Task::Task(uint16_t interval, uint8_t count, uint16_t timeout) :
        _interval(interval),
        _timeout(timeout),
        _successFlag(false),
        _currentServer(0),
        _count(count)
    {
        __LDBG_printf("interval=%u count=%u timeout=%u cur_server=%u", _interval, _count, _timeout, _currentServer);
    }

    Task::~Task()
    {
        __LDBG_printf("stopping task");
        stop();
    }

    void Task::addHost(String host)
    {
        __LDBG_printf("host=%s", host.c_str());
        if (host.trim().length()) {
            _pingHosts.emplace_back(std::move(host));
        }
    }

    void Task::_addAnswer(bool answer, uint32_t time)
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

    void Task::_next(bool error)
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

    void Task::_begin()
    {
        __LDBG_assert_printf(_currentServer < _pingHosts.size(), "server %u size %u", _currentServer, _pingHosts.size());
        String host = _pingHosts[_currentServer].getHostname();

        host.replace(FSPGM(_var_gateway), WiFi.isConnected() ? WiFi.gatewayIP().toString() : emptyString);
        host.trim();

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

    void Task::printStats(Print &out)
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

    void Task::addToJson(UnnamedObject &json)
    {
        if (hasStats()) {
            using namespace MQTT::Json;
            auto stats = getStats();
            json.append(
                NamedUint32(F("avg_resp_time"), stats.getAvgResponseTime()),
                NamedUint32(F("rcvd_pkts"), stats.getReceivedCount()),
                NamedUint32(F("lost_pkts"), stats.getLostCount()),
                NamedUint32(F("success"), stats.getSuccessCount()),
                NamedUint32(F("failure"), stats.getFailureCount())
            );
        }
    }

    void Task::start()
    {
        __LDBG_printf("hosts=%u", _pingHosts.size());
        _cancelPing();
        if (_pingHosts.size()) {
            _currentServer = 0;
            _ping.reset(new AsyncPing(), WsPingClient::getDefaultDeleter);
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
        }
        else {
            __LDBG_printf("no hosts");
        }
    }

    void Task::stop()
    {
        __LDBG_printf("hosts=%u", _pingHosts.size());
        _cancelPing();
        _pingHosts.clear();
        Logger_notice(FSPGM(ping_monitor_service_status), FSPGM(stopped));
    }

    void Task::_cancelPing()
    {
        __LDBG_printf("ping=%p timer=%u", _ping.get(), (bool)_nextTimer);
        _nextTimer.remove();
        _ping.reset();
    }

}
