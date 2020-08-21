/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Asynchronous syslog library with queue and different storage types
 *
 * SyslogStream -> SyslogFilter -> SyslogQueue(Memory/File) -> SyslogUDP/TCP/File -> destination server -> callback to SyslogQueue -> callback to SyslogStream
 *
 *
 */

#pragma once

#define SYSLOG_STREAM_MAX_FAILURES                      10
#if SYSLOG_STREAM_MAX_FAILURES >= 0x7e
#error SyslogQueueItem._failureCount is 7bit only
#endif

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
    SyslogStream(Syslog *syslog);
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

    // void transmitCallback(const SyslogQueue::ItemPtr &item, bool success);

    // String getLevel() const;

    // SyslogQueue &getQueue() {
    //     return _queue;
    // }
    // void setQueue(SyslogQueue *queue) {
    //     _queue = queue;
    // }
    // SyslogFilter *getFilter() {
    //     return _filter;
    // }
    // const SyslogParameter &getParameter() const {
    //     return _syslog._parameter;
    // }

private:
    // SyslogParameter &_parameter;
    // SyslogFilter *_filter;
    Syslog &_syslog;
    String _message;
};
