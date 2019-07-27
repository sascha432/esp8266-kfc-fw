/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define SYSLOG_DEFAULT_PORT -1

class SyslogFactory {
public:
	static Syslog *create(SyslogParameter &parameter, SyslogProtocol protocol, const String &host, uint16_t port = SYSLOG_DEFAULT_PORT);
};
