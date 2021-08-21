/**
 * Author: sascha_lammers@gmx.de
 */

#if LOGGER

#include "logger.h"
#include <time.h>
#include <vector>
#include <misc.h>
#include "kfc_fw_config.h"
#include "../src/plugins/plugins.h"

#if DEBUG_LOGGER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if DEBUG
#define ___DEBUG 1
#else
#define ___DEBUG 0
#endif
#undef DEBUG

using KFCConfigurationClasses::System;

Logger _logger;

Logger::Logger() :
    _logLevel(Level::NOTICE),
    #if ___DEBUG
        _enabled(EnumHelper::Bitset::all(Level::ERROR, Level::WARNING, Level::SECURITY))
    #else
        _enabled(EnumHelper::Bitset::all(Level::ERROR, Level::SECURITY))
    #endif
    #if SYSLOG_SUPPORT
        , _syslog(nullptr)
    #endif
{
}


const __FlashStringHelper *Logger::getLevelAsString(Level logLevel)
{
    switch(logLevel) {
        case Level::ERROR:
            return F("ERROR");
        case Level::SECURITY:
            return F("SECURITY");
        case Level::WARNING:
            return F("WARNING");
        case Level::NOTICE:
            return F("NOTICE");
        case Level::DEBUG:
            return F("DEBUG");
        case Level::ANY:
            return F("error|security|warning|notice|debug");
        default:
            break;
    }
    return F("");
}

void Logger::writeLog(Level logLevel, const char *message, va_list arg)
{
    if (logLevel > _logLevel) {
        return;
    }

    PrintString msg;
    {
        PrintString header;
        auto now = time(nullptr);
        auto file = __openLog(logLevel, true);
        if (!file) {
            file = __openLog(Level::MAX, true); // try to log in "messages" if custom log files cannot be opened
        }

        header.strftime_P(PSTR("%FT%TZ"), now);
        header.print(F(" ["));
        header.print(getLevelAsString(logLevel));
        header.print(F("] "));

        msg.vprintf_P(message, arg);

        msg.rtrim();

        if (file) {
            file.print(header);
            file.println(msg);
            if (logLevel <= Level::WARNING) {
                file.flush();
            }
        }
        #if DEBUG_LOGGER
            else {
                auto filename = _getLogFilename(logLevel);
                __LDBG_printf("Cannot append to log file %s", filename.c_str());
            }
        #endif

        _closeLog(file);

        #if LOGGER_SERIAL_OUTPUT
            if (at_mode_enabled()) {
                Serial.print(F("+LOGGER="));
                Serial.print(header);
                Serial.println(msg);
            }
            #if ___DEBUG
            else {
                DebugContext_prefix(DEBUG_OUTPUT.println(msg));
            }
            #endif
        #else
            DebugContext_prefix(DEBUG_OUTPUT.println(msg));
        #endif
    }

    #if SYSLOG_SUPPORT
        if (_syslog) {
            __LDBG_printf("sending message to syslog level=%u", logLevel);
            switch(logLevel) {
                case Level::SECURITY:
                    _syslog->setSeverity(SYSLOG_WARN);
                    _syslog->setFacility(SYSLOG_FACILITY_SECURE);
                    break;
                case Level::WARNING:
                    _syslog->setSeverity(SYSLOG_WARN);
                    _syslog->setFacility(SYSLOG_FACILITY_KERN);
                    break;
                case Level::NOTICE:
                    _syslog->setSeverity(SYSLOG_NOTICE);
                    _syslog->setFacility(SYSLOG_FACILITY_KERN);
                    break;
                case Level::DEBUG:
                    _syslog->setSeverity(SYSLOG_DEBUG);
                    _syslog->setFacility(SYSLOG_FACILITY_LOCAL0);
                    break;
                case Level::ERROR:
                default:
                    _syslog->setSeverity(SYSLOG_ERR);
                    _syslog->setFacility(SYSLOG_FACILITY_KERN);
                    break;
            }
            _syslog->write(reinterpret_cast<const uint8_t *>(msg.c_str()), msg.length());
            msg = String();
            _syslog->flush();
        }
    #endif
}

String Logger::_getLogFilename(Level logLevel)
{
    if (isExtraFileEnabled(logLevel)) {
        switch(logLevel) {
            case Level::DEBUG:
                return FSPGM(logger_filename_debug, "/.logs/debug");
            case Level::ERROR:
                return FSPGM(logger_filename_error, "/.logs/error");
            case Level::SECURITY:
                return FSPGM(logger_filename_security, "/.logs/security");
            case Level::WARNING:
            case Level::NOTICE:
                return FSPGM(logger_filename_warning, "/.logs/warning");
            default:
                break;
        }
    }
    return FSPGM(logger_filename_messags, "/.logs/messages");
}

void Logger::_closeLog(File file)
{
    String filename = file.fullName();
    #if LOGGER_MAX_FILESIZE
        if (file.size() >= LOGGER_MAX_FILESIZE) {
            #if LOGGER_MAX_BACKUP_FILES
                // rotation enabled
                String backFilename;
                int i;
                for(i = 0; i < LOGGER_MAX_BACKUP_FILES; i++) {
                    backFilename = _getBackupFilename(filename, i);
                    if (!KFCFS.exists(backFilename)) { // available?
                        __LDBG_printf("rotating num=%u max=%u", i, LOGGER_MAX_BACKUP_FILES);
                        KFCFS.remove(_getBackupFilename(filename, i + 1)); // delete next logfile to keep rotating
                        break;
                    }
                }
                if (i == LOGGER_MAX_BACKUP_FILES) { // max. rotations reached, restarting with 0
                    __LDBG_printf("restarting rotation num=0 max=%u", LOGGER_MAX_BACKUP_FILES);
                    backFilename = _getBackupFilename(filename, 0);
                    KFCFS.remove(backFilename);
                    KFCFS.remove(_getBackupFilename(filename, 1));
                }
            #else
                // rotation enabled disabled
                backFilename = _getBackupFilename(filename, 0);
                KFCFS.remove(backFilename);
            #endif
            __LDBG_printf("renaming %s to %s", filename.c_str(), backFilename.c_str());
            file.close();
            KFCFS.rename(filename, backFilename);
            return;
        }
    #endif
    __LDBG_printf("close file=%s", filename.c_str());
    file.close();
}

#endif
