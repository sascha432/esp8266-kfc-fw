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

size_t SyslogQueue::_getQueueItemSize(const String &msg) const
{
    return StringAllocSize::getAllocSize(msg.length()) + kSyslogQueueItemSize;
}

