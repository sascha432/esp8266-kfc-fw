/**
  Author: sascha_lammers@gmx.de
*/

#include "ping_monitor.h"

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
// class PingHost

namespace PingMonitor {

    inline Host::Host(const String &host) : _host(host)
    {
        __LDBG_printf("host=%s", _host.c_str());
    }

    inline const String &Host::getHostname() const
    {
        return _host;
    }

    inline Statistics &Host::getStats()
    {
        return _stats;
    }

    inline const Statistics &Host::getStats() const
    {
        return _stats;
    }

}

// ------------------------------------------------------------------------
// class Statistics

namespace PingMonitor {

    inline Statistics::Statistics() :
        _received(0),
        _lost(0),
        _success(0),
        _failure(0),
        _averageResponseTime(0)
    {
    }

    inline void Statistics::clear()
    {
        *this = Statistics();
    }

    inline uint32_t Statistics::getAvgResponseTime() const
    {
        return _averageResponseTime;
    }

    inline uint32_t Statistics::getSuccessCount() const
    {
        return _success;
    }

    inline uint32_t Statistics::getFailureCount() const
    {
        return _failure;
    }

    inline uint32_t Statistics::getReceivedCount() const
    {
        return _received;
    }

    inline uint32_t Statistics::getLostCount() const
    {
        return _lost;
    }

    inline void Statistics::addReceived(uint32_t millis)
    {
        _averageResponseTime *= static_cast<float>(_received);
        _averageResponseTime += millis;
        _averageResponseTime /= ++_received;
    }

    inline void Statistics::addLost()
    {
        _lost++;
    }

    inline void Statistics::addSuccess()
    {
        _success++;
    }

    inline void Statistics::addFailure()
    {
        _failure++;
    }

    inline bool Statistics::hasData() const
    {
        return _success || _failure;
    }

    inline float Statistics::getSuccessRatio() const
    {
        auto tmp = _success + _failure;
        if (tmp == 0) {
            return NAN;
        }
        return (_success * 100) / static_cast<float>(tmp);
    }

    inline float Statistics::getReceivedRatio() const
    {
        auto tmp = _received + _lost;
        if (tmp == 0) {
            return NAN;
        }
        return (_received * 100) / static_cast<float>(tmp);
    }

    inline String Statistics::getRatioString(float value, unsigned char digits) const
    {
        if (isnan(value)) {
            return String(FSPGM(n_a));
        }
        return String(value, digits) + '%';
    }

}

// ------------------------------------------------------------------------
// class WsPingClient

namespace PingMonitor {

    inline WsPingClient::WsPingClient(AsyncWebSocketClient *client) : WsClient(client)
    {
    }

    inline WsPingClient::~WsPingClient()
    {
        _cancelPing();
    }

    inline WsClient *WsPingClient::getInstance(AsyncWebSocketClient *socket)
    {
        return new WsPingClient(socket);
    }

    inline AsyncPingPtr &WsPingClient::getPing()
    {
        return _ping;
    }

    inline void WsPingClient::onDisconnect(uint8_t *data, size_t len)
    {
        __LDBG_print("disconnect");
        _cancelPing();
    }

    inline void WsPingClient::onError(WsErrorType type, uint8_t *data, size_t len)
    {
        __LDBG_printf("error type=%u", type);
        _cancelPing();
    }

    inline void WsPingClient::_cancelPing()
    {
        _pingMonitorSafeCancelPing(_ping);
    }

}


#if DEBUG_PING_MONITOR
#include <debug_helper_disable.h>
#endif
