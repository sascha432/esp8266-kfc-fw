/**
 * Author: sascha_lammers@gmx.de
 */

#if LOGGER

#include "logger.h"
#include <time.h>
#include <algorithm>
#include <stl_ext/utility.h>
#include <misc.h>
#include "kfc_fw_config.h"
#include "../src/plugins/plugins.h"

#if DEBUG_LOGGER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#undef DEBUG

using KFCConfigurationClasses::System;

Logger _logger;

Logger::Logger() :
    _logLevel(Level::NOTICE),
    _enabled(LoggerEnum(LoggerEnum::Enum::ERROR)|LoggerEnum(LoggerEnum::Enum::SECURITY))
    #if SYSLOG_SUPPORT
        , _syslog(nullptr)
    #endif
{
}

void Logger::end()
{
    _Timer(_writeTimer).remove();
    if (_logLevel != Level::NONE) {
        _logLevel = Level::NONE;
        _flushQueue();
    }
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
        case Level::NONE:
        default:
            break;
    }
    return F("NONE");
}

void Logger::writeLog(Level logLevel, const char *message, va_list arg)
{
    // check if logging is active
    if (logLevel > _logLevel) {
        return;
    }

    #if ESP32
        // log messages are sent from multiple cores
        MUTEX_LOCK_BLOCK(_lock)
    #endif
    {
        // create item with relative time
        auto item = MemoryQueueType(millis() - _lastFlushTimer, logLevel);

        // message header
        {
            PrintString header;
            auto now = time(nullptr);

            header.strftime_P(PSTR("%FT%TZ"), now);
            header.print(F(" ["));
            header.print(getLevelAsString(logLevel));
            header.print(F("] "));

            item.buffer.resize(item.headerLength = header.length());
            memmove(item.header(), header.c_str(), header.length());
        }

        // message body
        {
            PrintString msg;

            msg.vprintf_P(message, arg);
            msg.rtrim();
            item.buffer.resize(item.headerSize() + msg.length());
            memmove(item.message(), msg.c_str(), msg.length());

            #if SYSLOG_SUPPORT
                // send message to syslog plugin
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
                    _syslog->addMessage(std::move(msg));
                    SyslogPlugin::rearmTimer();
                }
            #endif

        }

        #if LOGGER_SERIAL_OUTPUT
            // send to serial output
            if (at_mode_enabled()) {
                Serial.print(F("+LOGGER="));
                Serial.write(item.header(), item.headerSize());
                Serial.write(item.message(), item.messageSize());
                Serial.println();
            }
        #else
            // send to debug log
            DebugContext_prefix(DEBUG_OUTPUT.println(DEBUG_OUTPUT.write(item.message(), item.messageSize()) && DEBUG_OUTPUT.println()));
        #endif

        // place item in queue
        __LDBG_printf("lvl=%x t=%u s=%u", item.logLevel, item.millis, item.buffer.size());
        QueueSizeType size;
        MUTEX_LOCK_BLOCK(_queueLock) {
            size = QueueSizeType::get(_queue);
            size.add(item); // add new item to see if it fits
            if (size.size() > kQueueMaxSize) {
                __DBG_printf_E("dropped item=%u size=%u num=%u", item.buffer.size() + sizeof(item), size.size(), size.num());
            }
            else {
                _queue.emplace_back(std::move(item));
            }
        }

        // execute writes in main loop
        for(;;) {
            auto delay = item.writeDelay();
            if (size.size() > kQueueFlushSize) {
                delay = 2; // flush queue immediately
            }
            if (_writeTimer) {
                // do not delay any pending writes
                if (delay >= _writeTimer->getShortInterval()) {
                    break;
                }
            }
            _Timer(_writeTimer).add(Event::milliseconds(delay - 1), false, [this](Event::CallbackTimerPtr) {
                this->_flushQueue();
            });
            break;
        }
    }
}

void Logger::_flushQueue()
{
    #if ESP32
        // log messages are sent from multiple cores
        MUTEX_LOCK_BLOCK(_flushLock)
    #endif
    {
        MemoryQueueTypeList tmp;
        MUTEX_LOCK_BLOCK(_queueLock) {
            tmp = std::move(_queue);
        }
        // sort by loglevel and time
        tmp.sort([](const MemoryQueueType &a, const MemoryQueueType &b) {
            if (a.logLevel == b.logLevel) {
                return a.millis < b.millis;
            }
            return a.logLevel < b.logLevel;
        });

        #if DEBUG_LOGGER
            auto size = QueueSizeType::get(tmp);
            __DBG_printf("queue size=%u num=%u", size.size(), size.num());
        #endif

        uint32_t start = millis();
        Level last = Level::NONE;
        File file;
        for(auto iterator = tmp.begin(); iterator != tmp.end(); iterator = tmp.begin()) {
            auto &item = *iterator;

            __LDBG_printf("lvl=%x t=%u s=%u", item.logLevel, item.millis, item.buffer.size());
            if (last != item.logLevel || !file) {
                // store as last
                last = item.logLevel;

                // check if the filenames match if the file is open
                for(;;) {
                    if (file) {
                        if (strcmp_P(file.fullName(), (PGM_P)_getLogFilename(last)) == 0) {
                            break; // already open
                        }
                    }
                    _closeLog(file);

                    // open new log file
                    file = __openLog(item.logLevel, true);
                    if (!file) {
                        file = __openLog(Level::MAX, true);
                    }
                    break;
                }
            }

            // write item
            file.write(item.buffer.data(), item.buffer.size());
            file.println();

            // remote item from list
            tmp.erase(iterator);

            optimistic_yield(10000);

            // check how much time we spend
            auto dur = get_time_since(start, millis());
            if (dur > kQueueMaxTimeout) {
                _closeLog(file);

                __LDBG_printf_E("timeout=%u re-adding=%u", dur, tmp.size());

                // re-add all items that are left
                MUTEX_LOCK_BLOCK(_queueLock) {
                    auto size = QueueSizeType::get(_queue);
                    for(auto &item: tmp) {
                        size.add(item);
                        if (size.size() > kQueueMaxSize) {
                            size = QueueSizeType::get(tmp);
                            __DBG_printf_E("dropped num=%u messages size=%u", size.num(), size.size());
                            break;
                        }
                        _queue.emplace_back(std::move(item));
                    }
                }
                break;
            }
        }

        _closeLog(file);
        _lastFlushTimer = millis();

        __LDBG_printf("flush time=%u", get_time_since(start, millis()));
    }
}

const __FlashStringHelper *Logger::_getLogFilename(Level logLevel)
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
    return FSPGM(logger_filename_messages, "/.logs/messages");
}

void Logger::_closeLog(File file)
{
    __LDBG_printf("_closeLog file=%u", !!file);
    if (!file) {
        return;
    }
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
