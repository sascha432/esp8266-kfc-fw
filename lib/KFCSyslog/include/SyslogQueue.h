/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define SYSLOG_MEMORY_QUEUE_ITEM_SIZE               sizeof(SyslogQueueItem) + 1


typedef uint32_t SyslogQueueId_t;
typedef uint16_t SyslogQueueIndex_t;

class SyslogQueueItem {
public:
	SyslogQueueItem();
	SyslogQueueItem(const String &message, Syslog *syslog);

	void clear();

	void setId(SyslogQueueId_t id);
	SyslogQueueId_t getId() const;

	void setMessage(const String &message);
	const String &getMessage() const;

	void setSyslog(Syslog *syslog);
	Syslog *getSyslog();
	bool isSyslog(Syslog *syslog) const;

	bool lock();
	bool unlock();
	bool isLocked() const;

	uint8_t incrFailureCount();
	uint8_t getFailureCount();

    void transmit(Syslog::Callback_t callback);

private:
    SyslogQueueId_t _id;
    String _message;
    Syslog *_syslog;
    uint8_t _failureCount : 7;
    uint8_t _locked : 1;
};

// Stores a single message in memory and supports only a single filter/destination
class SyslogQueue {
public:
    typedef std::unique_ptr<SyslogQueueItem> SyslogQueueItemPtr;

    SyslogQueue();
    virtual ~SyslogQueue() {
    }

    virtual SyslogQueueId_t add(const String &message, Syslog *syslog);
    virtual void remove(SyslogQueueItemPtr &item, bool success);
    virtual size_t size() const;

    virtual SyslogQueueItemPtr &at(SyslogQueueIndex_t index);

    // gets called after a new message has been added and each time the queue is checked
    virtual void cleanUp() {
    }

    virtual void clear() {
        while(size()) {
            remove(at(0), false);
        }
    }

private:
    SyslogQueueItemPtr _item;
};


// Stores multiple messages in memory to resend them if an error occurs. If it runs out of space, new messages are dropped
class SyslogMemoryQueue : public SyslogQueue {
public:
    typedef std::vector<SyslogQueueItemPtr> SyslogMemoryQueueVector;

    SyslogMemoryQueue(size_t maxSize);

    SyslogQueueId_t add(const String &message, Syslog *syslog) override;
    void remove(SyslogQueueItemPtr &item, bool success) override;
    size_t size() const override;

    SyslogQueueItemPtr &at(SyslogQueueIndex_t index) override;

private:
    size_t _maxSize;
    size_t _curSize;
    SyslogMemoryQueueVector _items;
    SyslogQueueIndex_t _uniqueId;
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

    SyslogQueueId_t add(const String &message, Syslog *syslog) override {
    }

    void remove(SyslogQueueItemPtr &item, bool success) override {
    }

    size_t size() const override {
        return 0;
    }

    SyslogQueueItemPtr &at(SyslogQueueIndex_t index) override;

private:
    String _filename;
    File _file;
    size_t _maxSize;
    SyslogQueueId_t _uniqueId;
};

#endif
