/**
 * Author: sascha_lammers@gmx.de
 */

#include "SyslogMemoryQueue.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogMemoryQueue::SyslogMemoryQueue(SyslogQueueManager *manager, size_t maxSize) : SyslogQueue(manager), _timer(0), _maxSize(maxSize), _curSize(0), _uniqueId(0)
{
}

SyslogMemoryQueue::SyslogMemoryQueue(size_t maxSize) : SyslogMemoryQueue(nullptr, maxSize)
{
}

SyslogMemoryQueue::SyslogMemoryQueue(SyslogQueueManager &manager, size_t maxSize) : SyslogMemoryQueue(&manager, maxSize)
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
    managerQueueSize(_items.size(), _isAvailable());
    return _uniqueId;
}

size_t SyslogMemoryQueue::size() const
{
    size_t count = 0;
    for(const auto &item: _items) {
        if (!item.isLocked()) {
            count++;
        }
    }
    return count;
}

void SyslogMemoryQueue::clear()
{
    _curSize = 0;
    _items.clear();
    managerQueueSize(0, false);
}

void SyslogMemoryQueue::dump(Print &output, bool items) const
{
    if (_timer) {
        uint32_t ms = millis();
        if (ms <= _timer) {
            output.printf_P(PSTR("locked=%.3fs "), (_timer - ms) / 1000.0);
        }
    }
    output.printf_P(PSTR("size=%u max=%u messages=%u\n"), _curSize, _maxSize, _items.size());
    if (items) {
        for(const auto &item: _items) {
            output.printf_P(PSTR("id=%08x locked=%u errors=%u msg="), item.getId(), item.isLocked(), item.getFailureCount());
            output.println(item.getMessage());
        }
    }
}

bool SyslogMemoryQueue::empty() const
{
    return _items.empty();
}

bool SyslogMemoryQueue::isAvailable()
{
    if (_timer && millis() >= _timer) {
        // unlock all messages after the timer has expired
        for(auto &item: _items) {
            item.setLocked(false);
        }
        _timer = 0;
        managerQueueSize(_items.size(), _isAvailable());
    }
    return _isAvailable();
}

bool SyslogMemoryQueue::_isAvailable() const
{
    return _timer == 0 && _items.empty() == false;
}

const SyslogQueue::Item *SyslogMemoryQueue::get()
{
    for(auto &item: _items) {
        if (item.try_lock()) {
            managerQueueSize(_items.size(), false);
            return &item;
        }
    }
    __DBG_panic("size=%u", _items.size());
    return nullptr;
}

void SyslogMemoryQueue::remove(uint32_t id, bool success)
{
    auto iterator = std::find(_items.begin(), _items.end(), id);
    // auto iterator = std::find_if(_items.begin(), _items.end(), [id](const Item &item) {
    //     return item._id == id && item._locked;
    // });
    if (iterator != _items.end()) {
        if (success == false && iterator->getFailureCount() < kFailureRetryLimit) {
            iterator->incrFailureCount();
            // delay sending next message
            _timer = millis() + kFailureWaitDelay;
            __LDBG_printf("retry=%u in %ums", iterator->getFailureCount(), kFailureWaitDelay);
            managerQueueSize(_items.size(), false);
        }
        else {
            __LDBG_printf("erase success=%u retry=%u max=%u", success, iterator->getFailureCount(), kFailureRetryLimit);
            if (success == false && iterator->getFailureCount() >= kFailureRetryLimit) {
                _dropped++;
            }
            _curSize -= _getQueueItemSize(iterator->getMessage());
            _items.erase(iterator);
            if (_items.empty()) {
                _timer = 0;
            }
            managerQueueSize(_items.size(), _isAvailable());
        }
    }
}
