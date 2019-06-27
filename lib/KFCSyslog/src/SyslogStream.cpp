/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogFactory.h"
#include "SyslogFilter.h"
#include "SyslogQueue.h"
#include "SyslogStream.h"

SyslogStream::SyslogStream(const SyslogParameter parameter, SyslogProtocol protocol, const char * host, uint16_t port, uint16_t queueSize) {
	_filter = new SyslogFilter(parameter);
	_parameter = &_filter->getParameter();
	_queue = new SyslogMemoryQueue(queueSize);
	_filter->addFilter(F("*.*"), SyslogFactory::create(*_parameter, protocol, host, port));
}

SyslogStream::SyslogStream(SyslogFilter* filter, SyslogQueue* queue) : _parameter(&filter->getParameter()), _filter(filter), _queue(queue) {
}

SyslogStream::~SyslogStream() {
    if (_queue) {
        delete _queue;
    }
    if (_filter) {
        delete _filter;
    }
}

void SyslogStream::setFacility(SyslogFacility facility) {
    _parameter->setFacility(facility);
}

void SyslogStream::setSeverity(SyslogSeverity severity) {
    _parameter->setSeverity(severity);
}

size_t SyslogStream::write(uint8_t data) {
    _message += (char)data;
    return 1;
}

size_t SyslogStream::write(const uint8_t *buffer, size_t len) {
    auto ptr = buffer;
    while (len--) {
        char ch = *ptr++;
        if (ch == '\n') {
            flush();
        } else {
            _message += ch;
        }
    }
    return ptr - buffer;
}

void SyslogStream::flush() {
    if (_message.length()) {
        _filter->applyFilters([this](SyslogFileFilterItem& filter) {
            String preparedMessage;
            filter.syslog->addHeader(preparedMessage);
            if (preparedMessage.length()) {
                preparedMessage += _message;
                _queue->add(preparedMessage, filter.syslog);
            }
        });
        _message = String();
        deliverQueue();
    }
}

bool SyslogStream::hasQueuedMessages() {
    return !!_queue->size();
}

int SyslogStream::available() {
    return false;
}

int SyslogStream::read() {
    return -1;
}

int SyslogStream::peek() {
    return -1;
}

void SyslogStream::deliverQueue(Syslog *syslog) {
	for (auto _item = _queue->begin(); _item != _queue->end(); ++_item) {
		auto item = *_item;
		if (item->isSyslog(syslog) && !item->getSyslog()->isSending()) {
			item->transmit([this, item](bool success) {
				transmitCallback(item, success);
			});
		}
	}
	_queue->cleanUp();
}

void SyslogStream::transmitCallback(SyslogMemoryQueueItem *item, bool success) {
#if DEBUG
	if (item->isLocked()) {
		debug_printf_P(PSTR("transmitCallback(%d): syslog queue item '%s' locked\n"), success, item->getMessage().c_str());
	}
#endif
    if (success) {
        _queue->remove(item, true);
    } else {
        if (item->incrFailureCount() >= SYSLOG_STREAM_MAX_FAILURES) {
            _queue->remove(item, false);
        }
    }
}

const String SyslogStream::getLevel() {
	String result;
	uint8_t i;

	auto facility = _parameter->getFacility();
    for (i = 0; syslogFilterFacilityItems[i].value != 0xff; i++) {
        if (syslogFilterFacilityItems[i].value == facility) {
            result += syslogFilterFacilityItems[i].name;
            break;
        }
    }
	result += '.';
	auto severity = _parameter->getSeverity();
	for(i = 0; syslogFilterSeverityItems[i].value != 0xff; i++) {
		if (syslogFilterSeverityItems[i].value == severity) {
			result += syslogFilterSeverityItems[i].name;
			break;
		}
	}

	return result;
}
