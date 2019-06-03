/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include "SyslogFile.h"

SyslogFile::SyslogFile(SyslogParameter& parameter, const String filename, size_t maxSize, uint16_t maxRotate) : Syslog(parameter) {
    _filename = filename;
    _maxSize = maxSize;
    _maxRotate = maxRotate;
}

void SyslogFile::addHeader(String& buffer) {
    _addTimestamp(buffer, PSTR(SYSLOG_FILE_TIMESTAMP_FORMAT));
    _addParameter(buffer, _parameter.getHostname());
    const auto &appName = _parameter.getAppName();
    if (!appName.empty()) {
        buffer += appName;
        const auto &processId = _parameter.getProcessId();
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

    auto logFile = SPIFFS.open(_filename, "a+"); // TODO "a+" required to get the file size with size() ?
    if (logFile) {
        if (_maxSize && (length + logFile.size() + 1) >= _maxSize) {
            logFile.close();
            _rotateLogfile(_filename, _maxRotate);
            logFile = SPIFFS.open(_filename, "a"); // if the rotation fails, just append to the existing file
        }
		if (logFile) {
            auto written = logFile.write((const uint8_t *)message, length);
            written += logFile.write('\n');
            logFile.close();

            callback(written == length + 1);
		}
    } else {
        callback(false);
    }
}

bool SyslogFile::canSend() {
    return true;
}

void SyslogFile::_rotateLogfile(const String filename, uint16_t maxRotate) {

	for (uint16_t num = maxRotate; num >= 1; num--) {
		PrintString from, to;

		if (num == 1) {
			from = filename.c_str();
		} else {
            from.printf_P("%s.%u", filename.c_str(), num - 1);
		}
		to.printf_P("%s.%u", filename.c_str(), num);

#if DEBUG
		int renameResult = -1;
#endif
        if (SPIFFS.exists(from)) {
			if (SPIFFS.exists(to)) {
				SPIFFS.remove(to);
			}
#if DEBUG
			renameResult =
#endif
			SPIFFS.rename(from, to);
		}
		if_debug_printf_P(PSTR("rename = %d: %s => %s\n"), renameResult, from.c_str(), to.c_str());
	}
}
