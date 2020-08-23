/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include "Syslog.h"
#include "SyslogQueue.h"
#include "SyslogFile.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogFile::SyslogFile(SyslogParameter &&parameter, SyslogQueue &queue, const String &filename, size_t maxSize, uint16_t maxRotate) :
    Syslog(std::move(parameter), queue),
    _filename(filename),
    _maxSize(maxSize),
    _maxRotate(maxRotate)
{
}

String SyslogFile::_getHeader()
{
    PrintString buffer;
    _addTimestamp(buffer, 0, PSTR(SYSLOG_FILE_TIMESTAMP_FORMAT));
    _addParameter(buffer, _parameter.getHostname());
    auto appName = _parameter.getAppName();
    if (appName) {
        buffer += appName;
        auto processId = _parameter.getProcessId();
        if (processId) {
            buffer += '[';
            buffer += processId;
            buffer += ']';
        }
        buffer += ':';
        buffer += ' ';
    }
    return buffer;
}

bool SyslogFile::setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port)
{
    return false;
}

String SyslogFile::getHostname() const
{
    return String(F("file://")) + _filename;
}

uint16_t SyslogFile::getPort() const
{
    return 0;
}

void SyslogFile::transmit(const SyslogQueueItem &item)
{
    auto &message = item.getMessage();
#if DEBUG_SYSLOG
    if (!String_startsWith(message, F("::transmit '"))) {
        __LDBG_printf("::transmit id=%u msg=%s", item.getId(), message.c_str());
    }
#endif

    auto logFile = SPIFFS.open(_filename, fs::FileOpenMode::appendplus);
    if (logFile) {
        String header = _getHeader();
        size_t msgLen = message.length() + header.length() + 1;
        if (_maxSize && (msgLen + logFile.size()) >= _maxSize) {
            logFile.close();
            _rotateLogfile(_filename, _maxRotate);
            logFile = SPIFFS.open(_filename, fs::FileOpenMode::append); // if the rotation fails, just append to the existing file
        }
		if (logFile) {
            auto written = logFile.print(header);
            written += logFile.print(message);
            written += logFile.write('\n');
            logFile.close();

            _queue.remove(item.getId(), written == msgLen);
            return;
		}
    }
    _queue.remove(item.getId(), false);
}

bool SyslogFile::canSend() const
{
    return true;
}

bool SyslogFile::isSending()
{
    return false;
}

void SyslogFile::_rotateLogfile(const String &filename, uint16_t maxRotate)
{
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

#if DEBUG_SYSLOG
		int renameResult = -1;
#endif
        if (SPIFFS.exists(from)) {
			if (SPIFFS.exists(to)) {
				SPIFFS.remove(to);
			}
#if DEBUG_SYSLOG
			renameResult =
#endif
			SPIFFS.rename(from, to);
		}
		_debug_printf_P(PSTR("rename = %d: %s => %s\n"), renameResult, from.c_str(), to.c_str());
	}
}
