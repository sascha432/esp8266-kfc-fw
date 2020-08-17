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

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogStream::SyslogStream(SyslogParameter &parameter, SyslogProtocol protocol, const String &host, uint16_t port, uint16_t queueSize) : _parameter(parameter)
{
	_filter = __LDBG_new(SyslogFilter, parameter);
	_queue = __LDBG_new(SyslogMemoryQueue, queueSize);
	_filter->addFilter(F("*.*"), SyslogFactory::create(_parameter, protocol, host, port));
}

SyslogStream::SyslogStream(SyslogFilter* filter, SyslogQueue* queue) : _parameter(filter->getParameter()), _filter(filter), _queue(queue)
{
}

SyslogStream::~SyslogStream() {
    if (_queue) {
        __LDBG_delete(_queue);
        _queue = nullptr;
    }
    if (_filter) {
        __LDBG_delete(_filter);
    }
}

void SyslogStream::setFacility(SyslogFacility facility)
{
    _parameter.setFacility(facility);
}

void SyslogStream::setSeverity(SyslogSeverity severity)
{
    _parameter.setSeverity(severity);
}

size_t SyslogStream::write(uint8_t data)
{
    _message += (char)data;
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
            _message += ch;
        }
    }
    return ptr - buffer;
}

void SyslogStream::flush()
{
    if (_message.length()) {
        _filter->applyFilters([this](SyslogFileFilterItem& filter) {
            String preparedMessage;
            filter.getSyslog()->addHeader(preparedMessage);
            if (preparedMessage.length()) {
                preparedMessage += _message;
                _queue->add(preparedMessage, filter.getSyslog());
            }
        });
        _message = String();
        deliverQueue();
    }
}

bool SyslogStream::hasQueuedMessages()
{
    return !!_queue->size();
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

void SyslogStream::deliverQueue(Syslog *syslog)
{
    size_t pos = 0;
    while(pos < _queue->size()) {
        _debug_printf_P(PSTR("SyslogStream::deliverQueue(): %u/%u\n"), pos + 1, _queue->size());
		auto &item = _queue->at(pos++);
		if (item && item->isSyslog(syslog) && !item->getSyslog()->isSending()) {

            // get pointer and id of SyslogQueueItem to find it inside the vector later
            // SyslogQueueItemPtr might became a dangling reference
            auto *itemPtr = item.get();
            auto itemUniqueId = item->getId();
            _debug_printf_P(PSTR("SyslogStream::deliverQueue(): 1,item=%p,itemPtr=%p,itemUniqueId=%u\n"), &item, itemPtr, itemUniqueId);

			item->transmit([this, itemPtr, itemUniqueId](bool success) {
                SyslogQueue::SyslogQueueItemPtr *item = nullptr; // find "item" again
                for(size_t pos2 = 0; pos2 < _queue->size(); pos2++) {
                    auto &itemCmp = _queue->at(pos2);
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

void SyslogStream::transmitCallback(SyslogQueue::SyslogQueueItemPtr &item, bool success)
{
    _debug_printf_P(PSTR("SyslogStream::transmitCallback(): success=%d,message='%s',locked=%d\n"), success, item->getMessage().c_str(), item->isLocked());
    if (success) {
        _queue->remove(item, true);
    } else {
        if (item->incrFailureCount() >= SYSLOG_STREAM_MAX_FAILURES) {
            _queue->remove(item, false);
        }
    }
}

String SyslogStream::getLevel() const
{
	String result;

	auto facility = _parameter.getFacility();
    for (uint8_t i = 0; syslogFilterFacilityItems[i].value != 0xff; i++) {
        if (syslogFilterFacilityItems[i].value == facility) {
            result += syslogFilterFacilityItems[i].name;
            break;
        }
    }
	result += '.';
	auto severity = _parameter.getSeverity();
	for(uint8_t i = 0; syslogFilterSeverityItems[i].value != 0xff; i++) {
		if (syslogFilterSeverityItems[i].value == severity) {
			result += syslogFilterSeverityItems[i].name;
			break;
		}
	}

	return result;
}
