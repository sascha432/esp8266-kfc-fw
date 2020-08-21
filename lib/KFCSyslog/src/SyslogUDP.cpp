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

SyslogUDP::SyslogUDP(SyslogParameter &&parameter, SyslogQueue &queue, const String &host, uint16_t port) :
    Syslog(std::move(parameter), queue),
    _host(nullptr),
    _port(port)
{
    if (!_address.fromString(host)) {
        _host = strdup(host.c_str()); // resolve hostname later
    }
    __LDBG_printf("%s:%d address=%s", host.c_str(), port, _address.toString().c_str());
}

SyslogUDP::~SyslogUDP()
{
    if (_host) {
        free(_host);
    }
}

void SyslogUDP::transmit(const SyslogQueueItem &item)
{
    auto &message = item.getMessage();

#if DEBUG_SYSLOG
    if (!String_startsWith(message, F("::transmit '"))) {
        __LDBG_printf("::transmit id=%u msg=%s%s", item.getId(), _getHeader().c_str(), message.c_str());
    }
#endif

    bool success = false;
    while(true) {
        if (!WiFi.isConnected()) {
            break;
        }
        if (_host) {
            if (WiFi.hostByName(_host, _address) && _address.isSet() && _address != INADDR_NONE) { // try to resolve host and store its IP
                free(_host);
                _host = nullptr;
            }
            else {
                break;
            }
        }

        success = _udp.beginPacket(_address, _port);
        if (success) {
            {
                String header = _getHeader();
                success = _udp.write((const uint8_t*)header.c_str(), header.length()) == header.length();
            }
            if (success) {
                success =
                    _udp.write((const uint8_t*)message.c_str(), message.length()) == message.length() &&
                    _udp.endPacket();
            }
        }
        break;
    }
    _queue.remove(item.getId(), success);
}
