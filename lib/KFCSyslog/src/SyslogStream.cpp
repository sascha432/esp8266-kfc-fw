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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
// SyslogStream
// ------------------------------------------------------------------------


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
    }
}

void SyslogStream::deliverQueue()
{
    _startMillis = _timeout + millis();
    while(millis() < _startMillis && !_syslog.isSending() && _syslog._queue.isAvailable()) {
        auto item = _syslog._queue.get();
        if (!item) {
            break;
        }
        _syslog.transmit(*item);
	}
}

void SyslogStream::dumpQueue(Print &output, bool items) const
{
    if (_syslog.getState(Syslog::StateType::HAS_CONNECTION)) {
        output.printf_P(PSTR("connected=%d "), _syslog.getState(Syslog::StateType::CONNECTED));
    }
    return _syslog._queue.dump(output, items);
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

