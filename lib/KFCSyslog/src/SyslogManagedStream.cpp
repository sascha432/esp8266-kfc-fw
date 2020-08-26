/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogFactory.h"
#include "SyslogManagedStream.h"

SyslogManagedStream::SyslogManagedStream(PGM_P hostname, const __FlashStringHelper *appName, SyslogQueue *queue, const String &syslogHostorPath, uint32_t portOrMaxFileSize, SyslogProtocolType protocol, Event::milliseconds interval) :
    SyslogManagedStream(SyslogFactory::create(SyslogParameter(hostname, appName), queue, protocol, syslogHostorPath, portOrMaxFileSize), interval)
{
}

SyslogManagedStream::SyslogManagedStream(Syslog *syslog, Event::milliseconds interval) :
    SyslogStream(syslog, _timer),
    _interval(interval)
{
    _syslog._queue.setManager(*this);
}

SyslogManagedStream::~SyslogManagedStream()
{
    __LDBG_delete(reinterpret_cast<Syslog *>(&_syslog));
}

void SyslogManagedStream::queueSize(uint32_t size, bool isAvailable)
{
    __LDBG_printf("size=%u available=%u timer=%u", size, isAvailable, (bool)_timer);
    if (size == 0) {
        __LDBG_print("remove timer");
        _timer.remove();
    }
    else if (isAvailable && !_timer) {
        __LDBG_print("add timer");
        _Timer(_timer).add(_interval, true, [this](Event::CallbackTimerPtr timer) {
            timerCallback(timer);
        });
    }
}

void SyslogManagedStream::timerCallback(Event::CallbackTimerPtr timer)
{
    if (hasQueuedMessages()) {
        deliverQueue();
    }
}
