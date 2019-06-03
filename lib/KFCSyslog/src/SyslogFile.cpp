/**
 * Author: sascha_lammers@gmx.de
 */

#include "SyslogFile.h"

SyslogFile::SyslogFile(SyslogParameter& parameter, const String filename, size_t maxSize, uint16_t maxRotate) : Syslog(parameter) {
    _filename = filename;
    _maxSize = maxSize;
    _maxRotate = maxRotate;
}

void SyslogFile::addHeader(String& buffer) {
    _addTimestamp(buffer, PSTR(SYSLOG_FILE_TIMESTAMP_FORMAT));
    _addParameter(buffer, _parameter.getHostname());
    const String& appName = _parameter.getAppName();
    if (!appName.empty()) {
        buffer += appName;
        const String& processId = _parameter.getProcessId();
        if (!processId.empty()) {
            buffer += '[';
            buffer += processId;
            buffer += ']';
        }
        buffer += ':';
        buffer += ' ';
    }
}

void SyslogFile::transmit(const char* message, size_t length, SyslogCallback callback) {
    if_debug_printf_P(PSTR("SyslogFile::transmit '%s' length %d\n"), message, length);

    File logFile = SPIFFS.open(_filename, "a+");
    if (logFile) {
        if (_maxSize && length >= _maxSize) {
            logFile.close();
            _rotateLogfile(_filename, _maxSize, _maxRotate);
            logFile = SPIFFS.open(_filename, "a");
        }

        size_t written = logFile.write((const uint8_t*)message, length);
        written += logFile.write('\n');
        logFile.close();

        callback(written == length + 1);
    } else {
        callback(false);
    }
}

bool SyslogFile::canSend() {
    return true;
}

void SyslogFile::_rotateLogfile(const String filename, size_t maxSize, uint16_t maxRotate) {
    String dirName = F("./");
    int dirEnd = filename.indexOf('/');
    if (dirEnd != -1) {
        dirName = filename.substring(0, dirEnd + 1);
    }

    Dir dir = SPIFFS.openDir(dirName);
}
