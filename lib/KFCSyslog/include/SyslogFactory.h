/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Syslog.h"

class SyslogFactory {
public:
	static constexpr uint32_t kDefaultValue = ~0;
	static constexpr uint16_t kDefaultPort = ~0;
	static constexpr uint16_t kZeroconfPort = 0;

	static Syslog *create(SyslogParameter &&parameter, SyslogQueue *queue, SyslogProtocol protocol, const String &host, uint16_t port = kDefaultPort);
	static Syslog *create(SyslogParameter &&parameter, SyslogQueue *queue, SyslogProtocol protocol, const String &hostOrPath, uint32_t portOrMaxFilesize = kDefaultValue);
};
