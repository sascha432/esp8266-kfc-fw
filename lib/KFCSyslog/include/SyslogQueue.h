/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define SYSLOG_MEMORY_QUEUE_ITEM_SIZE sizeof(SyslogMemoryQueueItem) + 1

typedef uint32_t SyslogQueueId_t;
typedef uint16_t SyslogQueueIndex_t;

class SyslogMemoryQueueItem {
public:
	SyslogMemoryQueueItem();
	SyslogMemoryQueueItem(SyslogQueueId_t id, const String message, Syslog *syslog);

	void clear();

	void setId(SyslogQueueId_t id);
	SyslogQueueId_t getId() const;

	void setMessage(const String message);
	const String &getMessage() const;

	void setSyslog(Syslog *syslog);
	Syslog *getSyslog();
	bool isSyslog(Syslog *syslog) const;

	bool lock();
	bool unlock();
	bool isLocked() const;

	uint8_t incrFailureCount();
	uint8_t getFailureCount();

    void transmit(SyslogCallback callback);

private:
    SyslogQueueId_t _id;
    String _message;
    Syslog *_syslog;
    uint8_t _failureCount : 7;
    uint8_t _locked : 1;
};

class SyslogQueue;

class SyslogQueueIterator {
public:
    SyslogQueueIterator(SyslogQueueIndex_t index, SyslogQueue *queue);

    SyslogQueueIterator &operator++();
    SyslogQueueIterator &operator--();

    bool operator!=(const SyslogQueueIterator &item);
    bool operator=(const SyslogQueueIterator &item);
    SyslogMemoryQueueItem *operator*();

private:
    SyslogQueueIndex_t _index;
    SyslogQueue *_queue;
};

// Stores a single message in memory and supports only a single filter/destination
class SyslogQueue {
public:
    SyslogQueue();
    virtual ~SyslogQueue() {}

    virtual SyslogQueueId_t add(const String message, Syslog *syslog);
    virtual void remove(SyslogMemoryQueueItem *item, bool success);
    virtual size_t size() const;

    virtual SyslogQueueIterator begin();
    virtual SyslogQueueIterator end();
    virtual SyslogMemoryQueueItem *getItem(SyslogQueueIndex_t index);

    virtual void cleanUp();

private:
    SyslogMemoryQueueItem _item;
};

typedef std::vector<SyslogMemoryQueueItem> SyslogMemoryQueueVector;

// Stores multiple messages in memory to resend them if an error occurs. If it runs out of space, new messages are dropped
class SyslogMemoryQueue : public SyslogQueue {
public:
    SyslogMemoryQueue(size_t maxSize);

    SyslogQueueId_t add(const String message, Syslog *syslog) override;
    void remove(SyslogMemoryQueueItem *item, bool success) override;
    size_t size() const override;

    SyslogQueueIterator begin() override;
    SyslogQueueIterator end() override;
    SyslogMemoryQueueItem *getItem(SyslogQueueIndex_t index) override;

    void cleanUp() override;

private:
    size_t _maxSize;
    size_t _curSize;
    SyslogQueueId_t _id;
    SyslogMemoryQueueVector _items;
    SyslogQueueIndex_t _count;
};
