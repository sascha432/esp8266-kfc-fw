/**
 * Author: sascha_lammers@gmx.de
 */

#if LOGGER

#include "logger.h"
#include <time.h>
#if SYSLOG
#include <KFCSyslog.h>
#endif
#include "timezone.h"
#include "kfc_fw_config.h"

#if DEBUG_LOGGER
#include <debug_helper_enable.h>
#include <misc.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(log_file_messages, "log://messages");
PROGMEM_STRING_DEF(log_file_access, "log://access");
#if DEBUG
PROGMEM_STRING_DEF(log_file_debug, "log://debug");
#endif

Logger _logger;

Logger::Logger()
{
    _logLevel = LOGLEVEL_ACCESS;
#if SYSLOG
    _syslog = nullptr;
#endif
    _messagesLogSize = 0;
    _accessLogSize = 0;
    _debugLogSize = 0;
    if (_openLog(LOGLEVEL_ERROR, false)) {
        _messagesLogSize = _file.size();
        _closeLog();
    }
    if (_openLog(LOGLEVEL_ACCESS, false)) {
        _accessLogSize = _file.size();
        _closeLog();
    }
    if (_openLog(LOGLEVEL_DEBUG, false)) {
        _debugLogSize = _file.size();
        _closeLog();
    }
}

void Logger::error(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_ERROR, message, arg);
    va_end(arg);
}

void Logger::error(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_ERROR, message, arg);
    va_end(arg);
}

void Logger::security(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_SECURITY, message, arg);
    va_end(arg);
}

void Logger::security(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_SECURITY, message, arg);
    va_end(arg);
}

void Logger::warning(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_WARNING, message, arg);
    va_end(arg);
}

void Logger::warning(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_WARNING, message, arg);
    va_end(arg);
}

void Logger::notice(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_NOTICE, message, arg);
    va_end(arg);
}

void Logger::notice(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_NOTICE, message, arg);
    va_end(arg);
}

void Logger::access(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_ACCESS, message, arg);
    va_end(arg);
}

void Logger::access(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_ACCESS, message, arg);
    va_end(arg);
}

void Logger::debug(const __FlashStringHelper *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_DEBUG, message, arg);
    va_end(arg);
}

void Logger::debug(const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(LOGLEVEL_DEBUG, message, arg);
    va_end(arg);
}

void Logger::log(LogLevel level, const String &message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(level, message, arg);
    va_end(arg);
}

void Logger::log(LogLevel level, const char *message, ...)
{
    va_list arg;
    va_start(arg, message);
    this->writeLog(level, message, arg);
    va_end(arg);
}

const String Logger::getLogLevelAsString(LogLevel logLevel)
{
    switch(logLevel) {
        case LOGLEVEL_ERROR:
            return F("ERROR");
        case LOGLEVEL_SECURITY:
            return F("SECURITY");
        case LOGLEVEL_WARNING:
            return F("WARNING");
        case LOGLEVEL_NOTICE:
            return F("NOTICE");
        case LOGLEVEL_ACCESS:
            return F("ACCESS");
        case LOGLEVEL_DEBUG:
            return F("DEBUG");
    }
    return _sharedEmptyString;
}

void Logger::setLogLevel(LogLevel logLevel)
{
    _logLevel = logLevel;
}

#if SYSLOG
void Logger::setSyslog(SyslogStream * syslog)
{
    _syslog = syslog;
}
#endif

File Logger::openLog(const char *filename)
{
    String logFile;
    if (!strcmp_P(filename, SPGM(log_file_messages))) {
        return openMessagesLog();
    }
#if DEBUG
    else if (!strcmp_P(filename, SPGM(log_file_debug))) {
        return openDebugLog();
    }
#endif
    else if (!strcmp_P(filename, SPGM(log_file_access))) {
        return openAccessLog();
    }
    return File();
}

File Logger::openMessagesLog()
{
    _openLog(LOGLEVEL_ERROR, false);
    return _file;
}

File Logger::openDebugLog()
{
    _openLog(LOGLEVEL_DEBUG, false);
    return _file;
}

#if DEBUG

File Logger::openAccessLog()
{
    _openLog(LOGLEVEL_ACCESS, false);
    return _file;
}

#endif

void Logger::getLogs(StringVector &logs)
{
    logs.clear();
    if (LOGLEVEL_ERROR <= _logLevel) {
        logs.emplace_back(std::move(String(FSPGM(log_file_messages))));
    }
    if (LOGLEVEL_ACCESS <= _logLevel) {
        logs.emplace_back(std::move(String(FSPGM(log_file_access))));
    }
#if DEBUG
    if (LOGLEVEL_DEBUG <= _logLevel) {
        logs.emplace_back(std::move(String(FSPGM(log_file_debug))));
    }
#endif
    debug_printf_P(PSTR("Logger::getLogs(): %s\n"), implode(',', &logs).c_str());
}

void Logger::writeLog(LogLevel logLevel, const char *message, va_list arg)
{
// Serial.printf("|%s|\n",message);
    if (logLevel > _logLevel) {
        return;
    }

    time_t now = time(nullptr);
    char temp[128];
    char* buffer = temp;
    const String logLevelStr = getLogLevelAsString(logLevel);
    bool isOpen = _openLog(logLevel);
    struct tm *tm;

    tm = timezone_localtime(&now);
    timezone_strftime_P(temp, sizeof(temp), PSTR("%FT%TZ"), tm);
    if (isOpen) {
        _file.write((const uint8_t *)temp, strlen(temp));
    } else {
        _debug_printf_P(PSTR("Cannot append to log file %s\n"), _getLogFilename(logLevel).c_str());
    }

    size_t len;
    len = vsnprintf(temp, sizeof(temp), message, arg);
    if (len > sizeof(temp) - 1) {
        buffer = (char *)malloc(len + 1);
        if (!buffer) {
            buffer = temp; // just send the limited size
            len = sizeof(temp) - 1;
        } else {
            len = vsnprintf(buffer, len + 1, message, arg);
        }
    }

#if LOGGER_SERIAL_OUTPUT
    if (config._H_GET(Config().flags).atModeEnabled) {
        char temp2[32];
        timezone_strftime_P(temp2, sizeof(temp2), PSTR("%FT%TZ"), tm);
#if DEBUG
        debug_printf_P(PSTR("%s [%s] %s\n"), temp2, logLevelStr.c_str(), buffer);
#else
        MySerial.printf_P(PSTR("+LOGGER: %s [%s] %s\n"), temp2, logLevelStr.c_str(), buffer);
#endif
    }
#endif

    if (isOpen) {
        _file.write(' ');
        _file.write('[');
        _file.write((const uint8_t *)logLevelStr.c_str(), logLevelStr.length());
        _file.write(']');
        _file.write(' ');
        _file.write((const uint8_t *)buffer, len);
        _file.write('\n');
    }

    _closeLog();

#if SYSLOG
    if (_syslog) {
        _debug_println(F("sending message to syslog"));
        switch(logLevel) {
            default:
            case LOGLEVEL_ERROR:
                _syslog->setSeverity(SYSLOG_ERR);
                _syslog->setFacility(SYSLOG_FACILITY_KERN);
                break;
            case LOGLEVEL_SECURITY:
                _syslog->setSeverity(SYSLOG_WARN);
                _syslog->setFacility(SYSLOG_FACILITY_SECURE);
                break;
            case LOGLEVEL_WARNING:
                _syslog->setSeverity(SYSLOG_WARN);
                _syslog->setFacility(SYSLOG_FACILITY_KERN);
                break;
            case LOGLEVEL_NOTICE:
                _syslog->setSeverity(SYSLOG_NOTICE);
                _syslog->setFacility(SYSLOG_FACILITY_KERN);
                break;
            case LOGLEVEL_ACCESS:
                _syslog->setSeverity(SYSLOG_NOTICE);
                _syslog->setFacility(SYSLOG_FACILITY_SECURE);
                break;
            case LOGLEVEL_DEBUG:
                _syslog->setSeverity(SYSLOG_DEBUG);
                _syslog->setFacility(SYSLOG_FACILITY_LOCAL0);
                break;
        }
        _syslog->write((const uint8_t *)buffer, len);
        _syslog->flush();
    }
#endif

    if (buffer != temp) {
        free(buffer);
    }
}

const String Logger::_getLogFilename(LogLevel logLevel)
{
    switch(logLevel) {
        case LOGLEVEL_DEBUG:
            return F("/debug");
        case LOGLEVEL_ACCESS:
            return F("/access");
        default:
            return F("/messages");
    }
}

bool Logger::_openLog(LogLevel logLevel, bool write)
{
    if (_file) {
        _closeLog();
    }
#if DEBUG_LOGGER
    DEBUG_OUTPUT.printf_P(PSTR("Logger::openLog(): open(): logLevel=%u\n"), logLevel);
#endif
    auto fileName = _getLogFilename(logLevel);
    _file = SPIFFS.open(fileName, write ? (SPIFFS.exists(fileName) ? fs::FileOpenMode::append : fs::FileOpenMode::write) : fs::FileOpenMode::read);
    return (bool)_file;
}

void Logger::_closeLog()
{
#if LOGGER_MAX_FILESIZE
    if (_file.size() >= LOGGER_MAX_FILESIZE) {
        String filename = _file.name();
        String backFilename = filename;
        backFilename += F(".bak");
#if DEBUG_LOGGER
        DEBUG_OUTPUT.printf_P(PSTR("Logger::closeLog(): renaming %s to %s\n"), filename.c_str(), backFilename.c_str());
#endif
        _file.close();
        SPIFFS.remove(backFilename);
        SPIFFS.rename(filename, backFilename);
        return;
    }
#endif
#if DEBUG_LOGGER
        DEBUG_OUTPUT.printf_P(PSTR("Logger::closeLog(): close(): filename=%s\n"), _file.name());
#endif
    _file.close();
}

#endif
