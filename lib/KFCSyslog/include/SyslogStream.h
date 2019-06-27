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

#define SYSLOG_STREAM_MAX_FAILURES 10

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
	SyslogStream(const SyslogParameter parameter, SyslogProtocol protocol, const char *host, uint16_t port = SYSLOG_DEFAULT_PORT, uint16_t queueSize = 1024);
    SyslogStream(SyslogFilter *filter, SyslogQueue *queue);
    virtual ~SyslogStream();

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

    SyslogQueue *getQueue() {
        return _queue;
    }
    SyslogFilter *getFilter() {
        return _filter;
    }
    SyslogParameter *getParameter() {
        return _parameter;
    }

   private:
    SyslogParameter *_parameter;
    SyslogFilter *_filter;
    SyslogQueue *_queue;
    String _message;
};
