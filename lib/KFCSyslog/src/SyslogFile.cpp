/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogQueue.h"
#include "SyslogFile.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogFile::SyslogFile(const char *hostname, SyslogQueue *queue, const String &filename, size_t maxSize, uint16_t maxRotate) :
    Syslog(hostname, queue),
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
    buffer += F(SYSLOG_APPNAME ": ");
    return buffer;
}

void SyslogFile::transmit(const SyslogQueueItem &item)
{
    auto &message = item.getMessage();
    #if DEBUG_SYSLOG
        if (!message.startsWith(F("::transmit '"))) {
            __LDBG_printf("::transmit id=%u msg=%s", item.getId(), message.c_str());
        }
    #endif

    auto logFile = KFCFS.open(_filename, fs::FileOpenMode::appendplus);
    if (logFile) {
        String header = _getHeader();
        size_t msgLen = message.length() + header.length() + 1;
        if (_maxSize && (msgLen + logFile.size()) >= _maxSize) {
            logFile.close();
            _rotateLogfile(_filename, _maxRotate);
            logFile = KFCFS.open(_filename, fs::FileOpenMode::append); // if the rotation fails, just append to the existing file
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

uint32_t SyslogFile::getState(StateType state)
{
    switch (state) {
    case StateType::CAN_SEND:
        return true;
    default:
        break;
    }
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
        if (KFCFS.exists(from)) {
			if (KFCFS.exists(to)) {
				KFCFS.remove(to);
			}
#if DEBUG_SYSLOG
			renameResult =
#endif
			KFCFS.rename(from, to);
		}
		__LDBG_printf("rename = %d: %s => %s", renameResult, from.c_str(), to.c_str());
	}
}
