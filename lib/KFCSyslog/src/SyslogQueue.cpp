/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <algorithm>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogQueue.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogQueue::SyslogQueue() {
}

SyslogQueueId_t SyslogQueue::add(const String &message, Syslog *syslog) {
    SyslogQueueItemPtr newItem(new SyslogQueueItem(message, syslog));
    newItem->setId(_item ? _item->getId() + 1 : 1);
    _item.swap(newItem);
    return _item->getId();
}

void SyslogQueue::remove(SyslogQueueItemPtr &item, bool success) {
    if (item) {
        item.release();
    }
}

size_t SyslogQueue::size() const {
    return _item ? 1 : 0;
}

SyslogQueue::SyslogQueueItemPtr &SyslogQueue::at(SyslogQueueIndex_t index) {
    return _item;
}

void SyslogQueue::cleanUp() {
}


SyslogMemoryQueue::SyslogMemoryQueue(size_t maxSize) {
    _maxSize = maxSize;
    _curSize = 0;
    _id = 0;
    _count = 0;
}

SyslogQueueId_t SyslogMemoryQueue::add(const String &message, Syslog *syslog) {
    _debug_printf_P(PSTR("SyslogMemoryQueue::add(): id=%d,queue_size=%u,mem_size=%u,message='%s'\n"), (_count + 1), _items.size(), _curSize, message.c_str());
    size_t size = message.length() + SYSLOG_MEMORY_QUEUE_ITEM_SIZE;
    if (size + _curSize > _maxSize) {
        _debug_printf_P(PSTR("SyslogMemoryQueue::add(): queue full (%d > %d), discarding last message\n"), size + _curSize, _maxSize);
        return 0;  // no space left, discard message
    }
    ;
    _items.push_back(SyslogQueueItemPtr(new SyslogQueueItem(message, syslog)));
    _items.back()->setId(++_count);
    _curSize += size;
    return _id;
}

void SyslogMemoryQueue::remove(SyslogQueue::SyslogQueueItemPtr &removeItem, bool success) {
    if (!removeItem) {
        _debug_printf_P(PSTR("SyslogMemoryQueue::invalid pointer\n"));
        panic();
    }
    _debug_printf_P(PSTR("SyslogMemoryQueue::remove(): id=%d,success=%d,queue_size=%u,mem_size=%u,message='%s'\n"), removeItem->getId(), success, _items.size(), _curSize, removeItem->getMessage().c_str());
    _items.erase(std::remove_if(_items.begin(), _items.end(), [this, &removeItem](const SyslogQueueItemPtr &item) {
        if (item == removeItem) {
            _curSize -= removeItem->getMessage().length() + SYSLOG_MEMORY_QUEUE_ITEM_SIZE;
            return true;
        }
        return false;
    }), _items.end());
}

size_t SyslogMemoryQueue::size() const {
    return _items.size();
}

SyslogQueue::SyslogQueueItemPtr &SyslogMemoryQueue::at(SyslogQueueIndex_t index) {
    return _items.at(index);
}

void SyslogMemoryQueue::cleanUp() {
    // _items.erase(std::remove_if(_items.begin(), _items.end(), [this](const SyslogQueueItemPtr &item) {
    //     if (item.getId() == 0) {
    //         _curSize -= item.getMessage().length() + SYSLOG_MEMORY_QUEUE_ITEM_SIZE;
    //         return true;
    //     }
    //     return false;
    // }), _items.end());
}

SyslogQueueItem::SyslogQueueItem() {
    _id = 0;
    _syslog = nullptr;
}

SyslogQueueItem::SyslogQueueItem(const String &message, Syslog *syslog) {
	_id = 1;
	_message = message;
	_locked = 0;
	_failureCount = 0;
	_syslog = syslog;
}

void SyslogQueueItem::setId(SyslogQueueId_t id) {
	_id = id;
}

SyslogQueueId_t SyslogQueueItem::getId() const {
	return _id;
}

void SyslogQueueItem::setMessage(const String &message) {
	_message = message;
}

const String & SyslogQueueItem::getMessage() const {
	return _message;
}

void SyslogQueueItem::setSyslog(Syslog *syslog) {
	_syslog = syslog;
}

Syslog * SyslogQueueItem::getSyslog() {
	return _syslog;
}

bool SyslogQueueItem::isSyslog(Syslog *syslog) const {
	return (syslog == nullptr || syslog == _syslog);
}

bool SyslogQueueItem::lock() {
    _debug_printf_P(PSTR("SyslogMemoryQueueItem::lock(): locked=%d,id=%d\n"), _locked, getId());
	if (_locked) {
		return false;
	}
	_locked = 1;
	return true;
}

bool SyslogQueueItem::unlock() {
    _debug_printf_P(PSTR("SyslogMemoryQueueItem::unlock(): locked=%d,id=%d\n"), _locked, getId());
	if (_locked) {
		_locked = 0;
        return true;
    }
	return false;
}

bool SyslogQueueItem::isLocked() const {
	return !!_locked;
}

uint8_t SyslogQueueItem::incrFailureCount() {
    _debug_printf_P(PSTR("SyslogMemoryQueueItem::incrFailureCount(): id=%u,failure_count=%u\n"), getId(), _failureCount);
	return ++_failureCount;
}

uint8_t SyslogQueueItem::getFailureCount() {
	return _failureCount;
}

void SyslogQueueItem::transmit(SyslogCallback callback) {
    _debug_printf_P(PSTR("SyslogMemoryQueueItem::transmit(): canSend=%d, callback %p\n"), _syslog->canSend(), &callback);
	if (_syslog->canSend()) {
		if (lock()) {
            _syslog->transmit(_message, [this, callback](bool success) {
				if (unlock()) {
                    _debug_printf_P(PSTR("SyslogMemoryQueueItem::transmit(): callback=%p, success=%d\n"), &callback, success);
                    callback(success);
				} else {
                    _debug_printf_P(PSTR("SyslogMemoryQueueItem::transmit(): callback=%p, success=%d, item unlocked\n"), &callback, success);
                }
			});
		}
	}
}
