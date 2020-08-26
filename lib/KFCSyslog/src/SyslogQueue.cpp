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
// StringAllocSize
// ------------------------------------------------------------------------

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

// ------------------------------------------------------------------------
// SyslogQueueItem
// ------------------------------------------------------------------------


SyslogQueueItem::SyslogQueueItem() :
    _id(0),
    _millis(0),
    _data(0)
{
}

SyslogQueueItem::SyslogQueueItem(const String &message, uint32_t id) :
    _message(message),
    _id(id),
    _millis(millis()),
    _data(0)
{
}

SyslogQueueItem::SyslogQueueItem(SyslogQueueItem &&move) noexcept :
    _message(std::move(move._message)),
    _id(std::exchange(move._id, 0)),
    _millis(std::exchange(move._millis, 0)),
    _data(std::exchange(move._data, 0))
{
}

#undef new

SyslogQueueItem &SyslogQueueItem::operator=(SyslogQueueItem &&move) noexcept
{
    new(this) SyslogQueueItem(std::move(move));
    return *this;
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

bool SyslogQueueItem::isLocked() const
{
    return _locked;
}

void SyslogQueueItem::setLocked(bool locked)
{
    _locked = locked;
}

bool SyslogQueueItem::try_lock()
{
    noInterrupts();
    if (_locked) {
        interrupts();
        return false;
    }
    _locked = true;
    interrupts();
    return true;
}

size_t SyslogQueueItem::getFailureCount() const
{
    return _failureCount;
}

void SyslogQueueItem::setFailureCount(size_t count)
{
    _failureCount = count;
    __LDBG_assert(_failureCount <= getMaxFailureCount());
}

void SyslogQueueItem::incrFailureCount()
{
    _failureCount++;
    __LDBG_assert(_failureCount <= getMaxFailureCount());
}

// ------------------------------------------------------------------------
// SyslogQueue
// ------------------------------------------------------------------------

SyslogQueue::SyslogQueue() : SyslogQueue(nullptr)
{
}

SyslogQueue::SyslogQueue(SyslogQueueManager &manager) : _dropped(0), _manager(&manager)
{
    managerQueueSize(0, false);
}

SyslogQueue::SyslogQueue(SyslogQueueManager *manager) : _dropped(0), _manager(manager)
{
    managerQueueSize(0, false);
}

SyslogQueue::~SyslogQueue()
{
    managerQueueSize(0, false);
}

void SyslogQueue::managerQueueSize(uint32_t size, bool isAvailable)
{
    __LDBG_printf("size=%u available=%u", size, isAvailable);
    if (_manager) {
        _manager->queueSize(size, isAvailable);
    }
}

uint32_t SyslogQueue::getDropped() const
{
    return _dropped;
}

void SyslogQueue::setManager(SyslogQueueManager &manager)
{
    _manager = &manager;
}

void SyslogQueue::removeManager()
{
    _manager = nullptr;
}

size_t SyslogQueue::_getQueueItemSize(const String &msg) const
{
    return StringAllocSize::getAllocSize(msg.length()) + kSyslogQueueItemSize;
}

