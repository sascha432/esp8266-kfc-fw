/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <algorithm>
#include <memory>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogQueue.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
// SyslogQueueItem
// ------------------------------------------------------------------------


SyslogQueueItem::SyslogQueueItem() : _id(0), _millis(0), _failureCount(0), _locked(false)
{
}

SyslogQueueItem::SyslogQueueItem(const String &message, uint32_t id) : _message(message), _id(id), _millis(millis()), _failureCount(0), _locked(false)
{
}

uint32_t SyslogQueueItem::getId() const
{
	return _id;
}

uint32_t SyslogQueueItem::getMillis() const
{
    return _millis;
}

const String &SyslogQueueItem::getMessage() const
{
	return _message;
}

// ------------------------------------------------------------------------
// SyslogQueue
// ------------------------------------------------------------------------


SyslogQueue::SyslogQueue() : _dropped(0)
{
}

SyslogQueue::~SyslogQueue()
{
}

uint32_t SyslogQueue::getDropped() const
{
    return _dropped;
}

#if defined(ESP8266)

class StringAllocSize : public String {
public:
    static size_t getAllocSize(size_t len) {
        if (len < String::SSOSIZE) {
            return 0;
        }
        return (len + 16) & ~0xf;
    }
};

#else

class StringAllocSize {
public:
    static size_t getAllocSize(size_t len) {
        return (len + 8) & ~7;
    }
};

#endif

size_t SyslogQueue::_getQueueItemSize(const String &msg) const
{
    return StringAllocSize::getAllocSize(msg.length()) + kSyslogQueueItemSize;
}

// ------------------------------------------------------------------------
// SyslogMemoryQueue
// ------------------------------------------------------------------------

SyslogMemoryQueue::SyslogMemoryQueue(size_t maxSize) : _timer(0), _maxSize(maxSize), _curSize(0), _uniqueId(0)
{
}

uint32_t SyslogMemoryQueue::add(const String &message)
{
    size_t size = _getQueueItemSize(message);
    __LDBG_printf("id=%d items=%u size=%u item_size=%u msg=%s", (_uniqueId + 1), _items.size(), _curSize, size, message.c_str());

    // check if the queue can store more messages
    if (size + _curSize > _maxSize) {
        _dropped++;
        __LDBG_printf("queue full size=%u msg=%u max=%u dropped=%u", _curSize, size, _maxSize, _dropped);
        return 0;
    }
    _items.emplace_back(message, ++_uniqueId);
    _curSize += size;
    return _uniqueId;
}

size_t SyslogMemoryQueue::size() const
{
    size_t count = 0;
    for(const auto &item: _items) {
        if (!item._locked) {
            count++;
        }
    }
    return count;
}

void SyslogMemoryQueue::clear()
{
    _items.clear();
    _curSize = 0;
}

void SyslogMemoryQueue::dump(Print &output) const
{
    if (_timer) {
        uint32_t ms = millis();
        if (ms <= _timer) {
            output.printf_P(PSTR("Locked for %.3f seconds, "), (_timer - ms) / 1000.0);
        }
    }
    output.printf_P(PSTR("%u bytes, %u message(s)\n"), _curSize, _items.size());
    for(const auto &item: _items) {
        output.printf_P(PSTR("id=%08x locked=%u errors=%u msg="), item._id, item._locked, item._failureCount);
        output.println(item._message);
    }
}

bool SyslogMemoryQueue::empty() const
{
    return _items.empty();
}

bool SyslogMemoryQueue::canSend()
{
    if (_timer && millis() >= _timer) {
        // unlock all messages after the timer has expired
        for(auto &item: _items) {
            item._locked = false;
        }
        _timer = 0;
    }
    return _timer == 0 && size() != 0;
}

const SyslogQueue::Item &SyslogMemoryQueue::get()
{
    for(auto &item: _items) {
        if (!item._locked) {
            item._locked = true;
            return item;
        }
    }
    assert(false);
    return _items.at(0);
}

void SyslogMemoryQueue::remove(uint32_t id, bool success)
{
    auto iterator = std::find(_items.begin(), _items.end(), id);
    // auto iterator = std::find_if(_items.begin(), _items.end(), [id](const Item &item) {
    //     return item._id == id && item._locked;
    // });
    if (iterator != _items.end()) {
        if (success == false && iterator->_failureCount < kFailureRetryLimit) {
            iterator->_failureCount++;
            // delay sending next message
            _timer = millis() + kFailureWaitDelay;
            __LDBG_printf("retry=%u in %ums", iterator->_failureCount, kFailureWaitDelay);
        }
        else {
            __LDBG_printf("erase success=%u retry=%u max=%u", success, iterator->_failureCount, kFailureRetryLimit);
            if (success == false && iterator->_failureCount >= kFailureRetryLimit) {
                _dropped++;
            }
            _curSize -= _getQueueItemSize(iterator->_message);
            _items.erase(iterator);
            if (_items.empty()) {
                _timer = 0;
            }
        }
    }
}
