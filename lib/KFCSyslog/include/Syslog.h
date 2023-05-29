/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_SYSLOG
#    define DEBUG_SYSLOG (1 || defined(DEBUG_ALL))
#endif

#include "SyslogParameter.h"
#include "SyslogQueue.h"
#include <Arduino_compat.h>
#include <PrintString.h>
#include <functional>
#include <vector>

#ifndef SYSLOG_USE_RFC5424
#    define SYSLOG_USE_RFC5424 0 // 1 is not working ATM
#endif
#if SYSLOG_USE_RFC5424
#    define SEND_NILVALUE_IF_INVALID_TIMESTAMP 1
#    define SYSLOG_VERSION                     "1"
#    define SYSLOG_TIMESTAMP_FORMAT            "%FT%H:%M:%S."
#    define SYSLOG_TIMESTAMP_FRAC_FMT          "%03u"
#    define SYSLOG_SEND_BOM                    1 // UTF8 BOM
// TCP only
#else
// old/fall-back format
#    define SYSLOG_TIMESTAMP_FORMAT            "%h %d %T "
#    define SYSLOG_VERSION                     "" // not used
#    define SEND_NILVALUE_IF_INVALID_TIMESTAMP 0 // must be 0 for fallback
#endif

#define SYSLOG_FILE_TIMESTAMP_FORMAT "%FT%T%z"

#define SYSLOG_NIL_SP    "- "
#define SYSLOG_NIL_VALUE (SYSLOG_NIL_SP[0])

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

class SyslogQueue;
class SyslogParameter;
class SyslogManagedStream;
class SyslogPlugin;

class Syslog {
public:
    enum class StateType {
        NONE = 0,
        CAN_SEND,               // ready to send messages
        IS_SENDING,             // busy sending
        HAS_CONNECTION,         // connection based?
        CONNECTED,              // is connected?
    };

public:
    Syslog(const Syslog &) = delete;

    Syslog(const char *hostname, SyslogQueue *queue);
    virtual ~Syslog();

    virtual bool setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port) = 0;
    virtual uint32_t getState(StateType type) = 0;

    inline bool canSend() {
        return getState(StateType::CAN_SEND);
    }
    inline bool isSending() {
        return getState(StateType::IS_SENDING);
    }
    inline bool isConnected() {
        return getState(StateType::CONNECTED);
    }
    inline bool hasConnection() {
        return getState(StateType::HAS_CONNECTION);
    }

    virtual void transmit(const SyslogQueueItem &item) = 0;
    virtual String getHostname() const = 0;
    virtual uint16_t getPort() const = 0;

    virtual void clear();

protected:
    friend SyslogStream;
    friend SyslogManagedStream;
    friend SyslogPlugin;

    String _getHeader(uint32_t millis) const;
    void _addTimestamp(PrintString &buffer, uint32_t millis, PGM_P format) const;

    void _addParameter(PrintString &buffer, const char *value) const;
    void _addParameter(PrintString &buffer, const String &value) const;
    void _addParameter(PrintString &buffer, const __FlashStringHelper *value) const;

private:
    template<typename _Ta>
    void __addParameter(PrintString &buffer, _Ta value) const
    {
        size_t len = 0;
        if (value) {
            len = buffer.print(value);
        }
        if (len) {
            buffer.write(' ');
        }
        else {
            buffer.print(F(SYSLOG_NIL_SP));
        }
    }

protected:
    SyslogParameter _parameter;
    SyslogQueue &_queue;
};

inline Syslog::Syslog(const char *hostname, SyslogQueue *queue) :
    _parameter(hostname),
    _queue(*queue)
{
}

inline Syslog::~Syslog()
{
	delete reinterpret_cast<SyslogQueue *>(&_queue);
}

inline void Syslog::clear()
{
    _queue.clear();
}

inline void Syslog::_addParameter(PrintString &buffer, const char *value) const
{
    __addParameter(buffer, value);
}

inline void Syslog::_addParameter(PrintString &buffer, const String &value) const
{
    __addParameter(buffer, value.c_str());
}

inline void Syslog::_addParameter(PrintString &buffer, const __FlashStringHelper *value) const
{
    __addParameter(buffer, value);
}
