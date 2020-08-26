/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "SyslogQueue.h"

class SyslogPlugin;

// Stores multiple messages in memory to resend them if an error occurs. If it runs out of space, new messages are dropped
class SyslogMemoryQueue : public SyslogQueue {
public:
    typedef std::vector<Item> QueueVector;

    SyslogMemoryQueue(size_t maxSize);
    SyslogMemoryQueue(SyslogQueueManager &manager, size_t maxSize);

    virtual bool isAvailable() override;
    virtual const Item &get() override;
    virtual uint32_t add(const String &message) override;
    virtual size_t size() const override;
    virtual void remove(uint32_t id, bool success);
    virtual void clear() override;
    virtual void dump(Print &output, bool items) const;
    virtual bool empty() const;

private:
    friend SyslogPlugin;

    SyslogMemoryQueue(SyslogQueueManager *manager, size_t maxSize);

    bool _isAvailable() const;

    QueueVector _items;
    uint32_t _timer;
    size_t _maxSize;
    size_t _curSize;
    uint32_t _uniqueId;
};
