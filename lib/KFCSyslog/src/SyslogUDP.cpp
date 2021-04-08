/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogUDP.h"
#include "SyslogQueue.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogUDP::SyslogUDP(const char *hostname, SyslogQueue *queue, const String &host, uint16_t port) :
    Syslog(hostname, queue),
    _host(nullptr),
    _port(port)
{
    if (host.length() && !_address.fromString(host)) {
        _host = strdup(host.c_str()); // resolve hostname later
    }
    __LDBG_printf("%s:%d address=%s", host.c_str(), port, _address.toString().c_str());
}

bool SyslogUDP::setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port)
{
    if (_host) {
        free(_host);
        _host = nullptr;
    }
    if (IPAddress_isValid(address)) {
        _address = address;
    }
    else if (hostname.length()) {
        _host = strdup(hostname.c_str());
    }
    _port = port;
    return true;
}

void SyslogUDP::transmit(const SyslogQueueItem &item)
{
    auto &message = item.getMessage();
    __LDBG_printf("id=%u msg=%s%s", item.getId(), _getHeader(item.getMillis()).c_str(), message.c_str());

    _queue.remove(item.getId(), [&]() {
        if (!WiFi.isConnected()) {
            return false;
        }
        if (_host) {
            if (WiFi.hostByName(_host, _address) && IPAddress_isValid(_address)) { // try to resolve host and store its IP
                free(_host);
                _host = nullptr;
            }
            else {
                // failed to resolve _host
                return false;
            }
        }
        if (!_udp.beginPacket(_address, _port)) {
            _udp.stop();
            return false;
        }
        {
            String header = _getHeader(item.getMillis());
            if (_udp.write((const uint8_t*)header.c_str(), header.length()) != header.length()) {
                _udp.stop();
                return false;
            }
        }
        if ((_udp.write((const uint8_t*)message.c_str(), message.length()) == message.length()) && _udp.endPacket()) {
            return true;
        }
        _udp.stop();
        return false;
    }());
}
