/**
 * Author: sascha_lammers@gmx.de
 */

#if LOGGER

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <EnumHelper.h>
#include <Mutex.h>

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
class SyslogStream;

extern Logger _logger;

#pragma push_macro("DEBUG")
#undef DEBUG

class Logger {
public:
    enum class Level : uint8_t {
        NONE = 0,
        ERROR = _BV(0),
        SECURITY = _BV(1),
        WARNING = _BV(2),
        NOTICE = _BV(3),
        DEBUG = _BV(4),
        MAX,
        _DEBUG = DEBUG,
        ANY = std::numeric_limits<std::underlying_type<Level>::type>::max()
    };

public:
    Logger();

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

    static const __FlashStringHelper *getLevelAsString(Level logLevel);
    static Level getLevelFromString(PGM_P str);
    void setLevel(Level logLevel);

    #if SYSLOG_SUPPORT
        void setSyslog(SyslogStream *syslog);
    #endif

    bool isExtraFileEnabled(Level level) const;
    void setExtraFileEnabled(Level level, bool state);

    File __openLog(Level logLevel, bool write);
    void __rotate(Level logLevel);

protected:
    void writeLog(Level logLevel, const char *message, va_list arg);
    void writeLog(Level logLevel, const String &message, va_list arg);
    void writeLog(Level logLevel, const __FlashStringHelper *message, va_list arg);

private:
    String _getLogFilename(Level logLevel);
    String _getLogDevice(Level logLevel);
    String _getBackupFilename(const String &filename, int num);
    void _closeLog(File file);

private:
    Level _logLevel;
    Level _enabled;
    #if SYSLOG_SUPPORT
        SyslogStream *_syslog;
    #endif
    #if ESP32
        SemaphoreMutex _lock;
    #endif
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
    auto level = static_cast<Level>(1 << static_cast<uint8_t>(stringlist_find_P_P(reinterpret_cast<PGM_P>(getLevelAsString(Level::ANY)), str, '|')));
    __LDBG_assert(level < Level::MAX); // as long as Level::ANY is defined correctly, we cannot get any invalid level. defaults to 0
    return level;
}

inline void Logger::setLevel(Level logLevel)
{
    _logLevel = logLevel;
}

#if SYSLOG_SUPPORT

inline void Logger::setSyslog(SyslogStream *syslog)
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
    auto fileName = _getLogFilename(logLevel);
    if (write) {
        __LDBG_printf("logLevel=%u append=%s", logLevel, fileName.c_str());
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
#include <debug_helper_disable.h>
#endif

#else

#define Logger_error(...)        ;
#define Logger_security(...)     ;
#define Logger_warning(...)      ;
#define Logger_notice(...)       ;
#define Logger_debug(...)        ;

#endif

