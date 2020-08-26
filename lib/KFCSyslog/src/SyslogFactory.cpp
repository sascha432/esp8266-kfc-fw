/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogQueue.h"
#include "SyslogTCP.h"
#include "SyslogUDP.h"
#include "SyslogFile.h"
#include "SyslogFactory.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Syslog *SyslogFactory::create(SyslogParameter &&parameter, SyslogQueue *queue, SyslogProtocol protocol, const String &hostOrPath, uint32_t portOrMaxFilesize)
{
	if (protocol == SyslogProtocol::FILE) {
		size_t maxFileSize;
		uint16_t maxRotate;
		if (portOrMaxFilesize == kDefaultValue) {
			maxFileSize = SyslogFile::kMaxFileSize;
			maxRotate = SyslogFile::kKeepRotatedFilesLimit;
		}
		else {
			maxFileSize = (portOrMaxFilesize & SyslogFile::kMaxFilesizeMask) << 2;
			maxRotate = (uint8_t)(portOrMaxFilesize >> 24);
		}
		return __LDBG_new(SyslogFile, std::move(parameter), *queue, hostOrPath, maxFileSize, maxRotate);
	}
	return create(std::move(parameter), queue, protocol, hostOrPath, static_cast<uint16_t>(portOrMaxFilesize));
}

Syslog *SyslogFactory::create(SyslogParameter &&parameter, SyslogQueue *queue, SyslogProtocol protocol, const String &host, uint16_t port)
{
	switch(protocol) {
		case SyslogProtocol::UDP:
			return __LDBG_new(SyslogUDP, std::move(parameter), *queue, host, port == kDefaultPort ? SyslogUDP::kDefaultPort : port);
		case SyslogProtocol::TCP:
			return __LDBG_new(SyslogTCP, std::move(parameter), *queue, host, port == kDefaultPort ? SyslogTCP::kDefaultPort : port, false);
		case SyslogProtocol::TCP_TLS:
			return __LDBG_new(SyslogTCP, std::move(parameter), *queue, host, port == kDefaultPort ? SyslogTCP::kDefaultPortTLS : port, true);
		default:
			break;
	}
	return nullptr;
}
