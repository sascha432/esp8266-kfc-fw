/**
  Author: sascha_lammers@gmx.de
*/

#include "async_web_response.h"
#include "progmem_data.h"
#include <ProgmemStream.h>
#include "fs_mapping.h"
#include "web_server.h"

#if DEBUG_ASYNC_WEB_RESPONSE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AsyncJsonResponse::AsyncJsonResponse() : _jsonBuffer(_json) {
    _code = 200;
    _contentType = F("application/json");
    _sendContentLength = true;
    _chunked = false;
}

bool AsyncJsonResponse::_sourceValid() const {
    return true;
}

size_t AsyncJsonResponse::_fillBuffer(uint8_t *data, size_t len) {
    return _jsonBuffer.fillBuffer(data, len);
}

JsonUnnamedObject &AsyncJsonResponse::getJsonObject() {
    return _json;
}

void AsyncJsonResponse::updateLength() {
    _contentLength = _json.length();
}


AsyncProgmemFileResponse::AsyncProgmemFileResponse(const String &contentType, FSMapping *mapping, AwsTemplateProcessor templateCallback) : AsyncAbstractResponse(templateCallback) {
    _code = 200;
    _content = mapping->open("r");
    _contentLength = _content.size();
    _contentType = contentType;
    _sendContentLength = true;
    _chunked = false;
}

bool AsyncProgmemFileResponse::_sourceValid() const {
    return (bool)_content;
}

size_t AsyncProgmemFileResponse::_fillBuffer(uint8_t *data, size_t len) {
    return _content.read(data, len);
}

AsyncDirResponse::AsyncDirResponse(const AsyncDirWrapper &dir) : AsyncAbstractResponse() {
    _debug_printf_P(PSTR("AsyncDirResponse::AsyncDirResponse(%s)\n"), _dir.getDirName().c_str());
    _code = 200;
    _contentLength = 0;
    _sendContentLength = false;
    _contentType = FSPGM(application_json);
    _chunked = true;
    _dir = dir;
    _state = 0;
    if (!_dir.isValid()) {
        _code = 404;
        _sendContentLength = true;
        _chunked = false;
        _state = 2;
        _debug_printf_P(PSTR("listing %s isValid()==true, next state 2, sending 404\n"), _dir.getDirName().c_str());
    }
}

bool AsyncDirResponse::_sourceValid() const {
    return true;
}

size_t AsyncDirResponse::_fillBuffer(uint8_t *data, size_t len) {
    _debug_printf_P(PSTR("AsyncDirResponse::_fillBuffer(%p, %d)\n"), data, len);
    char *ptr = reinterpret_cast<char *>(data);
    char *sptr = ptr;
    int16_t space = (int16_t)(len - 2); // reserve 2 byte
    int16_t result = 0;
    if (_state == 0) {
        FSInfo info;
        SPIFFS_info(info);

        _dirName = _dir.getDirName();
        _dirNameLen = _dirName.length();
        if (_dirNameLen > 1 && _dirName.charAt(_dirNameLen - 1) == '/') {
            _dirNameLen--;
        }

        if ((result = snprintf_P(ptr, space, PSTR("{\"total\":\"%s\",\"total_b\":%d,\"used\":\"%s\",\"used_b\":%d,\"usage\":\"%.2f%%\",\"dir\":\"%s\",\"files\":["),
                formatBytes(info.totalBytes).c_str(), info.totalBytes,
                formatBytes(info.usedBytes).c_str(), info.usedBytes,
                (float)info.usedBytes / (float)info.totalBytes * 100.0,
                _dirName.c_str())) >= space)
        { // buffer too small, abort response
            _debug_printf_P(PSTR("buffer too small %d\n"), space);
            _state = 2;
            return 0;
        }
        ptr += result;
        space -= result;
        sptr = ptr;

        _state = 1;
        _next = _dir.isValid() && _dir.next();
        _debug_printf_P(PSTR("init list %s next=%d\n"), _dirName.c_str(), _next);
    }
    if (_state == 1) {
        String tmp_dir = sys_get_temp_dir();
        char modified[64];

        _debug_printf_P(PSTR("load list %s, tmp_dir %s\n"), _dirName.c_str(), tmp_dir.c_str());

        String path;
        while (_next) {
            path = _dir.fileName();
            modified[0] = '0';
            modified[1] = 0;

#if SPIFFS_TMP_FILES_TTL || SPIFFS_CLEANUP_TMP_DURING_BOOT
            if (!_dir.isMapped() && path.length() > tmp_dir.length() && path.startsWith(tmp_dir)) { // check if the name matches the location of temporary files, exclude mapped files
#  if SPIFFS_TMP_FILES_TTL

                ulong ttl = strtoul(path.substring(tmp_dir.length()).c_str(), NULL, HEX);
                ulong now = (millis() / 1000UL);
                if (ttl > 0) {
                    if (now < ttl) {
                        snprintf_P(modified, sizeof(modified), PSTR("\"TTL %s\""), formatTime(ttl - now, true).c_str());
                    } else {
                        strncpy_P(modified, PSTR("\"TTL - scheduled for removal\""), sizeof(modified));
                    }
                } else
#  endif
                {
#  if SPIFFS_CLEANUP_TMP_DURING_BOOT
                    strncpy_P(modified, PSTR("\"TTL until next reboot\""), sizeof(modified));
#  endif
                }
            } else if (_dir.isMapped()) {

                _dir.getModificatonTime(modified, sizeof(modified), PSTR("\"%Y-%m-%d %H:%M\""));
            }
#endif

            // _debug_printf_P(PSTR("%s: %d %d\n"), path.c_str(), _dir.isDir(), _dir.isFile());

            String name = path.substring(_dirNameLen);
            String location = url_encode(path).c_str();

            if (_dir.isDir()) {
                remove_trailing_slash(name);
                remove_trailing_slash(location);

                if ((result = snprintf_P(ptr, space, PSTR("{\"f\":\"%s\",\"n\":\"%s\",\"m\":%d,\"t\":%s,\"d\":1},"),
                    location.c_str(),
                    name.c_str(),
                    path.startsWith(tmp_dir) ? TYPE_TMP_DIR : (_dir.isMapped() ? TYPE_MAPPED_DIR : TYPE_REGULAR_DIR),
                    modified)) >= space)
                {
                    break;
                }
                ptr += result;
                space -= result;
                sptr = ptr;
            }
            else if (_dir.isFile()) {
                if ((result = snprintf_P(ptr, space, PSTR("{\"f\":\"%s\",\"n\":\"%s\",\"s\":\"%s\",\"sb\":%d,\"m\":%d,\"t\":%s,\"d\":0},"),
                    location.c_str(),
                    name.c_str(),
                    formatBytes(_dir.fileSize()).c_str(),
                    _dir.fileSize(),
                    _dir.isMapped() ? TYPE_MAPPED_FILE : TYPE_REGULAR_FILE,
                    modified)
                ) >= space) {
                    break;
                }
                ptr += result;
                space -= result;
                sptr = ptr;
            }
#if DEBUG_ASYNC_WEB_RESPONSE
            else {
                _debug_printf_P(PSTR("path %s invalid type\n"), path.c_str());
            }
#endif
            _next = _dir.next();
        }
        if (!_next) { // end of listing
            if (sptr != reinterpret_cast<char *>(data)) { // any data added?
                sptr--;
                if (*sptr != ',') { // trailing comma?
                    sptr++;
                }
            }
            if (space >= 0) { // 2 byte have been reserved
                *sptr++ = ']';
                *sptr++ = '}';
            }
            _state = 2;
        }
        result = (sptr - reinterpret_cast<char *>(data));
    } else if (_state == 2) {
        _debug_printf_P(PSTR("end list %s, EOF\n"), _dirName.c_str());
        result = 0;
    }
    // _debug_printf_P(PSTR("chunk %d state %d next %d '%-*.*s'\n"), result, _state, _next, result, result, data);
    return result;
}

bool AsyncNetworkScanResponse::_locked = false;

AsyncNetworkScanResponse::AsyncNetworkScanResponse(bool hidden) : AsyncAbstractResponse() {
    _code = 200;
    _contentLength = 0;
    _sendContentLength = false;
    _contentType = FSPGM(application_json);
    _chunked = true;
    _position = 0;
    _done = false;
    _hidden = hidden;
}

AsyncNetworkScanResponse::~AsyncNetworkScanResponse() {
    if (_done) {
        WiFi.scanDelete();
        setLocked(false);
    }
}

bool AsyncNetworkScanResponse::_sourceValid() const {
    return true;
}

uint16_t AsyncNetworkScanResponse::_strcpy_P_safe(char *&dst, PGM_P str, int16_t &space) {
    if (space <= 1) {
        return 0;
    }
    strncpy_P(dst, str, space - 1)[space - 1] = 0;
    uint16_t copied = strlen(dst);
    space -= copied;
    dst += copied;
    return copied;
}

size_t AsyncNetworkScanResponse::_fillBuffer(uint8_t *data, size_t len) {
    if (_position == (uint8_t)-1) {
        _debug_println(F("AsyncNetworkScanResponse pos == -1, EOF"));
        return 0;
    }
    int8_t n = WiFi.scanComplete();
    if (n < 0) {
        if (!isLocked() && n == -2) { // no scan running and no results available
            setLocked();
            WiFi.scanNetworks(true, _hidden);
        }
        _position = -1;
        _debug_println(F("Scan running"));
        int16_t space = (int16_t)len;
        auto dst = reinterpret_cast<char *>(data);
        return _strcpy_P_safe(dst, PSTR("{\"pending\":true,\"msg\":\"Network scan still running\"}"), space);
    }
    else if (n == 0) {
        _position = -1;
        _debug_println(F("No networks in range"));
        int16_t space = (int16_t)len;
        auto dst = reinterpret_cast<char *>(data);
        return _strcpy_P_safe(dst, PSTR("{\"msg\":\"No WiFi networks in range\"}"), space);
    }
    else {
        if (_position >= n) {
            _debug_printf_P(PSTR("AsyncNetworkScanResponse %d >= %d\n, EOF"), (int)_position, (int)n);
            return 0;
        }
        auto ptr = reinterpret_cast<char *>(data);
        auto sptr = ptr;
        int16_t space = len - 2; // reserve 2 bytes
        if (_position == 0) {
            _strcpy_P_safe(ptr, PSTR("{\"result\":["), space);
        }
        uint16_t l;
        while (_position < n && space > 0) {
            _strcpy_P_safe(ptr, PSTR("{\"tr_class\":\""), space);
            if (WiFi_isHidden(_position)) {
                _strcpy_P_safe(ptr, PSTR("table-secondary"), space);
            } else {
                _strcpy_P_safe(ptr, PSTR("has-network-name\",\"td_class\":\"network-name"), space);
            }
            _strcpy_P_safe(ptr, PSTR("\",\"ssid\":\""), space);
            if (WiFi_isHidden(_position)) {
                _strcpy_P_safe(ptr, PSTR("<i>HIDDEN</i>"), space);
            }
            else {
                String tmp = WiFi.SSID(_position);
                tmp.replace(F("\""), F("\\\""));
                if ((int16_t)tmp.length() >= space) {
                    break;
                }
                strcpy(ptr, tmp.c_str());
                ptr += tmp.length();
                space -= tmp.length();
            }
            if ((l = snprintf_P(ptr, space, PSTR("\",\"channel\":%d,\"rssi\":%d,\"bssid\":\"%s\",\"encryption\":\"%s\"},"),
                WiFi.channel(_position),
                WiFi.RSSI(_position),
                WiFi.BSSIDstr(_position).c_str(),
                KFCFWConfiguration::getWiFiEncryptionType(WiFi.encryptionType(_position)).c_str()
                )) >= space)
            {
                space = 0;
                break;
            }
            ptr += l;
            space -= l;
            sptr = ptr;
            _position++;
        }
        if (_position >= n) {
            if (sptr != reinterpret_cast<char *>(data)) { // any data copied?
                sptr--;
                if (*sptr != ',') { // trailing comma?
                    sptr++;
                }
            }
             if (space >= 0) { // 2 byte have been reserved
                *sptr++ = ']';
                *sptr++ = '}';
             }
            _done = true;
            _position = -1;
        }
        else if (_position == 0) {  // not enough space in the buffer for the first entry, abort response
            _position = -1;
        }
        // int ll = (sptr - (char *)data);
        // _debug_printf_P(PSTR("chunk %d %-*.*s\n"), ll, ll, ll, data);
        return (sptr - reinterpret_cast<char *>(data));
    }
}

bool AsyncNetworkScanResponse::isLocked() {
    return _locked;
}

void AsyncNetworkScanResponse::setLocked(bool locked) {
    _locked = locked;
}

AsyncBufferResponse::AsyncBufferResponse(const String & contentType, Buffer * buffer, AwsTemplateProcessor templateCallback) : AsyncAbstractResponse(templateCallback) {
    _code = 200;
    _position = 0;
    _content = buffer;
    _contentLength = buffer->length();
    _sendContentLength = true;
    _chunked = false;
}

AsyncBufferResponse::~AsyncBufferResponse() {
    delete _content;
}

bool AsyncBufferResponse::_sourceValid() const {
    return (bool)_content->length();
}

size_t AsyncBufferResponse::_fillBuffer(uint8_t * buf, size_t maxLen) {
    size_t send = _content->length() - _position;
    if (send > maxLen) {
        send = maxLen;
    }
    memcpy(buf, &_content->getConstChar()[_position], send);
    // debug_printf("%u %u %d\n", _position, send, maxLen);
    _position += send;
    return send;
}

AsyncDirWrapper::AsyncDirWrapper() {
    _isValid = false;
    _dir = Dir();
    _curMapping = &Mappings::getInstance().getInvalid();
}

AsyncDirWrapper::AsyncDirWrapper(const String & dirName) : AsyncDirWrapper() {
    _debug_printf_P(PSTR("AsyncDirWrapper::AsyncDirWrapper(%s)\n"), dirName.c_str());
    _dirName = dirName;
    append_slash(_dirName);
    _dir = SPIFFSWrapper::openDir(_sharedEmptyString);
    _isValid = _dir.next();
    _firstNext = true;
    _dirs.push_back(_dirName);

    if (_isValid) { // filter files that do not match _dirName
        _begin = Mappings::getInstance().getBeginIterator();
        _end = Mappings::getInstance().getEndIterator();
        _iterator = _begin;
        _curMapping = nullptr;
    }
}

AsyncDirWrapper::~AsyncDirWrapper() {
    if (_curMapping && _curMapping->isValid()) { // must be allocated memory
        delete _curMapping;
    }
}

void AsyncDirWrapper::setVirtualRoot(const String & path) {
    _virtualRoot = path;
}

String & AsyncDirWrapper::getDirName() {
    return _dirName;
}

bool AsyncDirWrapper::isValid() const {
    return _isValid;
}

/*

check if files belong to _dirName or if the next subdirectory belongs to _dirname

dir /www/
false /file1
false /file2
true  /www/.
true  /www/file3
true  /www/js/file6
true  /www/images/file5
false /www/images/icons/.
false /www/images/icons/file7

*/
bool AsyncDirWrapper::_fileInside(const String & path) {
    _debug_printf_P(PSTR("AsyncDirWrapper::_fileInside(%s)\n"), path.c_str());
    if (!path.startsWith(_dirName)) { // not a match
        return false;
    }
    String name, fullpath;
    int pos = _dirName.length(); // points to the first character of the file or subdirectory
    int pos2 = path.indexOf('/', pos + 1);
    if (pos2 != -1) { // sub directory pos - pos2

        name = path.substring(pos, pos2 - pos);
        fullpath = path.substring(0, pos2);
        if (std::find(_dirs.begin(), _dirs.end(), fullpath) != _dirs.end()) { // do we have it already?
            return false;
        }
        _dirs.push_back(fullpath);
        _type = DIR;
        _fileName = fullpath;

    }
    else if (pos2 == -1) { // filename or "."

        name = path.substring(pos);
        fullpath = path;
        if (name == FSPGM(dot)) {
            return false;
        }
        _type = FILE;
        _fileName = fullpath;
    }

    _debug_printf(PSTR("%s: '%s' '%s' (%d, %d) %d\n"), path.c_str(), name.c_str(), fullpath.c_str(), pos, pos2, _type);
    return true;
}

bool AsyncDirWrapper::isDir() const {
    return _type == DIR;
}

bool AsyncDirWrapper::isFile() const {
    return _type == FILE;
}

bool AsyncDirWrapper::next() {
    _debug_printf_P(PSTR("AsyncDirWrapper::next()\n"));
    _type = INVALID;
    if (!isValid()) {
        return false;
    }
    // debug_println("next()");
    bool result = false;
    if (_iterator != _end) { // go through virtual files first
        result = true;
        do {
            if (_curMapping) {
                delete _curMapping;
            }
            if (_iterator == _end) { // continue with files from Dir()
                result = false;
                _curMapping = &Mappings::getInstance().getInvalid();
                break;
            }
            _curMapping = new FSMapping(_iterator->getPath(), _iterator->getMappedPath(), _iterator->getModificatonTime(), _iterator->getFileSize()); //TODO memory leak, FSMapping needs a clone method or unique_ptr has to be removed
            _fileName = _virtualRoot + _curMapping->getPath();
            // if (!_fileName.startsWith(_dirName)) {
            //     debug_printf("filtered-virtual %s: %s (%d) => %s\n", _dirName.c_str(), _fileName.c_str(), isMapped(), mappedFile().c_str());
            // }
            ++_iterator;
        } while (!_fileInside(_fileName)); // filer files if real file does not match _dirName
    }
    if (!result) {
        if (_curMapping && _curMapping->isValid()) { // must be allocated memory
            delete _curMapping;
            _curMapping = &Mappings::getInstance().getInvalid();
        }
        bool skip;
        do {
            if (_firstNext) {
                _firstNext = false;
                result = true;
            }
            else {
                result = _dir.next();
                if (!result) {
                    break;
                }
            }
            skip = !_fileInside(_dir.fileName());
            if (!skip) {
                for (auto it = _begin; it != _end; ++it) {
                    if (_dir.fileName().equals(it->getMappedPath())) {
                        skip = true; // remove real files that are mapped to virtual files
                    }
                }
            }
            // if (result && skip) {
            //     debug_printf("filtered-mapped %s: %s (%d)\n", _dirName.c_str(), mappedFile().c_str(), isMapped());
            // }
        } while (skip);
    }
#if DEBUG_ASYNC_WEB_RESPONSE
    if (!result) {
        for (const auto &dir : _dirs) {
            _debug_printf_P(PSTR("dirs: %s\n"), dir.c_str());
        }
    }
#endif
    _debug_printf(PSTR("next() = %s %d\n"), _fileName.c_str(), result);
    return result;
}

File AsyncDirWrapper::openFile(const char * mode) {
    if (_curMapping->isValid()) {
        return _curMapping->open(mode);
    }
    return SPIFFSWrapper::open(_dir, mode);
}

String AsyncDirWrapper::fileName() {
    return _fileName;
}

String AsyncDirWrapper::mappedFile() {
    if (_curMapping->isValid()) {
        return _curMapping->getMappedPath();
    }
    return _dir.fileName();
}

bool AsyncDirWrapper::isMapped() const {
    return _curMapping->isValid();
}

void AsyncDirWrapper::getModificatonTime(char * modified, size_t size, PGM_P format) {
    if (!_curMapping->isValid()) {
        modified[0] = '0';
        modified[1] = 0;
        return;
    }
    struct tm *tm = timezone_localtime(_curMapping->getModificatonTimePtr());
    timezone_strftime_P(modified, size, format, tm);
}

size_t AsyncDirWrapper::fileSize() {
    if (_curMapping->isValid()) {
        return _curMapping->getFileSize();
    }
    return _dir.fileSize();
}

AsyncTemplateResponse::AsyncTemplateResponse(const String &contentType, FSMapping *mapping, WebTemplate *webTemplate) : AsyncProgmemFileResponse(contentType, mapping, [this](const String &var) -> String {
    return this->process(var);
}) {
    _webTemplate = webTemplate;
    _contentLength = 0;
    _sendContentLength = false;
    _chunked = true;
}

AsyncTemplateResponse::~AsyncTemplateResponse() {
    delete _webTemplate;
}

AsyncSpeedTestResponse::AsyncSpeedTestResponse(const String &contentType, uint32_t size) : AsyncAbstractResponse(nullptr), _size(size)
{
    _length = 1536;
    do {
        _data = (char *)malloc(_length);
        _length /= 2;
    }
    while(!_data); // if we cannot allocate even 1 byte its likely to crash anyway

    for(uint16_t i = 0; i < _length; i++) {
        _data[i] = rand();
    }

    _code = 200;
    _contentLength = _size;
    _sendContentLength = true;
    _chunked = false;
    _contentType = contentType;
}

AsyncSpeedTestResponse::~AsyncSpeedTestResponse()
{
    free(_data);
}

bool AsyncSpeedTestResponse::_sourceValid() const
{
    return true;
}

size_t AsyncSpeedTestResponse::_fillBuffer(uint8_t *buf, size_t maxLen)
{
    size_t available = _size;
    if (available > maxLen) {
        available = maxLen;
    }
    if (available > _length) {
        available = _length;
    }
    memcpy(buf, _data, available);
    return available;
}
