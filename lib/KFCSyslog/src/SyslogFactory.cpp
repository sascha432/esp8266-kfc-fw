/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogTCP.h"
#include "SyslogUDP.h"
#include "SyslogFactory.h"

Syslog *SyslogFactory::create(SyslogParameter &parameter, SyslogProtocol protocol, const char *host, uint16_t port) {
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
