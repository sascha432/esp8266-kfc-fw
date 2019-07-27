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

SyslogStream::SyslogStream(const SyslogParameter &parameter, SyslogProtocol protocol, const String &host, uint16_t port, uint16_t queueSize) {
	_filter = _debug_new SyslogFilter(parameter);
	_parameter = &_filter->getParameter();
	_queue = _debug_new SyslogMemoryQueue(queueSize);
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
    size_t pos = 0;
    while(pos < _queue->size()) {
		auto &item = _queue->at(pos++);
		if (item && item->isSyslog(syslog) && !item->getSyslog()->isSending()) {

            // TODO review. passing &item leads to dangling references (by resizing the vector for example)

            auto *itemPtr = item.get();
            auto itemUniqueId = itemPtr->getId();
            _debug_printf_P(PSTR("SyslogStream::deliverQueue(): 1,item=%p,itemPtr=%p,itemUniqueId=%u\n"), &item, itemPtr, itemUniqueId);

            // "item" has a new address, _queue->at(pos++) has become invalid, but we can use the pointer to find the new reference inside the vector
            // DEBUG00005803 (SyslogStream.cpp:95<33912> deliverQueue): SyslogStream::deliverQueue(): 1,item=0x3fff3154,itemPtr=0x3fff11cc
            // DEBUG00006015 (SyslogStream.cpp:110<33488> operator()): SyslogStream::deliverQueue(): 2,item=0x3fff469c,itemPtr=0x3fff11cc,(bool)*item=1

            // the issue is that it would be theoretically possible that another SyslogQueueItem points to the same address (itemPtr) later in time, that
            // is up to malloc. using a real unique id like getId()+SyslogQueueItem * would work, but redesigning that part is probably a better idea

			item->transmit([this, itemPtr, itemUniqueId](bool success) {
                SyslogQueue::SyslogQueueItemPtr *item = nullptr;
                for(size_t pos = 0; pos < _queue->size(); pos++) {
                    auto &itemCmp = _queue->at(pos);
                    if (itemCmp && itemCmp.get() == itemPtr && itemCmp.get()->getId() == itemUniqueId) {
                        item = &itemCmp;
                        break;
                    }
                }
                _debug_printf_P(PSTR("SyslogStream::deliverQueue(): 2,item=%p,itemPtr=%p,(bool)*item=%d,itemUniqueId=%u\n"), item, itemPtr, item ? (bool)*item : -1, itemUniqueId);
                if (item) {
				    transmitCallback(*item, success);
                }
			});
		}
	}
	_queue->cleanUp();
}

void SyslogStream::transmitCallback(SyslogQueue::SyslogQueueItemPtr &item, bool success) {
    _debug_printf_P(PSTR("SyslogStream::transmitCallback(): success=%d,message='%s',locked=%d\n"), success, item->getMessage().c_str(), item->isLocked());
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
