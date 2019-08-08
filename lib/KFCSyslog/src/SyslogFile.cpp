/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogFile.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogFile::SyslogFile(SyslogParameter &parameter, const String &filename, size_t maxSize, uint16_t maxRotate) : Syslog(parameter) {
    _filename = filename;
    _maxSize = maxSize;
    _maxRotate = maxRotate;
}

void SyslogFile::addHeader(String& buffer) {
    _addTimestamp(buffer, PSTR(SYSLOG_FILE_TIMESTAMP_FORMAT));
    _addParameter(buffer, _parameter.getHostname());
    const auto &appName = _parameter.getAppName();
    if (!appName.length()) {
        buffer += appName;
        const auto &processId = _parameter.getProcessId();
        if (!processId.length()) {
            buffer += '[';
            buffer += processId;
            buffer += ']';
        }
        buffer += ':';
        buffer += ' ';
    }
}

void SyslogFile::transmit(const String &message, Callback_t callback) {
    _debug_printf_P(PSTR("SyslogFile::transmit '%s' length %d\n"), message.c_str(), message.length());

    auto logFile = SPIFFS.open(_filename, "a+"); // TODO "a+" required to get the file size with size() ?
    if (logFile) {
        if (_maxSize && (message.length() + logFile.size() + 1) >= _maxSize) {
            logFile.close();
            _rotateLogfile(_filename, _maxRotate);
            logFile = SPIFFS.open(_filename, "a"); // if the rotation fails, just append to the existing file
        }
		if (logFile) {
            auto written = logFile.write((const uint8_t *)message.c_str(), message.length());
            written += logFile.write('\n');
            logFile.close();

            callback(written == message.length() + 1);
		}
    } else {
        callback(false);
    }
}

bool SyslogFile::canSend() const {
    return true;
}

void SyslogFile::_rotateLogfile(const String &filename, uint16_t maxRotate) {

	for (uint16_t num = maxRotate; num >= 1; num--) {
		String from, to;

		if (num == 1) {
			from = filename; //.c_str();
		} else {
            from += filename;
            from += '.';
            from += String(num - 1);
		}
        to = filename;
        to += '.';
        to += String(num);

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
		_debug_printf_P(PSTR("rename = %d: %s => %s\n"), renameResult, from.c_str(), to.c_str());
	}
}
