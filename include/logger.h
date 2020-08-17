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

class Logger;
class SyslogStream;

extern Logger _logger;

typedef enum {
    LOGLEVEL_ERROR,
    LOGLEVEL_SECURITY,
    LOGLEVEL_WARNING,
    LOGLEVEL_NOTICE,
    LOGLEVEL_DEBUG,
    LOGLEVEL_MAX
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
    void debug(const String &message, ...);
    void debug(const __FlashStringHelper *message, ...);
    void log(LogLevel level, const String &message, ...);
    void log(LogLevel level, const char *message, ...);

    const String getLogLevelAsString(LogLevel logLevel);
    void setLogLevel(LogLevel logLevel);

#if SYSLOG_SUPPORT
    void setSyslog(SyslogStream *syslog);
#endif

    bool isExtraFileEnabled(LogLevel level) const;
    void setExtraFileEnabled(LogLevel level, bool state);

    File __openLog(LogLevel logLevel, bool write);
    void __rotate(LogLevel logLevel);

protected:
    void writeLog(LogLevel logLevel, const char *message, va_list arg);
    inline void writeLog(LogLevel logLevel, const String &message, va_list arg) {
        writeLog(logLevel, message.c_str(), arg);
    }
    inline void writeLog(LogLevel logLevel, const __FlashStringHelper *message, va_list arg) {
        writeLog(logLevel, String(message).c_str(), arg);
    }

private:
    String _getLogFilename(LogLevel logLevel);
    String _getLogDevice(LogLevel logLevel);
    String _getBackupFilename(String filename, int num);
    void _closeLog(File file);

private:
    LogLevel _logLevel;
    uint8_t _enabled;
#if SYSLOG_SUPPORT
    SyslogStream *_syslog;
#endif
};

#else

#define Logger_error(...)        ;
#define Logger_security(...)     ;
#define Logger_warning(...)      ;
#define Logger_notice(...)       ;
#define Logger_debug(...)        ;

#endif
