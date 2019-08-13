/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if LOGGER

#include <Arduino_compat.h>
#include <StreamString.h>
#include "logger.h"

class LoggerStream : public StreamString {
public:
    LoggerStream() : LoggerStream(LOGLEVEL_NOTICE) {
    }
    LoggerStream(LogLevel logLevel) : _logLevel(logLevel) {
    }

    void flush() override {
        auto str = this->c_str();
        if (*str) {
            _logger.log(_logLevel, str);
            remove(0, length());
        }
    }

private:
    LogLevel _logLevel;
};

#endif
