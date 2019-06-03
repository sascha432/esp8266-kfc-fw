/**
 * Author: sascha_lammers@gmx.de
 */

#include "SyslogStream.h"
#include <PrintString.h>
#include <time.h>

SyslogFilterItem syslogFilterFacilityItems[] = {
    {PSTR("kern"), SYSLOG_FACILITY_KERN},
    {PSTR("user"), SYSLOG_FACILITY_USER},
    {PSTR("user"), SYSLOG_FACILITY_USER},
    {PSTR("mail"), SYSLOG_FACILITY_MAIL},
    {PSTR("daemon"), SYSLOG_FACILITY_DAEMON},
    {PSTR("secure"), SYSLOG_FACILITY_SECURE},
    {PSTR("syslog"), SYSLOG_FACILITY_SYSLOG},
    {PSTR("ntp"), SYSLOG_FACILITY_NTP},
    {PSTR("local0"), SYSLOG_FACILITY_LOCAL0},
    {PSTR("local1"), SYSLOG_FACILITY_LOCAL1},
    {PSTR("local2"), SYSLOG_FACILITY_LOCAL2},
    {PSTR("local3"), SYSLOG_FACILITY_LOCAL3},
    {PSTR("local4"), SYSLOG_FACILITY_LOCAL4},
    {PSTR("local5"), SYSLOG_FACILITY_LOCAL5},
    {PSTR("local6"), SYSLOG_FACILITY_LOCAL6},
    {PSTR("local7"), SYSLOG_FACILITY_LOCAL7},
    {nullptr, 0xff},
};

SyslogFilterItem syslogFilterSeverityItems[] = {
    {PSTR("emerg"), SYSLOG_EMERG},
    {PSTR("emergency"), SYSLOG_EMERG},
    {PSTR("alert"), SYSLOG_ALERT},
    {PSTR("crit"), SYSLOG_CRIT},
    {PSTR("critical"), SYSLOG_CRIT},
    {PSTR("err"), SYSLOG_ERR},
    {PSTR("error"), SYSLOG_ERR},
    {PSTR("warn"), SYSLOG_WARN},
    {PSTR("warning"), SYSLOG_WARN},
    {PSTR("notice"), SYSLOG_NOTICE},
    {PSTR("info"), SYSLOG_INFO},
    {PSTR("debug"), SYSLOG_DEBUG},
    {nullptr, 0xff},
};

bool _syslogIsNumeric(const String& value) {
    const char* ptr = value.c_str();
    while (*ptr) {
        if (!isdigit(*ptr++)) {
            return false;
        }
    }
    return true;
}

SyslogFacility _syslogFacilityToInt(const String& str) {
    if (str.length() != 0 && str != F("*")) {
        if (_syslogIsNumeric(str)) {
            return (SyslogFacility)str.toInt();
        }
        for (uint8_t i = 0; syslogFilterFacilityItems[i].value != 0xff; i++) {
            if (strcasecmp_P(str.c_str(), syslogFilterFacilityItems[i].name) == 0) {
                return (SyslogFacility)syslogFilterFacilityItems[i].value;
            }
        }
    }
    return SYSLOG_FACILITY_ANY;
}

SyslogSeverity _syslogSeverityToInt(const String& str) {
    if (str.length() != 0 && str != F("*")) {
        if (_syslogIsNumeric(str)) {
            return (SyslogSeverity)str.toInt();
        }
        for (uint8_t i = 0; syslogFilterSeverityItems[i].value != 0xff; i++) {
            if (strcasecmp_P(str.c_str(), syslogFilterSeverityItems[i].name) == 0) {
                return (SyslogSeverity)syslogFilterSeverityItems[i].value;
            }
        }
    }
    return SYSLOG_SEVERITY_ANY;
}

SyslogQueue::SyslogQueue() {
	_item.id = 0;
}

uint32_t SyslogQueue::add(const String& message, Syslog* syslog) {
    _item.message = message;
    _item.id = 1;
    return 1;
}

void SyslogQueue::remove(SyslogMemoryQueueItem *item, bool success) {
    item->id = 0;
    item->message = String();
}

//SyslogMemoryQueueItem* SyslogQueue::get(Syslog* syslog) {
//    if (_item.id) {
//        return &_item;
//    }
//    return nullptr;
//}

size_t SyslogQueue::size() const {
    return _item.id ? 1 : 0;
}

inline SyslogQueueIterator SyslogQueue::begin() {
	return SyslogQueueIterator(0, this);
}

inline SyslogQueueIterator SyslogQueue::end() {
	return SyslogQueueIterator(_item.id, this);
}

inline SyslogMemoryQueueItem * SyslogQueue::getItem(uint32_t index) {
	return &_item;
}

void SyslogQueue::cleanUp() {
}

SyslogMemoryQueue::SyslogMemoryQueue(uint32_t maxSize) {
    _maxSize = maxSize;
    _curSize = 0;
    _id = 0;
	_count = 0;
}

uint32_t SyslogMemoryQueue::add(const String& message, Syslog* syslog) {
    size_t size = message.length() + SYSLOG_MEMORY_QUEUE_ITEM_SIZE;
    if (size + _curSize > _maxSize) {
        return 0;  // no space left, discard message
    }
    _items.push_back({++_id, message, 0, syslog});
    _curSize += size;
	_count++;
    return _id;
}

void SyslogMemoryQueue::remove(SyslogMemoryQueueItem *item, bool success) {
	if (item->id) {
		item->id = 0;
		_count--;
    }
}

//SyslogMemoryQueueItem* SyslogMemoryQueue::get(Syslog* syslog) {
//    if (_items.size()) {
//        if (!syslog) {
//            return &_items.front();
//        }
//        for (auto item = _items.begin(); item != _items.end(); ++item) {
//            if (item->syslog == syslog) {
//                return &*item;
//            }
//        }
//    }
//    return nullptr;
//}

size_t SyslogMemoryQueue::size() const {
    return _count;
}

inline SyslogQueueIterator SyslogMemoryQueue::begin() {
	return SyslogQueueIterator(0, this);
}

inline SyslogQueueIterator SyslogMemoryQueue::end() {
	return SyslogQueueIterator(_items.size(), this);
}

inline SyslogMemoryQueueItem * SyslogMemoryQueue::getItem(uint32_t index) {
	return &_items[index];
}

void SyslogMemoryQueue::cleanUp() {
	auto newEnd = std::remove_if(_items.begin(), _items.end(), [this](SyslogMemoryQueueItem& item) {
		if (item.id == 0) {
			_curSize -= item.message.length() + SYSLOG_MEMORY_QUEUE_ITEM_SIZE;
			return true;
		}
		return false;
	});
	_items.erase(newEnd, _items.end());
}

SyslogParameter::SyslogParameter() {
    _facility = SYSLOG_FACILITY_KERN;
    _severity = SYSLOG_ERR;
}

SyslogParameter::SyslogParameter(const char *hostname, const char *appName, const char *processId)
{
	_hostname = hostname;
	_appName = appName;
	if (processId) {
		_processId = processId;
	}
}

SyslogParameter::SyslogParameter(const String hostname, const String appName)
{
	_hostname = hostname;
	_appName = appName;
}

SyslogParameter::SyslogParameter(const String hostname, const String appName, const String processId)
{
	_hostname = hostname;
	_appName = appName;
	_processId = processId;
}

void SyslogParameter::setFacility(SyslogFacility facility) {
    _facility = facility;
}

SyslogFacility SyslogParameter::getFacility() {
    return _facility;
}

void SyslogParameter::setSeverity(SyslogSeverity severity) {
    _severity = severity;
}

SyslogSeverity SyslogParameter::getSeverity() {
    return _severity;
}

void SyslogParameter::setAppName(const char* appName) {
    _appName = appName;
}

void SyslogParameter::setAppName(const String appName)
{
	_appName = appName;
}

const String& SyslogParameter::getAppName() {
    return _appName;
}

void SyslogParameter::setHostname(const char* hostname) {
    _hostname = hostname;
}

void SyslogParameter::setHostname(const String hostname)
{
	_hostname = hostname;
}

const String& SyslogParameter::getHostname() {
    return _hostname;
}

void SyslogParameter::setProcessId(const char* processId) {
    _processId = processId;
}

void SyslogParameter::setProcessId(const String processId)
{
	_processId = processId;
}

const String& SyslogParameter::getProcessId() {
    return _processId;
}

Syslog::Syslog(SyslogParameter &parameter) : _parameter(parameter) {
}

void Syslog::transmit(const char* message, size_t length, SyslogCallback callback) {
    callback(true);
}

void Syslog::_addTimestamp(String& buffer, PGM_P format) {
    time_t now = time(nullptr);
    char buf2[24];

    if (now > 1300000000L) {  // valid date?
        // timezone_strftime_P(buffer, sizeof(buffer), PSTR("%FT%TZ"), timezone_localtime(&now));
        struct tm tm;
        if (localtime_s(&tm, &now)) {
            buffer += '-';
        } else {
            strftime(buf2, sizeof(buf2), PSTR(SYSLOG_TIMESTAMP_FORMAT), &tm);
            buffer += buf2;
        }
    } else {
        buffer += '-';
    }
    buffer += ' ';
}

void Syslog::_addParameter(String& buffer, const String& value) {
    if (value.empty()) {
        buffer += '-';
    } else {
        buffer += value;
    }
    buffer += ' ';
}

/*

   The syslog message has the following ABNF [RFC5234] definition:

      SYSLOG-MSG      = HEADER SP STRUCTURED-DATA [SP MSG]

      HEADER          = PRI VERSION SP TIMESTAMP SP HOSTNAME
                        SP APP-NAME SP PROCID SP MSGID
      PRI             = "<" PRIVAL ">"
      PRIVAL          = 1*3DIGIT ; range 0 .. 191
      VERSION         = NONZERO-DIGIT 0*2DIGIT
      HOSTNAME        = NILVALUE / 1*255PRINTUSASCII

      APP-NAME        = NILVALUE / 1*48PRINTUSASCII
      PROCID          = NILVALUE / 1*128PRINTUSASCII
      MSGID           = NILVALUE / 1*32PRINTUSASCII

      TIMESTAMP       = NILVALUE / FULL-DATE "T" FULL-TIME
      FULL-DATE       = DATE-FULLYEAR "-" DATE-MONTH "-" DATE-MDAY
      DATE-FULLYEAR   = 4DIGIT
      DATE-MONTH      = 2DIGIT  ; 01-12
      DATE-MDAY       = 2DIGIT  ; 01-28, 01-29, 01-30, 01-31 based on
                                ; month/year
      FULL-TIME       = PARTIAL-TIME TIME-OFFSET
      PARTIAL-TIME    = TIME-HOUR ":" TIME-MINUTE ":" TIME-SECOND
                        [TIME-SECFRAC]
      TIME-HOUR       = 2DIGIT  ; 00-23
      TIME-MINUTE     = 2DIGIT  ; 00-59
      TIME-SECOND     = 2DIGIT  ; 00-59
      TIME-SECFRAC    = "." 1*6DIGIT
      TIME-OFFSET     = "Z" / TIME-NUMOFFSET
      TIME-NUMOFFSET  = ("+" / "-") TIME-HOUR ":" TIME-MINUTE

      STRUCTURED-DATA = NILVALUE / 1*SD-ELEMENT
      SD-ELEMENT      = "[" SD-ID *(SP SD-PARAM) "]"
      SD-PARAM        = PARAM-NAME "=" %d34 PARAM-VALUE %d34
      SD-ID           = SD-NAME
      PARAM-NAME      = SD-NAME
      PARAM-VALUE     = UTF-8-STRING ; characters '"', '\' and
                                     ; ']' MUST be escaped.
      SD-NAME         = 1*32PRINTUSASCII
                        ; except '=', SP, ']', %d34 (")

      MSG             = MSG-ANY / MSG-UTF8
      MSG-ANY         = *OCTET ; not starting with BOM
      MSG-UTF8        = BOM UTF-8-STRING
      BOM             = %xEF.BB.BF

      UTF-8-STRING    = *OCTET ; UTF-8 string as specified
                        ; in RFC 3629

      OCTET           = %d00-255
      SP              = %d32
      PRINTUSASCII    = %d33-126
      NONZERO-DIGIT   = %d49-57
      DIGIT           = %d48 / NONZERO-DIGIT
      NILVALUE        = "-"
*/

void Syslog::addHeader(String& buffer) {
#if USE_RFC5424

    buffer += '<';
    buffer += String(_parameter.getFacility() << 3 | _parameter.getSeverity());
    buffer += '>';
    _addParameter(buffer, F(SYSLOG_VERSION));
    _addTimestamp(buffer);
    _addParameter(buffer, _parameter.getHostname());
    _addParameter(buffer, _parameter.getAppName());
    _addParameter(buffer, _parameter.getProcessId());
    buffer += F("- - ");  // NIL values for message id & structured data

#else

    // format used by "rsyslog/stable,now 8.24.0-1 armhf"
    // <PRIVAL> SP TIMESTAMP SP HOSTNAME SP APPNAME ["[" PROCESSID "]"] ":" SP MESSAGE
    // the process id part is optional

    buffer += '<';
    buffer += String(_parameter.getFacility() << 3 | _parameter.getSeverity());
    buffer += '>';
    _addTimestamp(buffer);
    _addParameter(buffer, _parameter.getHostname());
    if (!_parameter.getAppName().empty()) {
        buffer += _parameter.getAppName();
        if (!_parameter.getProcessId().empty()) {
            buffer += '[';
            buffer += _parameter.getProcessId();
            buffer += ']';
        }
        buffer += ':';
        buffer += ' ';
    }
#endif
}

bool Syslog::canSend()
{
	return WiFi.isConnected();
}

SyslogUDP::SyslogUDP(SyslogParameter& parameter, const char* host, uint16_t port) : Syslog(parameter) {
    _host = host;
    _port = port;
}

void SyslogUDP::transmit(const char* message, size_t length, SyslogCallback callback) {

	if_debug_printf_P(PSTR("SyslogUDP::transmit '%s' length %d\n"), message, length);

    bool success = _udp.beginPacket(_host.c_str(), _port) && _udp.write((const uint8_t*)message, length) == length && _udp.endPacket();
    callback(success);
}

SyslogTCP::SyslogTCP(SyslogParameter& parameter, const char* host, uint16_t port, bool useTLS) : Syslog(parameter) {
    _host = host;
    _port = port;
    _useTLS = useTLS;
}

SyslogFile::SyslogFile(SyslogParameter& parameter, const String filename, uint32_t maxSize, uint16_t maxRotate) : Syslog(parameter) {
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
		size_t written = logFile.write((const uint8_t*)message, length);
        written += logFile.write('\n');
        logFile.close();

		callback(written == length + 1);
	} else {
		callback(false);
	}
}

bool SyslogFile::canSend()
{
	return true;
}

SyslogStream::SyslogStream(SyslogFilter* filter, SyslogQueue* queue) : _parameter(filter->getParameter()), _filter(filter), _queue(queue) {
}

SyslogStream::~SyslogStream() {
    if (_queue) {
        delete _queue;
    }
    if (_filter) {
        delete _filter;
    }
}

void SyslogStream::setFacility(SyslogFacility facility) {
    _parameter.setFacility(facility);
}

void SyslogStream::setSeverity(SyslogSeverity severity) {
    _parameter.setSeverity(severity);
}

size_t SyslogStream::write(uint8_t data) {
    _message += data;
    return 1;
}

size_t SyslogStream::write(const uint8_t* buffer, size_t len) {
    auto ptr = buffer;
    while (len--) {
        _message += *ptr++;
    }
    return ptr - buffer;
}

void SyslogStream::flush() {

	//_queue->dump();

    _filter->applyFilters([this](SyslogFileFilterItem& filter) {
        String preparedMessage;
        auto syslog = filter.syslog;
        syslog->addHeader(preparedMessage);
        if (!preparedMessage.empty()) {
            preparedMessage.append(_message);
			_queue->add(preparedMessage, syslog);
        }
    });
    _message = String();
    deliverQueue();

	//_queue->dump();
}

bool SyslogStream::hasQueuedMessages() {
    return !!_queue->size();
}

int SyslogStream::available() {
    return false;
}

int SyslogStream::read() {
    return -1;
}

int SyslogStream::peek() {
    return -1;
}

//void SyslogStream::poll() {
//	if (canSend()) {
//        auto item = _queue->get(nullptr);
//        if (item) {
//            unsigned long start = millis();
//            while (millis() - start < QUEUE_MAX_SEND_TIME) {  // send messages until 20ms have been reached
//                item->syslog->transmit(item->message.c_str(), item->message.length(), [this, item](bool success) { this->transmitCallback(item, success); });
//                delay(QUEUE_MAX_DELAY);
//                auto newItem = _queue->get(nullptr);  // check if the message has been sent already
//                if (!newItem || newItem->id == item->id) {
//                    break;  // nothing left or no new item available
//                }
//                item = newItem;
//            }
//        }
//	}
//}

void SyslogStream::deliverQueue(Syslog * syslog) {
	for (auto _item = _queue->begin(); _item != _queue->end(); ++_item) {
		auto item = *_item;
		if (!syslog || item->syslog == syslog) {
			item->syslog->transmit(item->message, [this, item](bool success) {
				transmitCallback(item, success);
			});
		}

	}
	_queue->cleanUp();
}

void SyslogStream::transmitCallback(SyslogMemoryQueueItem *item, bool success) {
    if (success) {
        _queue->remove(item, true);
    } else {
        if (++item->failureCount > MAX_FAILURES) {
            _queue->remove(item, false);
        }
    }
}

const String SyslogStream::getLevel()
{
	String result;
	uint8_t i;

	auto facility = _parameter.getFacility();
    for (i = 0; syslogFilterFacilityItems[i].value != 0xff; i++) {
        if (syslogFilterFacilityItems[i].value == facility) {
            result += syslogFilterFacilityItems[i].name;
            break;
        }
    }
	result += '.';
	auto severity = _parameter.getSeverity();
	for(i = 0; syslogFilterSeverityItems[i].value != 0xff; i++) {
		if (syslogFilterSeverityItems[i].value == severity) {
			result += syslogFilterSeverityItems[i].name;
			break;
		}
	}

	return result;
}

SyslogFilter::SyslogFilter(const SyslogParameter parameter) {
    _parameter = parameter;
}

void SyslogFilter::addFilter(const String filter, const String destination) {
    _filters.push_back({_parseFilter(filter), _createSyslogFromString(destination)});
}

void SyslogFilter::addFilter(const String filter, Syslog* syslog) {
    _filters.push_back({_parseFilter(filter), syslog});
}

SyslogParameter& SyslogFilter::getParameter() {
    return _parameter;
}

void SyslogFilter::applyFilters(SyslogFilterCallback callback) {
    for (auto filter = _filters.begin(); filter != _filters.end(); ++filter) {
        if (_matchFilterExpression(filter->filter, _parameter.getFacility(), _parameter.getSeverity())) {
            if (!filter->syslog) {  // "stop"
                break;
            }
            callback(*filter);
        }
    }
}

SyslogFilterItemVector SyslogFilter::_parseFilter(const String filter) {
    SyslogFilterItemVector filters;
    int startPos = 0;
    do {
        String severity, facility;
        int endPos = filter.indexOf(',', startPos);
        facility = endPos == -1 ? filter.substring(startPos) : filter.substring(startPos, endPos);
        int splitPos = facility.indexOf('.');
        if (splitPos != -1) {
            severity = facility.substring(splitPos + 1);
            facility.remove(splitPos);
        }
        filters.push_back(std::make_pair(_syslogFacilityToInt(facility), _syslogSeverityToInt(severity)));
        startPos = endPos + 1;
    } while (startPos);

    return filters;
}

bool SyslogFilter::_matchFilterExpression(const SyslogFilterItemVector& filter, SyslogFacility facility, SyslogSeverity severity) {
    for (auto item : filter) {
        if ((item.first == SYSLOG_FACILITY_ANY || item.first == facility) && (item.second == SYSLOG_SEVERITY_ANY || item.second == severity)) {
            return true;
        }
    }
    return false;
}

Syslog* SyslogFilter::_createSyslogFromString(const String& str) {
    char* tok[4];
    uint8_t tok_count = 0;
    Syslog* syslog = nullptr;

    for (auto item = _syslogObjects.begin(); item != _syslogObjects.end(); ++item) {
        if (item->first == str) {
            return item->second;
        }
    }

    char *dupStr = strdup(str.c_str());
    char *ptr = dupStr;
    if (!ptr) {
        return nullptr;
    }

    char *ntok = nullptr;
    const char *sep = ":";
    tok[tok_count] = strtok_s(ptr, sep, &ntok);
    while (tok[tok_count++] && tok_count < 3) {
        tok[tok_count] = strtok_s(nullptr, sep, &ntok);
    }
    if (tok_count > 1) {
        tok[tok_count] = nullptr;

        ptr = tok[0];
        if (*ptr++ == '@') {
            if (*ptr == '@') {  // TCP
                bool useTLS = false;
                if (*++ptr == '!') { // TLS over TCP
                    useTLS = true;
                    ptr++;
                }
                syslog = new SyslogTCP(_parameter, ptr, tok[1] ? atoi(tok[1]) : (useTLS ? SYSLOG_PORT_TCP_TLS : SYSLOG_PORT_TCP), useTLS);
            } else if (*ptr) {  // UDP
                syslog = new SyslogUDP(_parameter, ptr, tok[1] ? atoi(tok[1]) : SYSLOG_PORT_UDP);
            }
        } else if (strcasecmp_P(ptr, PSTR("stop")) == 0) {
            syslog = SYSLOG_FILTER_STOP;
        } else {
            syslog = new SyslogFile(_parameter, tok[0], tok[1] ? atoi(tok[1]) : SYSLOG_FILE_MAX_SIZE, tok[2] ? atoi(tok[2]) : SYSLOG_FILE_MAX_ROTATE);
        }
    }
    free(dupStr);
    _syslogObjects.push_back(make_pair(str, syslog));
    return syslog;
}

SyslogQueueIterator::SyslogQueueIterator(uint32_t index, SyslogQueue *queue) : _index(index), _queue(queue) {
}

SyslogQueueIterator& SyslogQueueIterator::operator++() {
    ++_index;
    return *this;
}
SyslogQueueIterator& SyslogQueueIterator::operator--() {
    --_index;
    return *this;
}

bool SyslogQueueIterator::operator!=(const SyslogQueueIterator& item) {
    return _index != item._index;
}
bool SyslogQueueIterator::operator=(const SyslogQueueIterator& item) {
    return _index == item._index;
}

SyslogMemoryQueueItem *SyslogQueueIterator::operator*() {
    return _queue->getItem(_index);
}
