/**
* Author: sascha_lammers@gmx.de
*/

#include "SyslogFactory.h"
#include "Syslog.h"
#include "SyslogFile.h"
#include "SyslogParameter.h"
#include "SyslogQueue.h"
#include "SyslogTCP.h"
#include "SyslogUDP.h"
#include <Arduino_compat.h>

#if DEBUG_SYSLOG
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

Syslog *SyslogFactory::create(const char *hostname, SyslogQueue *queue, SyslogProtocol protocol, const String &hostOrPath, uint32_t portOrMaxFilesize)
{
    if (protocol == SyslogProtocol::FILE) {
        size_t maxFileSize;
        uint16_t maxRotate;
        if (portOrMaxFilesize == kDefaultValue) {
            maxFileSize = SyslogFile::kMaxFileSize;
            maxRotate = SyslogFile::kKeepRotatedFilesLimit;
        } else {
            maxFileSize = (portOrMaxFilesize & SyslogFile::kMaxFilesizeMask) << 2;
            maxRotate = (uint8_t)(portOrMaxFilesize >> 24);
        }
        return new SyslogFile(hostname, queue, hostOrPath, maxFileSize, maxRotate);
    }
    return create(hostname, queue, protocol, hostOrPath, static_cast<uint16_t>(portOrMaxFilesize));
}

Syslog *SyslogFactory::create(const char *hostname, SyslogQueue *queue, SyslogProtocol protocol, const String &host, uint16_t port)
{
    switch (protocol) {
    case SyslogProtocol::UDP:
        return new SyslogUDP(hostname, queue, host, port == kDefaultPort ? SyslogUDP::kDefaultPort : port);
    case SyslogProtocol::TCP:
        return new SyslogTCP(hostname, queue, host, port == kDefaultPort ? SyslogTCP::kDefaultPort : port, false);
    case SyslogProtocol::TCP_TLS:
        return new SyslogTCP(hostname, queue, host, port == kDefaultPort ? SyslogTCP::kDefaultPortTLS : port, true);
    default:
        break;
    }
    return nullptr;
}
