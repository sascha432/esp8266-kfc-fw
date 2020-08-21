/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogQueue.h"
#include "SyslogTCP.h"
#include "SyslogUDP.h"
#include "SyslogFactory.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Syslog *SyslogFactory::create(SyslogParameter &&parameter, SyslogQueue *queue, SyslogProtocol protocol, const String &host, uint16_t port)
{
	switch(protocol) {
		case SyslogProtocol::UDP:
			return __LDBG_new(SyslogUDP, std::move(parameter), *queue, host, port == kDefaultPort ? SyslogUDP::kDefaultPort : port);
		case SyslogProtocol::TCP:
			return __LDBG_new(SyslogTCP, std::move(parameter), *queue, host, port == kDefaultPort ? SyslogTCP::kDefaultPort : port, false);
		case SyslogProtocol::TCP_TLS:
			return __LDBG_new(SyslogTCP, std::move(parameter), *queue, host, port == kDefaultPort ? SyslogTCP::kDefaultPortTLS : port, true);
		case SyslogProtocol::NONE:
		case SyslogProtocol::FILE:
		default:
			break;
	}
	return nullptr;
}
