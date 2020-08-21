/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class SyslogFactory {
public:
	static constexpr uint16_t kDefaultPort = ~0;

	static Syslog *create(SyslogParameter &&parameter, SyslogQueue *queue, SyslogProtocol protocol, const String &host, uint16_t port = kDefaultPort);
};
