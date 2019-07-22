/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if LOGGER

#include <Arduino_compat.h>

#ifndef DEBUG_LOGGER
#define DEBUG_LOGGER 1
#endif

#define Logger_error        _logger.error
#define Logger_security     _logger.security
#define Logger_warning      _logger.warning
#define Logger_notice       _logger.notice
#define Logger_access       _logger.access
#define Logger_debug        _logger.debug

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

    const String getLogLevelAsString(LogLevel logLevel);
    void setLogLevel(LogLevel logLevel);

#if SYSLOG
    void setSyslog(SyslogStream *syslog);
#endif

protected:
    void writeLog(LogLevel logLevel, const char *message, va_list arg);
    inline void writeLog(LogLevel logLevel, const String &message, va_list arg) {
        writeLog(logLevel, message.c_str(), arg);
    }
    inline void writeLog(LogLevel logLevel, const __FlashStringHelper *message, va_list arg) {
        writeLog(logLevel, String(message).c_str(), arg);
    }

private:
    const String getLogFilename(LogLevel logLevel);
    bool openLog(LogLevel logLevel);
    void closeLog();

private:
    File _file;
    LogLevel _logLevel;
#if SYSLOG
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
