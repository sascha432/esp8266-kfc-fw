/**
 * Author: sascha_lammers@gmx.de
 */

#if LOGGER

#include "logger.h"
#include <PrintString.h>
#include <time.h>
#include <vector>
#if SYSLOG_SUPPORT
#include <KFCSyslog.h>
#endif
#include <misc.h>
#include "kfc_fw_config.h"

#if DEBUG_LOGGER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

Logger _logger;

Logger::Logger() :
    _logLevel(LOGLEVEL_NOTICE),
    // _logFileSize(),
#if DEBUG
    _enabled(_BV(LOGLEVEL_ERROR)|_BV(LOGLEVEL_WARNING)|_BV(LOGLEVEL_SECURITY))
#else
    _enabled(_BV(LOGLEVEL_ERROR)|_BV(LOGLEVEL_SECURITY))
#endif
#if SYSLOG_SUPPORT
    , _syslog(nullptr)
#endif
{
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
        case LOGLEVEL_DEBUG:
            return F("DEBUG");
        default:
            break;
    }
    return emptyString;
}

void Logger::setLogLevel(LogLevel logLevel)
{
    _logLevel = logLevel;
}

#if SYSLOG_SUPPORT
void Logger::setSyslog(SyslogStream * syslog)
{
    _syslog = syslog;
}
#endif

bool Logger::isExtraFileEnabled(LogLevel level) const
{
    return _enabled & (1 << level);
}

void Logger::setExtraFileEnabled(LogLevel level, bool state)
{
    if (state) {
        _enabled |= (1 << level);
    } else {
        _enabled &= ~(1 << level);
    }
}

void Logger::writeLog(LogLevel logLevel, const char *message, va_list arg)
{
// Serial.printf("|%s|\n",message);
    if (logLevel > _logLevel) {
        return;
    }

    PrintString tmp;
    time_t now = time(nullptr);
    auto file = __openLog(logLevel, true);
    if (!file) {
        file = __openLog(LOGLEVEL_MAX, true); // try to log in "messages" if custom log files cannot be opened
    }

    tmp.strftime_P(PSTR("%FT%TZ"), now);
    tmp.print(F(" ["));
    tmp.print(getLogLevelAsString(logLevel));
    tmp.print(F("] "));
    tmp.vprintf_P(message, arg);
    tmp.println();

    if (file) {
        file.print(tmp);
        if (logLevel == LOGLEVEL_ERROR || logLevel == LOGLEVEL_WARNING) {
            file.flush();
        }
    }
    else {
        __LDBG_printf("Cannot append to log file %s", _getLogFilename(logLevel).c_str());
    }

#if DEBUG
        DEBUG_OUTPUT.print(tmp);
#elif LOGGER_SERIAL_OUTPUT
    if (System::Flags::getConfig().is_at_mode_enabled) {
        Serial.print(F("+LOGGER="))
        Serial.print(tmp);
    }
#endif

    _closeLog(file);

#if SYSLOG_SUPPORT
    if (_syslog) {
        __LDBG_print("sending message to syslog");
        switch(logLevel) {
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
            case LOGLEVEL_DEBUG:
                _syslog->setSeverity(SYSLOG_DEBUG);
                _syslog->setFacility(SYSLOG_FACILITY_LOCAL0);
                break;
            case LOGLEVEL_ERROR:
            default:
                _syslog->setSeverity(SYSLOG_ERR);
                _syslog->setFacility(SYSLOG_FACILITY_KERN);
                break;
        }
        _syslog->write(reinterpret_cast<const uint8_t *>(tmp.c_str()), String_rtrim(tmp));
        _syslog->flush();
    }
#endif
}

String Logger::_getLogFilename(LogLevel logLevel)
{
    if (isExtraFileEnabled(logLevel)) {
        switch(logLevel) {
            case LOGLEVEL_DEBUG:
                return FSPGM(logger_filename_debug, "/.logs/debug");
            case LOGLEVEL_ERROR:
                return FSPGM(logger_filename_error, "/.logs/error");
            case LOGLEVEL_SECURITY:
                return FSPGM(logger_filename_security, "/.logs/security");
            case LOGLEVEL_WARNING:
            case LOGLEVEL_NOTICE:
                return FSPGM(logger_filename_warning, "/.logs/warning");
            default:
                break;
        }
    }
    return FSPGM(logger_filename_messags, "/.logs/messages");
}

File Logger::__openLog(LogLevel logLevel, bool write)
{
    auto fileName = _getLogFilename(logLevel);
    __LDBG_printf("logLevel=%u filename=%s", logLevel, fileName.c_str());
    return SPIFFS.open(fileName, write ? (SPIFFS.exists(fileName) ? fs::FileOpenMode::append : fs::FileOpenMode::write) : fs::FileOpenMode::read);
}

void Logger::__rotate(LogLevel logLevel)
{
    __LDBG_printf("rotate=%u", logLevel);
    _closeLog(__openLog(logLevel, true));
}

String Logger::_getBackupFilename(String filename, int num)
{
    if (num > 0) {
        filename += '.';
        filename += String(num);
    }
    return filename + F(".bak");
}

void Logger::_closeLog(File file)
{
#if LOGGER_MAX_FILESIZE
    if (file.size() >= LOGGER_MAX_FILESIZE) {
        String filename = file.fullName();
#if LOGGER_MAX_BACKUP_FILES
        // rotation enabled
        String backFilename;
        int i;
        for(i = 0; i < LOGGER_MAX_BACKUP_FILES; i++) {
            backFilename = _getBackupFilename(filename, i);
            if (!SPIFFS.exists(backFilename)) { // available?
                __LDBG_printf("rotating num=%u max=%u", i, LOGGER_MAX_BACKUP_FILES);
                SPIFFS.remove(_getBackupFilename(filename, i + 1)); // delete next logfile to keep rotating
                break;
            }
        }
        if (i == LOGGER_MAX_BACKUP_FILES) { // max. rotations reached, restarting with 0
            __LDBG_printf("restarting rotation num=0 max=%u", LOGGER_MAX_BACKUP_FILES);
            backFilename = _getBackupFilename(filename, 0);
            SPIFFS.remove(backFilename);
            SPIFFS.remove(_getBackupFilename(filename, 1));
        }
#else
        // rotation enabled disabled
        backFilename = _getBackupFilename(filename, 0);
        SPIFFS.remove(backFilename);
#endif
        __LDBG_printf("renaming %s to %s", filename.c_str(), backFilename.c_str());
        file.close();
        SPIFFS.rename(filename, backFilename);
        return;
    }
#endif
    __LDBG_printf("filename=%s", file.fullName());
    file.close();
}

#endif
