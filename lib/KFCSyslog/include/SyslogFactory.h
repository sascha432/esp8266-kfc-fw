/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class SyslogFactory {
public:
	static constexpr uint16_t kDefaultPort = ~0;

	static Syslog *create(SyslogParameter &parameter, SyslogProtocol protocol, const String &host, uint16_t port = kDefaultPort);
};
