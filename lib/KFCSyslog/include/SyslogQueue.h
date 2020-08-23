/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

class Syslog;
class SyslogMemoryQueue;

class SyslogQueueItem {
public:
	SyslogQueueItem();
	SyslogQueueItem(const String &message, uint32_t id = 1);

    operator uint32_t() const {
        return _id;
    }

	void clear();
	uint32_t getId() const;
	const String &getMessage() const;

private:
    friend Syslog;
    friend SyslogMemoryQueue;

    String _message;
    struct __attribute__packed__ {
        uint32_t _id;
        uint8_t _failureCount: 7;
        uint8_t _locked: 1;
    };
};

// Stores a single message in memory and supports only a single filter/destination
class SyslogQueue {
public:
    using Item = SyslogQueueItem;

    static constexpr size_t kSyslogQueueItemSize = sizeof(Item);
    static constexpr uint32_t kFailureWaitDelay = 1000U;        // delay before retrying in milliseconds
    static constexpr uint8_t kFailureRetryLimit = 60;           // number of times before discarding a message or the entire queue

    SyslogQueue();
    virtual ~SyslogQueue();

    // returns the id
    virtual uint32_t add(const String &message) = 0;

    // return true if queue is ready to send a new item
    virtual bool canSend() = 0;

    // get one item from the queue
    // canSend() must be checked before
    virtual const Item &get() = 0;

    // remove item after it has been delivered
    virtual void remove(uint32_t id, bool success) = 0;

    // return size of the queue excluding messages in transit
    virtual size_t size() const = 0;

    // clear queue
    virtual void clear() = 0;

    // dump queue in human readable format
    virtual void dump(Print &output) const = 0;

    // return true if the queue is empty and all messages have been delivered
    virtual bool empty() const = 0;

protected:
    // get size of SyslogQueueItem::Ptr for a particular message
    size_t _getQueueItemSize(const String &msg) const;
};


// Stores multiple messages in memory to resend them if an error occurs. If it runs out of space, new messages are dropped
class SyslogMemoryQueue : public SyslogQueue {
public:
    typedef std::vector<Item> QueueVector;

    SyslogMemoryQueue(size_t maxSize);

    virtual bool canSend() override;
    virtual const Item &get() override;
    virtual uint32_t add(const String &message) override;
    virtual size_t size() const override;
    virtual void remove(uint32_t id, bool success);
    virtual void clear() override;
    virtual void dump(Print &output) const;
    virtual bool empty() const;

private:
    QueueVector _items;
    uint32_t _timer;
    size_t _maxSize;
    size_t _curSize;
    uint32_t _uniqueId;
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
