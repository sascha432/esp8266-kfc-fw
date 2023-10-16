/**
 * Author: sascha_lammers@gmx.de
 */

#if LOGGER

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <EnumHelper.h>
#include <Mutex.h>
#include <vector>
#include <list>
#include <Scheduler.h>

#ifndef DEBUG_LOGGER
#    define DEBUG_LOGGER (0 || defined(DEBUG_ALL))
#endif

#ifndef LOGGER_SERIAL_OUTPUT
#    define LOGGER_SERIAL_OUTPUT 1
#endif

#if DEBUG_LOGGER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#define Logger_error    _logger.error
#define Logger_security _logger.security
#define Logger_warning  _logger.warning
#define Logger_notice   _logger.notice
#define Logger_debug    _logger.debug

class Logger;
class Syslog;

extern Logger _logger;

#pragma push_macro("DEBUG")
#undef DEBUG

class Logger {
public:
    enum class Level : uint8_t {
        NONE = 0,
        ERROR = _BV(0),
        MIN = ERROR,
        SECURITY = _BV(1),
        WARNING = _BV(2),
        NOTICE = _BV(3),
        DEBUG = _BV(4),
        MAX
    };

public:
    Logger();

    // end logger and flush queue
    void end();

    // send error
    void error(const String &message, ...);
    void error(const __FlashStringHelper *message, ...);
    void security(const String &message, ...);
    void security(const __FlashStringHelper *message, ...);
    void warning(const String &message, ...);
    void warning(const __FlashStringHelper *message, ...);
    void notice(const String &message, ...);
    void notice(const __FlashStringHelper *message, ...);
    void debug(const String &message, ...);
    void debug(const __FlashStringHelper *message, ...);
    void log(Level level, const String &message, ...);
    void log(Level level, const char *message, ...);

    // getters / setters
    static const __FlashStringHelper *getLevelAsString(Level logLevel);
    static Level getLevelFromString(PGM_P str);
    void setLevel(Level logLevel);

    #if SYSLOG_SUPPORT
        void setSyslog(Syslog *syslog);
    #endif

    bool isExtraFileEnabled(Level level) const;
    void setExtraFileEnabled(Level level, bool state);

    File __openLog(Level logLevel, bool write);
    void __rotate(Level logLevel);

protected:
    void writeLog(Level logLevel, const char *message, va_list arg);
    void writeLog(Level logLevel, const String &message, va_list arg);
    void writeLog(Level logLevel, const __FlashStringHelper *message, va_list arg);

public:
    struct MemoryQueueType {
        uint32_t millis;
        Level logLevel;
        uint8_t headerLength;
        std::vector<char> buffer;

        MemoryQueueType(uint32_t ms, Level level) :
            millis(ms),
            logLevel(level),
            headerLength(0)
        {
        }

        uint32_t writeDelay() const
        {
            // return at least 2
            switch(logLevel) {
                case Level::ERROR:
                    return 2;
                case Level::WARNING:
                case Level::SECURITY:
                    return 1000;
                default:
                    break;
            }
            return 5000;
        }

        MemoryQueueType(const MemoryQueueType &) = delete;
        void operator=(const MemoryQueueType &) = delete;

        MemoryQueueType(MemoryQueueType &&move) :
            millis(move.millis),
            logLevel(move.logLevel)  ,
            headerLength(std::exchange(move.headerLength, 0)),
            buffer(std::move(move.buffer))
        {
        }

        char *header()
        {
            return& buffer.data()[0];
        }

        size_t headerSize() const
        {
            return headerLength;
        }

        char *message()
        {
            return &buffer.data()[headerLength];
        }

        size_t messageSize() const
        {
            return buffer.size() - headerLength;
        }
    };

private:
    using MemoryQueueTypeList = std::list<Logger::MemoryQueueType>;

    struct MemoryQueueTypeListEx : MemoryQueueTypeList {
        static constexpr size_t getNodeSize() {
            return sizeof(MemoryQueueTypeList::_Node);
        }
    };

    struct QueueSizeType {
        size_t size() const {
            return _size;
        }
        size_t num() const {
            return _num;
        }
        QueueSizeType() : _size(0), _num(0) {
        }
        void add(const MemoryQueueType &item) {
            add(sizeof(item) + item.buffer.capacity() + MemoryQueueTypeListEx::getNodeSize());
        }
        void add(size_t size) {
            _size += size;
            _num++;
        }
        static QueueSizeType get(const MemoryQueueTypeList &queue) {
            QueueSizeType size;
            for(const auto &item: queue) {
                size.add(item);
            }
            return size;
        }
    private:
        size_t _size;
        size_t _num;
    };

    const __FlashStringHelper *_getLogFilename(Level logLevel);
    String _getLogDevice(Level logLevel);
    String _getBackupFilename(const String &filename, int num);
    void _closeLog(File file);

    static constexpr size_t kQueueMaxSize = 1536;
    static constexpr size_t kQueueMaxTimeout = 1000;
    void _flushQueue();

private:
    Level _logLevel;
    Level _enabled;
    MemoryQueueTypeList _queue;
    uint32_t _lastFlushTimer;
    Event::Timer _writeTimer;
    #if SYSLOG_SUPPORT
        Syslog *_syslog;
    #endif
    #if ESP32
        SemaphoreMutex _lock;
        SemaphoreMutex _flushLock;
    #endif
    SemaphoreMutex _queueLock;
};

inline void Logger::writeLog(Level logLevel, const String &message, va_list arg)
{
    writeLog(logLevel, message.c_str(), arg);
}

inline void Logger::writeLog(Level logLevel, const __FlashStringHelper *message, va_list arg)
{
    writeLog(logLevel, reinterpret_cast<PGM_P>(message), arg);
}

inline void Logger::error(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::ERROR, message, arg);
    va_end(arg);
}

inline void Logger::error(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::ERROR, message, arg);
    va_end(arg);
}

inline void Logger::security(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::SECURITY, message, arg);
    va_end(arg);
}

inline void Logger::security(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::SECURITY, message, arg);
    va_end(arg);
}

inline void Logger::warning(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::WARNING, message, arg);
    va_end(arg);
}

inline void Logger::warning(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::WARNING, message, arg);
    va_end(arg);
}

inline void Logger::notice(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::NOTICE, message, arg);
    va_end(arg);
}

inline void Logger::notice(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::NOTICE, message, arg);
    va_end(arg);
}

inline void Logger::debug(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::DEBUG, message, arg);
    va_end(arg);
}

inline void Logger::debug(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(Level::DEBUG, message, arg);
    va_end(arg);
}

inline void Logger::log(Level level, const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(level, message, arg);
    va_end(arg);
}

inline void Logger::log(Level level, const char *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(level, message, arg);
    va_end(arg);
}

inline Logger::Level Logger::getLevelFromString(PGM_P str)
{
    auto len = strlen_P(str);
    for(auto i = Level::MIN; i < Level::MAX; i = Level(int(i) + 1)) {
        auto level = String(getLevelAsString(i));
        if (level.equalsIgnoreCase(FPSTR(str)) || (len >= 2 && level.startsWithIgnoreCase(FPSTR(str)))) {
            return i;
        }
    }
    return Level::NONE;
}

inline void Logger::setLevel(Level logLevel)
{
    _logLevel = logLevel;
}

#if SYSLOG_SUPPORT

    inline void Logger::setSyslog(Syslog *syslog)
    {
        _syslog = syslog;
    }

#endif

inline bool Logger::isExtraFileEnabled(Level level) const
{
    return EnumHelper::Bitset::hasAny(_enabled, level);
}

inline void Logger::setExtraFileEnabled(Level level, bool state)
{
    EnumHelper::Bitset::setBits(_enabled, state, level);
}

inline File Logger::__openLog(Level logLevel, bool write)
{
    __LDBG_printf("__openLog level=%u", logLevel);
    auto fileName = String(_getLogFilename(logLevel));
    if (write) {
        __LDBG_printf("logLevel=%u append=%s", logLevel, fileName.c_str());
        // return KFCFS.open(fileName, fs::FileOpenMode::append);
        return createFileRecursive(fileName, fs::FileOpenMode::append);
    }
    __LDBG_printf("logLevel=%u read=%s", logLevel, fileName.c_str());
    return KFCFS.open(fileName, fs::FileOpenMode::read);
}

inline void Logger::__rotate(Level logLevel)
{
    __LDBG_printf("rotate=%u", logLevel);
    _closeLog(__openLog(logLevel, true));
}

inline String Logger::_getBackupFilename(const String &filename, int num)
{
    PrintString str = filename;
    if (num > 0) {
        str.printf_P(PSTR(".%u"), num);
    }
    str.print(F(".bak"));
    return str;
}

#pragma pop_macro("DEBUG")

#if DEBUG_LOGGER
#    include <debug_helper_disable.h>
#endif

#else

#define Logger_error(...)        ;
#define Logger_security(...)     ;
#define Logger_warning(...)      ;
#define Logger_notice(...)       ;
#define Logger_debug(...)        ;

#endif

