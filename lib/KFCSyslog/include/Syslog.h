/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_SYSLOG
#define DEBUG_SYSLOG                            1
#endif

#include <Arduino_compat.h>
#include <PrintString.h>
#include <functional>
#include <vector>
#include "SyslogParameter.h"
#include "SyslogQueue.h"

#ifndef SYSLOG_USE_RFC5424
#define SYSLOG_USE_RFC5424                      0                           // 1 is not working ATM
#endif
#if SYSLOG_USE_RFC5424
#define SEND_NILVALUE_IF_INVALID_TIMESTAMP      1
#define SYSLOG_VERSION                          "1"
#define SYSLOG_TIMESTAMP_FORMAT                 "%FT%H:%M:%S."
#define SYSLOG_TIMESTAMP_FRAC_FMT               "%03u"
#define SYSLOG_TIMESTAMP_FRAC_FUNC              (millis() % 1000)
// #define SYSLOG_TIMESTAMP_FRAC_FMT               "%06u"
// #define SYSLOG_TIMESTAMP_FRAC_FUNC              (micros() % 1000000)
#define SYSLOG_SEND_BOM                         1                           // UTF8 BOM
// TCP only
#else
// old/fall-back format
#define SYSLOG_TIMESTAMP_FORMAT                 "%h %d %T "
#define SYSLOG_VERSION                          ""                          // not used
#define SEND_NILVALUE_IF_INVALID_TIMESTAMP      0                           // must be 0 for fallback
#endif

#define SYSLOG_FILE_TIMESTAMP_FORMAT            "%FT%T%z"

class Syslog;
class SyslogStream;

#if defined(HAVE_KFC_FIRMWARE_VERSION) && HAVE_KFC_FIRMWARE_VERSION

#include "kfc_fw_config_types.h"

using SyslogProtocol = SyslogProtocolType;

#else

enum class SyslogProtocol {
    MIN = 0,
    NONE = MIN,
    UDP,
    TCP,
    TCP_TLS,
    FILE,
    MAX
};

#endif

#if 0
// facility/severity name/value pairs
typedef struct {
    PGM_P name;
    uint8_t value;
} SyslogFilterItemPair;

extern const SyslogFilterItemPair syslogFilterFacilityItems[] PROGMEM;
extern const SyslogFilterItemPair syslogFilterSeverityItems[] PROGMEM;

#endif

PROGMEM_STRING_DECL(syslog_nil_value);

class SyslogQueue;
class SyslogParameter;

class Syslog {
public:
    Syslog(const Syslog &) = delete;

    Syslog(SyslogParameter &&parameter, SyslogQueue &queue);
    virtual ~Syslog();

    virtual bool setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port) = 0;
    virtual bool canSend() const = 0;
	virtual bool isSending() = 0;
    virtual void transmit(const SyslogQueueItem &item) = 0;
    virtual String getHostname() const = 0;
    virtual uint16_t getPort() const = 0;

    virtual void clear();

protected:
    friend SyslogStream;

    String _getHeader();

    void _addTimestamp(PrintString &buffer, PGM_P format);
    void _addParameter(PrintString &buffer, PGM_P value);

    SyslogParameter _parameter;
    SyslogQueue &_queue;
};
