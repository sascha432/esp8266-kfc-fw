/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogFactory.h"
#include "SyslogQueue.h"
#include "SyslogStream.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogStream::SyslogStream(Syslog *syslog, Event::Timer &timer) : _syslog(*syslog), _timer(timer)
{
    assert(&_syslog != nullptr);
}

SyslogStream::~SyslogStream()
{
    __LDBG_delete(&_syslog);
}

void SyslogStream::setFacility(SyslogFacility facility)
{
    _syslog._parameter.setFacility(facility);
}

void SyslogStream::setSeverity(SyslogSeverity severity)
{
    _syslog._parameter.setSeverity(severity);
}

size_t SyslogStream::write(uint8_t data)
{
    _message += (char)data;
    if (data == '\n') {
        flush();
    }
    return 1;
}

size_t SyslogStream::write(const uint8_t *buffer, size_t len)
{
    auto ptr = buffer;
    while (len--) {
        char ch = *ptr++;
        if (ch == '\n') {
            flush();
        } else {
            _message += (char)ch;
        }
    }
    return ptr - buffer;
}

void SyslogStream::flush()
{
    if (_message.length()) {
        _syslog._queue.add(_message);
        _message = String();
        deliverQueue();
        if (!_syslog._queue.empty() && _timer) {
            _timer->rearm(Event::milliseconds(100));
        }
    }
}

bool SyslogStream::hasQueuedMessages()
{
    return _syslog._queue.size() != 0;
}

int SyslogStream::available()
{
    return false;
}

int SyslogStream::read()
{
    return -1;
}

int SyslogStream::peek()
{
    return -1;
}

void SyslogStream::deliverQueue()
{
    _startMillis = _timeout + millis();
    while(millis() < _startMillis && !_syslog.isSending() && _syslog._queue.canSend()) {
        _syslog.transmit(_syslog._queue.get());
	}
    if (_syslog._queue.empty() && _timer) {
        _timer->rearm(Event::seconds(60));
    }
}

void SyslogStream::clearQueue()
{
    _syslog.clear();
}

size_t SyslogStream::queueSize() const
{
    return _syslog._queue.size();
}

void SyslogStream::dumpQueue(Print &print) const
{
    return _syslog._queue.dump(print);
}

Syslog &SyslogStream::getSyslog()
{
    return _syslog;
}

// String SyslogStream::getLevel() const
// {
// 	String result;

// 	auto facility = _parameter.getFacility();
//     for (uint8_t i = 0; syslogFilterFacilityItems[i].value != 0xff; i++) {
//         if (syslogFilterFacilityItems[i].value == facility) {
//             result += syslogFilterFacilityItems[i].name;
//             break;
//         }
//     }
// 	result += '.';
// 	auto severity = _parameter.getSeverity();
// 	for(uint8_t i = 0; syslogFilterSeverityItems[i].value != 0xff; i++) {
// 		if (syslogFilterSeverityItems[i].value == severity) {
// 			result += syslogFilterSeverityItems[i].name;
// 			break;
// 		}
// 	}

// 	return result;
// }
