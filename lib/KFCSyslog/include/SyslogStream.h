/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Asynchronous syslog library with queue and different storage types
 *
 * SyslogStream -> SyslogFilter -> SyslogQueue(Memory/File) -> SyslogUDP/TCP/File
 *
 *
 */

#pragma once

#include <Arduino_compat.h>
#include "Syslog.h"
#include "SyslogFilter.h"
#include "SyslogQueue.h"

#define Syslog_log(syslog, severity, format, ...) \
    {                                               \
        syslog.setSeverity(severity);               \
        syslog.printf(format, __VA_ARGS__); \
        syslog.flush();                             \
    }
#define Syslog_log_P(syslog, severity, format, ...) \
    {                                               \
        syslog.setSeverity(severity);               \
        syslog.printf_P(format, __VA_ARGS__); \
        syslog.flush();                             \
    }

class SyslogStream : public Stream {
   public:
    const uint8_t MAX_FAILURES = 10;

    SyslogStream(SyslogFilter *filter, SyslogQueue *queue);
    ~SyslogStream();

    void setFacility(SyslogFacility facility);
    void setSeverity(SyslogSeverity severity);

    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buffer, size_t len) override;
    void flush() override;

    bool hasQueuedMessages();

    int available() override;
    int read() override;
    int peek() override;

    void deliverQueue(Syslog *syslog = nullptr);

    void transmitCallback(SyslogMemoryQueueItem *item, bool success);

    const String getLevel();

   private:
    SyslogParameter &_parameter;
    SyslogFilter *_filter;
    SyslogQueue *_queue;
    String _message;
};
