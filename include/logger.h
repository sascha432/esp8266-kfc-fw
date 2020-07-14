/**
 * Author: sascha_lammers@gmx.de
 */

#if LOGGER

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_LOGGER
#define DEBUG_LOGGER                    0
#endif

#ifndef LOGGER_SERIAL_OUTPUT
#define LOGGER_SERIAL_OUTPUT            1
#endif

#define Logger_error                    _logger.error
#define Logger_security                 _logger.security
#define Logger_warning                  _logger.warning
#define Logger_notice                   _logger.notice
#define Logger_access                   _logger.access
#define Logger_debug                    _logger.debug

PROGMEM_STRING_DECL(log_file_messages);
PROGMEM_STRING_DECL(log_file_access);
#if DEBUG
PROGMEM_STRING_DECL(log_file_debug);
#endif

class Logger;
class SyslogStream;

extern Logger _logger;

typedef enum {
    LOGLEVEL_ERROR,
    LOGLEVEL_SECURITY,
    LOGLEVEL_WARNING,
    LOGLEVEL_NOTICE,
    LOGLEVEL_ACCESS,
    LOGLEVEL_DEBUG,
} LogLevel;

class Logger {
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
    void access(const String &message, ...);
    void access(const __FlashStringHelper *message, ...);
    void debug(const String &message, ...);
    void debug(const __FlashStringHelper *message, ...);
    void log(LogLevel level, const String &message, ...);
    void log(LogLevel level, const char *message, ...);

    const String getLogLevelAsString(LogLevel logLevel);
    void setLogLevel(LogLevel logLevel);

#if SYSLOG_SUPPORT
    void setSyslog(SyslogStream *syslog);
#endif

    File openLog(const char *filename);
    File openMessagesLog();
    File openAccessLog();
#if DEBUG
    File openDebugLog();
#endif
    void getLogs(StringVector &logs);

protected:
    void writeLog(LogLevel logLevel, const char *message, va_list arg);
    inline void writeLog(LogLevel logLevel, const String &message, va_list arg) {
        writeLog(logLevel, message.c_str(), arg);
    }
    inline void writeLog(LogLevel logLevel, const __FlashStringHelper *message, va_list arg) {
        writeLog(logLevel, String(message).c_str(), arg);
    }

private:
    const String _getLogFilename(LogLevel logLevel);
    bool _openLog(LogLevel logLevel, bool write = true);
    void _closeLog();

private:
    File _file;
    size_t _messagesLogSize;
    size_t _accessLogSize;
    size_t _debugLogSize;
    LogLevel _logLevel;
#if SYSLOG_SUPPORT
    SyslogStream *_syslog;
#endif
};

#else

#define Logger_error(...)        ;
#define Logger_security(...)     ;
#define Logger_warning(...)      ;
#define Logger_notice(...)       ;
#define Logger_access(...)       ;
#define Logger_debug(...)        ;

#endif
