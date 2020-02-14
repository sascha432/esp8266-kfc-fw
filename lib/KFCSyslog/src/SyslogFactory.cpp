/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogTCP.h"
#include "SyslogUDP.h"
#include "SyslogFactory.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Syslog *SyslogFactory::create(SyslogParameter &parameter, SyslogProtocol protocol, const String &host, uint16_t port) {
	switch (protocol) {
		case SYSLOG_PROTOCOL_UDP:
			return new SyslogUDP(parameter, host, port == SYSLOG_DEFAULT_PORT ? SYSLOG_PORT_UDP : port);
		case SYSLOG_PROTOCOL_TCP:
			return new SyslogTCP(parameter, host, port == SYSLOG_DEFAULT_PORT ? SYSLOG_PORT_TCP : port, false);
		case SYSLOG_PROTOCOL_TCP_TLS:
			return new SyslogTCP(parameter, host, port == SYSLOG_DEFAULT_PORT ? SYSLOG_PORT_TCP_TLS : port, true);
		case SYSLOG_PROTOCOL_NONE:
		case SYSLOG_PROTOCOL_FILE:
			break;
	}
	return nullptr;
}
