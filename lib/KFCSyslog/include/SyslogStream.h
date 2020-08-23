/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Asynchronous syslog library with queue and different storage types
 *
 * SyslogStream -> SyslogQueue(Memory/File) -> SyslogUDP/TCP/File -> destination server/file
 *
 */

#pragma once

#include <EventScheduler.h>

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
    SyslogStream(Syslog *syslog, Event::Timer &timer);
    virtual ~SyslogStream();

    void setFacility(SyslogFacility facility);
    void setSeverity(SyslogSeverity severity);

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *buffer, size_t len) override;
    virtual void flush() override;

    bool hasQueuedMessages();
    void deliverQueue();
    void clearQueue();
    size_t queueSize() const;
    void dumpQueue(Print &print) const;

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;

    Syslog &getSyslog();

private:
    Syslog &_syslog;
    String _message;
    Event::Timer &_timer;
};
