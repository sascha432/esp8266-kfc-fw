/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

class Syslog;

class SyslogQueueItem {
public:
    SyslogQueueItem(const SyslogQueueItem &) = delete;
    SyslogQueueItem &operator=(const SyslogQueueItem &) = delete;

    SyslogQueueItem();
	SyslogQueueItem(const String &message, uint32_t id = 1);
    SyslogQueueItem(SyslogQueueItem &&move) noexcept;

    SyslogQueueItem &operator=(SyslogQueueItem &&move) noexcept;

    operator uint32_t() const {
        return _id;
    }

	uint32_t getId() const;
    uint32_t getMillis() const;
	const String &getMessage() const;

    bool isLocked() const;
    bool try_lock();
    void setLocked(bool locked);
    size_t getFailureCount() const;
    void setFailureCount(size_t count);
    void incrFailureCount();

    static constexpr size_t getMaxFailureCount() {
        return 126;
    }

private:
    friend Syslog;

    String _message;
    uint32_t _id;
    uint32_t _millis;
    union __attribute__packed__  {
        struct __attribute__packed__ {
            uint8_t _failureCount : 7;
            uint8_t _locked : 1;
        };
        uint8_t _data;
    };
};

class SyslogQueueManager {
public:
    virtual void queueSize(uint32_t size, bool isAvailable) = 0;       // report queue size including locked/queued items
};

class SyslogQueue {
public:
    using Item = SyslogQueueItem;

    static constexpr size_t kSyslogQueueItemSize = sizeof(Item);
    static constexpr uint32_t kFailureWaitDelay = 1000U;            // delay before retrying in milliseconds
    static constexpr uint8_t kFailureRetryLimit = 60;               // number of times before discarding a message or the entire queue

    static_assert(kFailureRetryLimit < SyslogQueueItem::getMaxFailureCount(), "too many retries");

    SyslogQueue();
    SyslogQueue(SyslogQueueManager &_manager);
    virtual ~SyslogQueue();

    // returns the id
    virtual uint32_t add(const String &message) = 0;

    // return true if queue is ready to send a new item
    virtual bool isAvailable() = 0;

    // get one item from the queue
    // isAvailable() must be checked before
    virtual const Item &get() = 0;

    // remove item after it has been delivered
    virtual void remove(uint32_t id, bool success) = 0;

    // return size of the queue excluding messages in transit
    virtual size_t size() const = 0;

    // clear queue
    virtual void clear() = 0;

    // dump queue in human readable format
    virtual void dump(Print &output, bool items) const = 0;

    // return true if the queue is empty and all messages have been delivered
    virtual bool empty() const = 0;

    uint32_t getDropped() const;

    void setManager(SyslogQueueManager &manager);

    void removeManager();

protected:
    SyslogQueue(SyslogQueueManager *_manager = nullptr);

    void managerQueueSize(uint32_t size, bool isAvailable);

    // get size of SyslogQueueItem::Ptr for a particular message
    size_t _getQueueItemSize(const String &msg) const;
    uint32_t _dropped;
    SyslogQueueManager *_manager;
};


#if 0
// persistant storage that survives reboots and can store a significant amount of messages if the syslog server isn't available
// TODO
class SyslogSPIFFSQueue : public SyslogQueue {
public:
    SyslogSPIFFSQueue(const String &filename, size_t maxSize) {
        _maxSize = maxSize;
        _filename = filename;
    }

    virtual IdType add(const String &message) override {
    }

    virtual void remove(uint32_t id, bool success) override {
    }

    virtual size_t size() const override {
        return 0;
    }

private:
    String _filename;
    File _file;
    size_t _maxSize;
    uint32_t _uniqueId;
};

#endif
