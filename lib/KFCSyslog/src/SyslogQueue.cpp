/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include "SyslogQueue.h"

SyslogQueue::SyslogQueue() {
}

SyslogQueueId_t SyslogQueue::add(const String message, Syslog* syslog) {
    _item.setMessage(message);
    _item.setId(1);
    return 1;
}

void SyslogQueue::remove(SyslogMemoryQueueItem* item, bool success) {
    item->clear();
}

size_t SyslogQueue::size() const {
    return _item.getId() ? 1 : 0;
}

SyslogQueueIterator SyslogQueue::begin() {
    return SyslogQueueIterator(0, this);
}

SyslogQueueIterator SyslogQueue::end() {
    return SyslogQueueIterator(_item.getId() ? 1 : 0, this);
}

SyslogMemoryQueueItem* SyslogQueue::getItem(SyslogQueueIndex_t index) {
    return &_item;
}

void SyslogQueue::cleanUp() {
}

SyslogMemoryQueue::SyslogMemoryQueue(size_t maxSize) {
    _maxSize = maxSize;
    _curSize = 0;
    _id = 0;
    _count = 0;
}

SyslogQueueId_t SyslogMemoryQueue::add(const String message, Syslog* syslog) {
    size_t size = message.length() + SYSLOG_MEMORY_QUEUE_ITEM_SIZE;
    if (size + _curSize > _maxSize) {
        return 0;  // no space left, discard message
    }
    _items.push_back(SyslogMemoryQueueItem(++_id, message, syslog));
    _curSize += size;
    _count++;
    return _id;
}

void SyslogMemoryQueue::remove(SyslogMemoryQueueItem* item, bool success) {
    if (item->getId()) {
        item->clear();
        _count--;
    }
}

size_t SyslogMemoryQueue::size() const {
    return _count;
}

SyslogQueueIterator SyslogMemoryQueue::begin() {
    return SyslogQueueIterator(0, this);
}

SyslogQueueIterator SyslogMemoryQueue::end() {
    return SyslogQueueIterator((SyslogQueueIndex_t)_items.size(), this);
}

SyslogMemoryQueueItem* SyslogMemoryQueue::getItem(SyslogQueueIndex_t index) {
    return &_items[index];
}

void SyslogMemoryQueue::cleanUp() {
    auto newEnd = std::remove_if(_items.begin(), _items.end(), [this](SyslogMemoryQueueItem& item) {
        if (item.getId() == 0) {
            _curSize -= item.getMessage().length() + SYSLOG_MEMORY_QUEUE_ITEM_SIZE;
            return true;
        }
        return false;
    });
    _items.erase(newEnd, _items.end());
}

SyslogQueueIterator::SyslogQueueIterator(SyslogQueueIndex_t index, SyslogQueue* queue) : _index(index), _queue(queue) {
}

SyslogQueueIterator& SyslogQueueIterator::operator++() {
    ++_index;
    return *this;
}
SyslogQueueIterator& SyslogQueueIterator::operator--() {
    --_index;
    return *this;
}

bool SyslogQueueIterator::operator!=(const SyslogQueueIterator& item) {
    return _index != item._index;
}
bool SyslogQueueIterator::operator=(const SyslogQueueIterator& item) {
    return _index == item._index;
}

SyslogMemoryQueueItem* SyslogQueueIterator::operator*() {
    return _queue->getItem(_index);
}

void SyslogMemoryQueueItem::transmit(SyslogCallback callback) {
    _syslog->transmit(_message, callback);
}
