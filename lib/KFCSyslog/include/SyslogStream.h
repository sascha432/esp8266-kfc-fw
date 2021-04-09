/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Asynchronous syslog library with queue and different storage types
 *
 * SyslogStream -> Syslog(Memory)Queue -> Syslog(UDP/TCP/File) -> destination server/file
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

class SyslogPlugin;

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
    void dumpQueue(Print &print, bool items) const;

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;

protected:
    friend SyslogPlugin;

    Syslog &_syslog;
    String _message;
    Event::Timer &_timer;
};

inline SyslogStream::SyslogStream(Syslog *syslog, Event::Timer &timer) :
    _syslog(*syslog),
    _timer(timer)
{
}

inline SyslogStream::~SyslogStream()
{
    delete &_syslog;
}

inline void SyslogStream::setFacility(SyslogFacility facility)
{
    _syslog._parameter.setFacility(facility);
}

inline void SyslogStream::setSeverity(SyslogSeverity severity)
{
    _syslog._parameter.setSeverity(severity);
}

inline size_t SyslogStream::write(uint8_t data)
{
    _message += (char)data;
    if (data == '\n') {
        flush();
    }
    return 1;
}

inline int SyslogStream::read()
{
    return -1;
}

inline int SyslogStream::peek()
{
    return -1;
}

inline void SyslogStream::clearQueue()
{
    _syslog.clear();
}

inline size_t SyslogStream::queueSize() const
{
    return _syslog._queue.size();
}

inline bool SyslogStream::hasQueuedMessages()
{
    return _syslog._queue.size() != 0;
}

inline int SyslogStream::available()
{
    return false;
}
