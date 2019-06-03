/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Asynchronous syslog library with queue and different storage types
 *
 * SyslogStream -> SyslogFilter -> SyslogQueue(Memory/File) -> SyslogUDP/TCP/File
 *
 *
 */

#pragma once

#include <Arduino_compat.h>
#include <algorithm>
#include <functional>
#include <vector>

#define USE_RFC5424 1  // does not seem to be supported by many syslog daemons

#if USE_RFC5424
#define SYSLOG_VERSION "1"
#define SYSLOG_TIMESTAMP_FORMAT "%FT%TZ"
#define SYSLOG_TIMESTAMP_FORMAT_MAX_LEN 24
#else  // old/fall-back format
#define SYSLOG_TIMESTAMP_FORMAT "%h %d %T"
#define SYSLOG_TIMESTAMP_FORMAT_MAX_LEN 16
#endif
#define SYSLOG_FILE_TIMESTAMP_FORMAT "%FT%TZ"

enum SyslogProtocol {
    SYSLOG_PROTOCOL_NONE = 0,
    SYSLOG_PROTOCOL_UDP,
    SYSLOG_PROTOCOL_TCP,
    SYSLOG_PROTOCOL_TCP_TLS,
    SYSLOG_PROTOCOL_FILE,
};


/*
    Numerical 		Severity Code

    0			Emergency : system is unusable
    1			Alert : action must be taken immediately
    2			Critical : critical conditions
    3			Error : error conditions
    4			Warning : warning conditions
    5			Notice : normal but significant condition
    6			Informational : informational messages
    7			Debug: debug-level messages
                                                                                                                                                                                                                                                                          level messages
*/

enum SyslogSeverity {
    SYSLOG_EMERG = 0,
    SYSLOG_ALERT,
    SYSLOG_CRIT,
    SYSLOG_ERR,
    SYSLOG_WARN,
    SYSLOG_NOTICE,
    SYSLOG_INFO,
    SYSLOG_DEBUG,
    SYSLOG_SEVERITY_ANY = 0xff,
};

/*
    Numerical		Facility Code

    0				kernel messages
    1				user-level messages
    2				mail system
    3				system daemons
    4				security/authorization messages
    5				messages generated internally by syslogd
    6				line printer subsystem
    7				network news subsystem
    8				UUCP subsystem
    9				clock daemon
    10				security/authorization messages
    11				FTP daemon
    12				NTP subsystem
    13				log audit
    14              log alert
    15              clock daemon (note 2)
    16              local use 0  (local0)
    17              local use 1  (local1)
    18              local use 2  (local2)
    19              local use 3  (local3)
    20              local use 4  (local4)
    21              local use 5  (local5)
    22              local use 6  (local6)
    23              local use 7  (local7)
*/

typedef uint8_t SyslogFacility;

enum {
    SYSLOG_FACILITY_KERN = 0,
    SYSLOG_FACILITY_USER,
    SYSLOG_FACILITY_MAIL,
    SYSLOG_FACILITY_DAEMON,
    SYSLOG_FACILITY_SECURE,
    SYSLOG_FACILITY_SYSLOG,
    SYSLOG_FACILITY_NTP = 12,
    SYSLOG_FACILITY_LOCAL0 = 16,
    SYSLOG_FACILITY_LOCAL1,
    SYSLOG_FACILITY_LOCAL2,
    SYSLOG_FACILITY_LOCAL3,
    SYSLOG_FACILITY_LOCAL4,
    SYSLOG_FACILITY_LOCAL5,
    SYSLOG_FACILITY_LOCAL6,
    SYSLOG_FACILITY_LOCAL7,
    SYSLOG_FACILITY_ANY = 0xff,
};

class Syslog;

#define SYSLOG_MEMORY_QUEUE_ITEM_SIZE sizeof(uint32_t) + sizeof(uint8_t) + sizeof(Syslog *) + 1

struct SyslogMemoryQueueItem {
    uint32_t id;
    String message;
    uint8_t failureCount;
    Syslog *syslog;
};

class SyslogQueue;

class SyslogQueueIterator {
public:
	SyslogQueueIterator(uint32_t index, SyslogQueue *queue);

	SyslogQueueIterator &operator++();
	SyslogQueueIterator &operator--();

	bool operator!=(const SyslogQueueIterator &item);
	bool operator=(const SyslogQueueIterator &item);
    SyslogMemoryQueueItem *operator*();

private:
	uint32_t _index;
	SyslogQueue *_queue;
};

// Stores a single message in memory and supports only a single filter/destination
class SyslogQueue {
public:
	SyslogQueue();

    virtual uint32_t add(const String &message, Syslog *syslog);
    virtual void remove(SyslogMemoryQueueItem *item, bool success);
    virtual size_t size() const;

	virtual SyslogQueueIterator begin();
	virtual SyslogQueueIterator end();
	virtual SyslogMemoryQueueItem *getItem(uint32_t index);

	virtual void cleanUp();

private:
	SyslogMemoryQueueItem _item;
};

typedef std::vector<SyslogMemoryQueueItem> SyslogMemoryQueueVector;

// Stores multiple messages in memory to resend them if an error occurs. If it runs out of space, new messages are dropped
class SyslogMemoryQueue : public SyslogQueue {
   public:
    SyslogMemoryQueue(uint32_t maxSize);

    uint32_t add(const String &message, Syslog *syslog) override;
    void remove(SyslogMemoryQueueItem *item, bool success) override;
    size_t size() const override;

	SyslogQueueIterator begin() override;
	SyslogQueueIterator end() override;
	SyslogMemoryQueueItem *getItem(uint32_t index) override;

	void cleanUp() override;

   private:
    uint32_t _maxSize;
    uint32_t _curSize;
    uint32_t _id;
    SyslogMemoryQueueVector _items;
	uint16_t _count;
};

class SyslogParameter {
public:
    SyslogParameter();
	SyslogParameter(const char *hostname, const char *appName, const char *processId);
	SyslogParameter(const String hostname, const String appName);
	SyslogParameter(const String hostname, const String appName, const String processId);

    void setFacility(SyslogFacility facility);
    SyslogFacility getFacility();

    void setSeverity(SyslogSeverity severity);
    SyslogSeverity getSeverity();

	void setHostname(const char *hostname);
	void setHostname(const String hostname);
    const String &getHostname();

    void setAppName(const char *appName);
	void setAppName(const String appName);
    const String &getAppName();

    void setProcessId(const char *processId);
	void setProcessId(const String processId);
    const String &getProcessId();

private:
    SyslogFacility _facility;
    SyslogSeverity _severity;
    String _hostname;
    String _appName;
    String _processId;
};

// facility/severity name/value pairs
struct SyslogFilterItem {
    PGM_P name;
    uint8_t value;
};

typedef std::vector<std::pair<SyslogFacility, SyslogSeverity>> SyslogFilterItemVector;

struct SyslogFileFilterItem {
    SyslogFilterItemVector filter;
    Syslog *syslog;
};

typedef std::function<void(SyslogFileFilterItem &)> SyslogFilterCallback;

typedef std::vector<SyslogFileFilterItem> SyslogFiltersVector;

typedef std::vector<std::pair<const String, Syslog *>> SyslogObjectsVector;

#define SYSLOG_FILTER_STOP	nullptr

class SyslogFilter {
public:
    SyslogFilter(const SyslogParameter parameter);

    /**
     * @HOST[:PORT]				forward to UDP (port is 514 by default)
     * @@HOST[:PORT]			forward to TCP (port 514)
     * @@!HOST[:PORT]			forward to TCP using TLS (port 6514)
     * /var/log/message[:max size[:max rotate]]
     *                          store in file (max size = 64K, max rotate = 10)
     * stop						discard message
     */
    void addFilter(const String filter, const String destination);
    void addFilter(const String filter, Syslog *syslog);
    SyslogParameter &getParameter();

    void applyFilters(SyslogFilterCallback callback);

private:
    SyslogFilterItemVector _parseFilter(const String filter);
    bool _matchFilterExpression(const SyslogFilterItemVector &filter, SyslogFacility facility, SyslogSeverity severity);
    Syslog *_createSyslogFromString(const String &str);

    SyslogFiltersVector _filters;
    SyslogParameter _parameter;
	SyslogObjectsVector _syslogObjects;
};

typedef std::function<void(bool success)> SyslogCallback;

class Syslog {
   public:
    Syslog(SyslogParameter &parameter);

	void transmit(const String message, SyslogCallback callback) {
		transmit(message.c_str(), message.length(), callback);
	}
    virtual void transmit(const char *message, size_t length, SyslogCallback callback);

    virtual void addHeader(String &buffer);
	virtual bool canSend();

   protected:
    void _addTimestamp(String &buffer, PGM_P format = SYSLOG_TIMESTAMP_FORMAT);
    void _addParameter(String &buffer, const String &value);

    SyslogParameter &_parameter;
};

#define SYSLOG_PORT_UDP 514

class SyslogUDP : public Syslog {
public:
    SyslogUDP(SyslogParameter &parameter, const char *host, uint16_t port = SYSLOG_PORT_UDP);

    void transmit(const char *message, size_t length, SyslogCallback callback) override;

private:
    String _host;
    uint16_t _port;
    WiFiUDP _udp;
};

#define SYSLOG_PORT_TCP 514
#define SYSLOG_PORT_TCP_TLS 6514

class SyslogTCP : public Syslog {
public:
    SyslogTCP(SyslogParameter &parameter, const char *host, uint16_t port = SYSLOG_PORT_TCP, bool useTLS = false);

private:
    String _host;
    uint16_t _port;
    bool _useTLS;
};

#define SYSLOG_FILE_MAX_SIZE 0xffff
#define SYSLOG_FILE_MAX_ROTATE 10

class SyslogFile : public Syslog {
public:
    SyslogFile(SyslogParameter &parameter, const String filename, uint32_t maxSize = SYSLOG_FILE_MAX_SIZE, uint16_t maxRotate = SYSLOG_FILE_MAX_ROTATE);

    void addHeader(String &buffer) override;
    void transmit(const char *message, size_t length, SyslogCallback callback) override;
	bool canSend() override;

private:
    String _filename;
    uint32_t _maxSize;
    uint16_t _maxRotate;
};

#define Syslog_log(syslog, severity, format, ...) \
    {                                               \
        syslog.setSeverity(severity);               \
        syslog.printf(format, __VA_ARGS__); \
        syslog.flush();                             \
    }
#define Syslog_log_P(syslog, severity, format, ...) \
    {                                               \
        syslog.setSeverity(severity);               \
        syslog.printf_P(format, __VA_ARGS__); \
        syslog.flush();                             \
    }

class SyslogStream : public Stream {
   public:
    const uint8_t MAX_FAILURES = 10;
    const uint8_t QUEUE_MAX_SEND_TIME = 20;  // TODO adjust values for async. TCP sockets
    const uint8_t QUEUE_MAX_DELAY = 2;  // TODO adjust values for async. TCP sockets and slow UDP connections... since there is no feedback assume every udp connection is "slow" rather than dropping packets. for tcp there should be an ack packet

    SyslogStream(SyslogFilter *filter, SyslogQueue *queue);
    ~SyslogStream();

    void setFacility(SyslogFacility facility);
    void setSeverity(SyslogSeverity severity);

    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buffer, size_t len) override;
    void flush() override;

    bool hasQueuedMessages();

    int available() override;
    int read() override;
    int peek() override;

	void deliverQueue(Syslog *syslog = nullptr);

    void transmitCallback(SyslogMemoryQueueItem *item, bool success);

	const String getLevel();

   private:
    SyslogParameter &_parameter;
    SyslogFilter *_filter;
    SyslogQueue *_queue;
    String _message;
};
