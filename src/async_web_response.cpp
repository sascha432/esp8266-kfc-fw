/**
  Author: sascha_lammers@gmx.de
*/

#include "async_web_response.h"
#include "progmem_data.h"
#include <ProgmemStream.h>
#include <PrintHtmlEntitiesString.h>
#include "fs_mapping.h"
#include "web_server.h"

#if DEBUG_ASYNC_WEB_RESPONSE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AsyncJsonResponse::AsyncJsonResponse() : _jsonBuffer(_json)
{
    _code = 200;
    _contentType = FSPGM(mime_application_json);
    _sendContentLength = true;
    _chunked = false;
}

bool AsyncJsonResponse::_sourceValid() const
{
    return true;
}

size_t AsyncJsonResponse::_fillBuffer(uint8_t *data, size_t len)
{
    return _jsonBuffer.fillBuffer(data, len);
}

JsonUnnamedObject &AsyncJsonResponse::getJsonObject()
{
    return _json;
}

void AsyncJsonResponse::updateLength()
{
    _contentLength = _json.length();
}

AsyncProgmemFileResponse::AsyncProgmemFileResponse(const String &contentType, const FSMappingEntry *mapping, TemplateDataProvider::ResolveCallback callback) :
    AsyncAbstractResponse(nullptr),
    _contentWrapped(Mappings::open(mapping, fs::FileOpenMode::read)),
    _provider(callback),
    _content(_contentWrapped, _provider)
{
    _code = 200;
    _contentLength = _content.size();
    _contentType = contentType;
    _sendContentLength = true;
    _chunked = false;
}

bool AsyncProgmemFileResponse::_sourceValid() const
{
    return (bool)_content;
}

size_t AsyncProgmemFileResponse::_fillBuffer(uint8_t *data, size_t len)
{
    return _content.read(data, len);
}

AsyncDirResponse::AsyncDirResponse(const Dir &dir, const String &dirName) : AsyncAbstractResponse()
{
    _code = 200;
    _contentLength = 0;
    _sendContentLength = false;
    _contentType = FSPGM(mime_application_json);
    _chunked = true;
    _dir = dir;
    _state = 0;
    _dirName = dirName;
    append_slash(_dirName);
    _debug_printf_P(PSTR("AsyncDirResponse::AsyncDirResponse(%s)\n"), _dirName.c_str());

    if (!_dir.rewind()) {
        _code = 404;
        _sendContentLength = true;
        _chunked = false;
        _state = 2;
        _debug_printf_P(PSTR("listing %s isValid()==true, next state 2, sending 404\n"), _dirName.c_str());
    }
}

bool AsyncDirResponse::_sourceValid() const
{
    return true;
}

size_t AsyncDirResponse::_fillBuffer(uint8_t *data, size_t len)
{
    _debug_printf_P(PSTR("AsyncDirResponse::_fillBuffer(%p, %d)\n"), data, len);
    char *ptr = reinterpret_cast<char *>(data);
    char *sptr = ptr;
    int16_t space = (int16_t)(len - 2); // reserve 2 byte
    int16_t result = 0;
    if (_state == 0) {
        FSInfo info;
        SPIFFS_info(info);

        // if (String_endsWith(_dirName, '/')) {
        //     _dirName.remove(_dirName.length() - 1, 1);
        // }

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
        _next = _dir.next();
        _debug_printf_P(PSTR("init list %s next=%d\n"), _dirName.c_str(), _next);
    }
    if (_state == 1) {
        String tmp_dir = sys_get_temp_dir();
        char modified[32];

        _debug_printf_P(PSTR("load list %s, tmp_dir %s\n"), _dirName.c_str(), tmp_dir.c_str());

        String path;
        while (_next) {
            path = _dir.fileName();
            auto mapping = Mappings::getEntry(path);
            modified[0] = '0';
            modified[1] = 0;

#if SPIFFS_TMP_FILES_TTL || SPIFFS_CLEANUP_TMP_DURING_BOOT
            if (!mapping && path.length() > tmp_dir.length() && path.startsWith(tmp_dir)) { // check if the name matches the location of temporary files, exclude mapped files
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
            }
            else if (mapping) {
                auto time = mapping->modificationTime;
                auto tm = timezone_localtime(&time);
                timezone_strftime_P(modified, sizeof(modified), PSTR("\"%Y-%m-%d %H:%M\""), tm);
            }
#endif

            // _debug_printf_P(PSTR("%s: %d %d\n"), path.c_str(), _dir.isDir(), _dir.isFile());

            String name = path.substring(_dirName.length());
            String location = url_encode(path).c_str();

            if (_dir.isDirectory()) {
                remove_trailing_slash(name);
                remove_trailing_slash(location);

                if ((result = snprintf_P(ptr, space, PSTR("{\"f\":\"%s\",\"n\":\"%s\",\"m\":%d,\"t\":%s,\"d\":1},"),
                    location.c_str(),
                    name.c_str(),
                    path.startsWith(tmp_dir) ? TYPE_TMP_DIR : (mapping ? TYPE_MAPPED_DIR : TYPE_REGULAR_DIR),
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
                    mapping ? TYPE_MAPPED_FILE : TYPE_REGULAR_FILE,
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

AsyncNetworkScanResponse::AsyncNetworkScanResponse(bool hidden) : AsyncAbstractResponse()
{
    _code = 200;
    _contentLength = 0;
    _sendContentLength = false;
    _contentType = FSPGM(mime_application_json);
    _chunked = true;
    _position = 0;
    _done = false;
    _hidden = hidden;
}

AsyncNetworkScanResponse::~AsyncNetworkScanResponse()
{
    if (_done) {
        WiFi.scanDelete();
        setLocked(false);
    }
}

bool AsyncNetworkScanResponse::_sourceValid() const
{
    return true;
}

uint16_t AsyncNetworkScanResponse::_strcpy_P_safe(char *&dst, PGM_P str, int16_t &space)
{
    if (space <= 1) {
        return 0;
    }
    strncpy_P(dst, str, space - 1)[space - 1] = 0;
    uint16_t copied = strlen(dst);
    space -= copied;
    dst += copied;
    return copied;
}

size_t AsyncNetworkScanResponse::_fillBuffer(uint8_t *data, size_t len)
{
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

bool AsyncNetworkScanResponse::isLocked()
{
    return _locked;
}

void AsyncNetworkScanResponse::setLocked(bool locked)
{
    _locked = locked;
}

AsyncBufferResponse::AsyncBufferResponse(const String & contentType, Buffer * buffer, AwsTemplateProcessor templateCallback) : AsyncAbstractResponse(templateCallback)
{
    _code = 200;
    _position = 0;
    _content = buffer;
    _contentLength = buffer->length();
    _sendContentLength = true;
    _chunked = false;
}

AsyncBufferResponse::~AsyncBufferResponse()
{
    delete _content;
}

bool AsyncBufferResponse::_sourceValid() const
{
    return (bool)_content->length();
}

size_t AsyncBufferResponse::_fillBuffer(uint8_t * buf, size_t maxLen)
{
    size_t send = _content->length() - _position;
    if (send > maxLen) {
        send = maxLen;
    }
    memcpy(buf, &_content->getConstChar()[_position], send);
    // debug_printf("%u %u %d\n", _position, send, maxLen);
    _position += send;
    return send;
}


AsyncTemplateResponse::AsyncTemplateResponse(const String &contentType, const FSMappingEntry *mapping, WebTemplate *webTemplate, TemplateDataProvider::ResolveCallback callback) :
    AsyncProgmemFileResponse(contentType, mapping, callback), _webTemplate(webTemplate)
{
    _contentLength = 0;
    _sendContentLength = false;
    _chunked = true;
}

AsyncTemplateResponse::~AsyncTemplateResponse()
{
    delete _webTemplate;
}

AsyncSpeedTestResponse::AsyncSpeedTestResponse(const String &contentType, uint32_t size) : AsyncAbstractResponse(nullptr)
{
    uint16_t width = sqrt(size / 2);
    _size = width * width * 2 + sizeof(_header);
    memset(&_header, 0, sizeof(_header));
    _header.h.bfh.bfType = 'B' | ('M' << 8);
    _header.h.bfh.bfSize = _size;
    _header.h.bfh.bfReserved2 = sizeof(_header.h);

    _header.h.bih.biSize = sizeof(_header.h.bih);
    _header.h.bih.biPlanes = 1;
    _header.h.bih.biBitCount = 16;
    _header.h.bih.biWidth = width;
    _header.h.bih.biHeight = width;

    _code = 200;
    _contentLength = _size;
    _sendContentLength = true;
    _chunked = false;
    _contentType = contentType;
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
    if (_header.h.bih.biWidth) {
        memcpy(buf, &_header, sizeof(_header)); // maxLen is > 54 byte for the first call for sure
        _header.h.bih.biWidth = 0;
        _size -= sizeof(_header);
        return sizeof(_header);
    }
    memset(buf, 0xff, available);
    _size -= available;
    return available;
}
