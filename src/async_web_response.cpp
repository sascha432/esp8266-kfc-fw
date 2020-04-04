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

AsyncBaseResponse::AsyncBaseResponse(bool chunked)
{
    if (chunked) {
        _contentLength = 0;
        _sendContentLength = false;
        _chunked = true;
    }
}

void AsyncBaseResponse::__assembleHead(uint8_t version)
{
    PrintString out;
    out.printf_P(PSTR("HTTP/1.%d %d %s\r\n"), version, _code, _responseCodeToString(_code));

    if (_sendContentLength) {
        HttpContentLengthHeader(_contentLength).printTo(out);
        //out.printf_P(PSTR("Content-Length: %d\r\n"), _contentLength);
    }
    if (_contentType.length()) {
        HttpContentType(_contentType).printTo(out);
        //out.printf_P(PSTR("Content-Type: %s\r\n"), _contentType.c_str());
    }

    HttpConnectionHeader().printTo(out);
    //addHeader(F("Connection"), F("close"));

    for(const auto &header : _headers) {
        out.printf_P(PSTR("%s: %s\r\n"), header->name().c_str(), header->value().c_str());
    }
    _headers.free();

    if (version) {
        out += AsyncWebHeader(F("Accept-Ranges"), F("none")).toString();
        if (_chunked) {
            out += AsyncWebHeader(F("Transfer-Encoding"), F("chunked")).toString();
        }
    }

    for(const auto &header : _httpHeaders) {
        header->printTo(out);
    }
    _httpHeaders.clear();

    out.println();
    _headLength = out.length();
    _head = std::move(out);
}

void AsyncBaseResponse::_respond(AsyncWebServerRequest *request)
{
    __assembleHead(request->version());
    _state = RESPONSE_HEADERS;
    _ack(request, 0, 0);
}

size_t AsyncBaseResponse::_ack(AsyncWebServerRequest* request, size_t len, uint32_t time)
{
    if (!_sourceValid()) {
        _state = RESPONSE_FAILED;
        request->client()->close();
        return 0;
    }
    _ackedLength += len;
    size_t space = request->client()->space();

    size_t headLen = _head.length();
    if (_state == RESPONSE_HEADERS) {
        if (space > headLen) {
            // do not send extra packet for remaining headers
            _state = RESPONSE_CONTENT;
        } else {
            _writtenLength += request->client()->write(_head.c_str(), space);
            _head.remove(0, space);
            return space;
        }
    }

    if (_state == RESPONSE_CONTENT) {
        size_t outLen;
        space -= headLen; // remove header length if we have headers
        if (_chunked) {
            if (space <= 8) { // we need at 8 extra bytes for the chunked header
                return 0;
            }
            outLen = space;
        } else if (!_sendContentLength) { // unknown content length
            outLen = space;
        } else {
            outLen = std::min(space, (_contentLength - _sentLength)); // max. data we have to send
        }

        auto bufPtr = std::unique_ptr<uint8_t[]>(new uint8_t[outLen + headLen]);
        auto buf = bufPtr.get();
        if (!buf) {
            return 0;
        }

        size_t readLen = 0;
        if (_chunked) {
            buf += 6;
            if ((readLen = _fillBuffer(buf + headLen, outLen - 8)) == RESPONSE_TRY_AGAIN) {
                return 0;
            }
            auto header = PrintString(F("%x\r\n"), readLen);
            buf -= header.length();
            memcpy(buf + headLen, header.c_str(), header.length());

            outLen = readLen + headLen + header.length();
            buf[outLen++] = '\r';
            buf[outLen++] = '\n';

        } else {
            if ((readLen = _fillBuffer(buf + headLen, outLen)) == RESPONSE_TRY_AGAIN) {
                return 0;
            }
            outLen = readLen + headLen;
        }
        _sentLength += readLen;

        if (headLen) {
            memcpy(buf, _head.c_str(), headLen);
            _head = String();
        }
        _writtenLength += request->client()->write(reinterpret_cast<char *>(buf), outLen);

        if (!readLen) {
            _state = RESPONSE_WAIT_ACK;
        }
        return outLen;

    } else if (_state == RESPONSE_WAIT_ACK) {
        if (!_sendContentLength || _ackedLength >= _writtenLength) {
            _state = RESPONSE_END;
            if (!_chunked && !_sendContentLength) {
                request->client()->close(true);
            }
        }
    }
    return 0;
}

AsyncJsonResponse::AsyncJsonResponse() : AsyncBaseResponse(false), _jsonBuffer(_json)
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

AsyncProgmemFileResponse::AsyncProgmemFileResponse(const String &contentType, const File &file, TemplateDataProvider::ResolveCallback callback) :
    AsyncBaseResponse(false),
    _contentWrapped(file),
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

AsyncDirResponse::AsyncDirResponse(const ListDir &dir, const String &dirName) : AsyncBaseResponse(true)
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
            modified[0] = '0';
            modified[1] = 0;

#if SPIFFS_TMP_FILES_TTL || SPIFFS_CLEANUP_TMP_DURING_BOOT
            if (path.length() > tmp_dir.length() && path.startsWith(tmp_dir)) { // check if the name matches the location of temporary files, exclude mapped files
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
            else if (_dir.isMapping()) {
                time_t time = _dir.fileTime();
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
                    path.startsWith(tmp_dir) ? TYPE_TMP_DIR : (_dir.isMapping() ? TYPE_MAPPED_DIR : TYPE_REGULAR_DIR),
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
                    _dir.isMapping() ? TYPE_MAPPED_FILE : TYPE_REGULAR_FILE,
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

AsyncNetworkScanResponse::AsyncNetworkScanResponse(bool hidden) : AsyncBaseResponse(true)
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

// AsyncBufferResponse::AsyncBufferResponse(const String & contentType, Buffer * buffer, AwsTemplateProcessor templateCallback) : AsyncBaseResponse(true)
// {
//     _code = 200;
//     _position = 0;
//     _content = buffer;
//     _contentLength = buffer->length();
//     _sendContentLength = true;
//     _chunked = false;
// }

// AsyncBufferResponse::~AsyncBufferResponse()
// {
//     delete _content;
// }

// bool AsyncBufferResponse::_sourceValid() const
// {
//     return (bool)_content->length();
// }

// size_t AsyncBufferResponse::_fillBuffer(uint8_t * buf, size_t maxLen)
// {
//     size_t send = _content->length() - _position;
//     if (send > maxLen) {
//         send = maxLen;
//     }
//     memcpy(buf, &_content->getConstChar()[_position], send);
//     // debug_printf("%u %u %d\n", _position, send, maxLen);
//     _position += send;
//     return send;
// }


AsyncTemplateResponse::AsyncTemplateResponse(const String &contentType, const File &file, WebTemplate *webTemplate, TemplateDataProvider::ResolveCallback callback) :
    AsyncProgmemFileResponse(contentType, file, callback), _webTemplate(webTemplate)
{
    _contentLength = 0;
    _sendContentLength = false;
    _chunked = true;
}

AsyncTemplateResponse::~AsyncTemplateResponse()
{
    delete _webTemplate;
}

AsyncSpeedTestResponse::AsyncSpeedTestResponse(const String &contentType, uint32_t size) : AsyncBaseResponse(true)
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
