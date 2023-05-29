/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "Syslog.h"
#include "SyslogStream.h"
#include "SyslogQueue.h"
#include "SyslogFactory.h"

// adds a timer and queue manager to the stream
class SyslogManagedStream : public SyslogStream, public SyslogQueueManager {
public:
    static constexpr auto kQueuePollInterval = Event::milliseconds(125);

public:
    SyslogManagedStream(const char *hostname, SyslogQueue *queue, const String &syslogHostOrPath, uint32_t portOrMaxFilesize, SyslogProtocolType protocol = SyslogProtocolType::TCP, Event::milliseconds interval = kQueuePollInterval);
    SyslogManagedStream(Syslog *syslog, Event::milliseconds interval = kQueuePollInterval);
    ~SyslogManagedStream();

    virtual void queueSize(uint32_t size, bool isAvailable);

private:
    void timerCallback();

    Event::milliseconds _interval;
    Event::Timer _timer;
};

inline SyslogManagedStream::SyslogManagedStream(const char *hostname, SyslogQueue *queue, const String &syslogHostorPath, uint32_t portOrMaxFileSize, SyslogProtocolType protocol, Event::milliseconds interval) :
    SyslogManagedStream(SyslogFactory::create(hostname, queue, protocol, syslogHostorPath, portOrMaxFileSize), interval)
{
}

inline SyslogManagedStream::SyslogManagedStream(Syslog *syslog, Event::milliseconds interval) :
    SyslogStream(syslog, _timer),
    _interval(interval)
{
    _syslog._queue.setManager(*this);
}

inline SyslogManagedStream::~SyslogManagedStream()
{
    delete reinterpret_cast<Syslog *>(&_syslog);
}

inline void SyslogManagedStream::timerCallback()
{
    if (hasQueuedMessages()) {
        deliverQueue();
    }
}

inline void SyslogManagedStream::queueSize(uint32_t size, bool isAvailable)
{
    if (size == 0) {
        _Timer(_timer).remove();
    }
    else if (isAvailable && !_timer) {
        _Timer(_timer).add(_interval, true, [this](Event::CallbackTimerPtr timer) {
            timerCallback();
        });
    }
}

