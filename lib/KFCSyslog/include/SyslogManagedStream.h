/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "Syslog.h"
#include "SyslogStream.h"
#include "SyslogQueue.h"

// adds a timer and queue manager to the stream
class SyslogManagedStream : public SyslogStream, public SyslogQueueManager {
public:
    static constexpr auto kQueuePollInterval = Event::milliseconds(125);

public:
    SyslogManagedStream(PGM_P hostname, const __FlashStringHelper *appName, SyslogQueue *queue, const String &syslogHostOrPath, uint32_t portOrMaxFilesize, SyslogProtocolType protocol = SyslogProtocolType::TCP, Event::milliseconds interval = kQueuePollInterval);
    SyslogManagedStream(Syslog *syslog, Event::milliseconds interval = kQueuePollInterval);
    ~SyslogManagedStream();

    virtual void queueSize(uint32_t size, bool isAvailable);

private:
    void timerCallback(Event::CallbackTimerPtr timer);

    Event::milliseconds _interval;
    Event::Timer _timer;
};
