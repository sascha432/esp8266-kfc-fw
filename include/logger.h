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
#define Logger_debug                    _logger.debug

class Logger;
class SyslogStream;

extern Logger _logger;

#pragma push_macro("DEBUG")
#undef DEBUG

class Logger {
public:
    enum class Level : uint8_t {
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
    inline void writeLog(Level logLevel, const String &message, va_list arg) {
        writeLog(logLevel, message.c_str(), arg);
    }
    inline void writeLog(Level logLevel, const __FlashStringHelper *message, va_list arg) {
        writeLog(logLevel, String(message).c_str(), arg);
    }

private:
    String _getLogFilename(Level logLevel);
    String _getLogDevice(Level logLevel);
    String _getBackupFilename(String filename, int num);
    void _closeLog(File file);

private:
    Level _logLevel;
    Level _enabled;
#if SYSLOG_SUPPORT
    SyslogStream *_syslog;
#endif
};

#pragma pop_macro("DEBUG")

#else

#define Logger_error(...)        ;
#define Logger_security(...)     ;
#define Logger_warning(...)      ;
#define Logger_notice(...)       ;
#define Logger_debug(...)        ;

#endif

