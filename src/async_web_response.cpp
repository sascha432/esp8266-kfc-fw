/**
  Author: sascha_lammers@gmx.de
*/

#include "async_web_response.h"
#include <PrintHtmlEntitiesString.h>
#include <MicrosTimer.h>
#include "fs_mapping.h"
#include "web_server.h"
#include "stl_ext/algorithm.h"
#include "../src/plugins/plugins.h"

#if DEBUG_ASYNC_WEB_RESPONSE
#include <debug_helper_enable.h>
#else

#include <debug_helper_disable.h>
// enable partial debugging here
#define DEBUG_ASYNC_WEB_RESPONSE_DIR_RESPONSE       0

#endif

AsyncBaseResponse::AsyncBaseResponse(bool chunked)
{
    if (chunked) {
        // change those 2 values for chunked
        _sendContentLength = false;
        _chunked = true;
    }
}

void AsyncBaseResponse::__assembleHead(uint8_t version)
{
    PrintString out;
    out.printf_P(PSTR("HTTP/1.%d %d %s\r\n"), version, _code, _responseCodeToString(_code));

    if (_sendContentLength) {
        _httpHeaders.replace<HttpContentLengthHeader>(_contentLength);
        // out.printf_P(PSTR("Content-Length: %d\r\n"), _contentLength);
    }
    else {
        _httpHeaders.remove(F("Content-Length"));
    }
    if (_contentType.length()) {
        _httpHeaders.replace<HttpContentType>(_contentType);
        //out.printf_P(PSTR("Content-Type: %s\r\n"), _contentType.c_str());
    }

    _httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::CLOSE);

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
            _head.~String();
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

AsyncJsonResponse::AsyncJsonResponse() :
    AsyncBaseResponse(false),
    _jsonBuffer(_json)
{
    _code = 200;
    _contentType = FSPGM(mime_application_json);
}

bool AsyncJsonResponse::_sourceValid() const
{
    return true;
}

size_t AsyncJsonResponse::_fillBuffer(uint8_t *data, size_t len)
{
#if DEBUG
    auto size = _jsonBuffer.fillBuffer(data, len);
    if (size == 0) {
        auto diff = _contentLength - _sentLength;
        if (diff > len) {
            diff = len;
        }
        memset(data, ' ', diff);
        // debug code below in __assembleHead()
        __DBG_printf("_json.length()=%u _jsonBuffer.fillBuffer()=%u filling missing data with spaces. check for utf-8/unicode issues", _json.length(), _sentLength);
        return diff;
    }
    return size;
#else
    return _jsonBuffer.fillBuffer(data, len);
#endif
}

JsonUnnamedObject &AsyncJsonResponse::getJsonObject()
{
    return _json;
}

void AsyncJsonResponse::__assembleHead(uint8_t version)
{
#if 1
    __DBG_printf("__assembleHead _sendContentLength=%u _contentLength=%u json=%u/%u", _sendContentLength, _contentLength, _json.length(), _json.toString().length());
    Serial.println(F("------ _json.toString() ------"));
    Serial.println(_json.toString());
    Serial.println(F("------ JsonBuffer(_json).fillBuffer(); ------"));
    uint8_t buffer[101];
    JsonBuffer b(_json);
    PrintString c(F("chunks "));
    int res = 0;
    do {
        res = b.fillBuffer(buffer, 100);
        buffer[100] = 0;
        buffer[res] = 0;
        c.printf("%d ", res);
        Serial.println((char *)buffer);
    }
    while(res);
    Serial.println(F("------"));
    Serial.println(c);
    Serial.println(F("------"));
#endif
    if (_sendContentLength) {
        _contentLength = _json.length();
    }
    AsyncBaseResponse::__assembleHead(version);
};


#if ESP32

size_t AsyncMDNSResponse::_fillBuffer(uint8_t *data, size_t len)
{
    auto &json = getJsonObject();
    if (!json.size()) {
        auto &rows = json.addArray('l');
        return AsyncJsonResponse::_fillBuffer(data, len);
    }
    return 0;
}

#else

#if MDNS_PLUGIN

size_t AsyncMDNSResponse::_fillBuffer(uint8_t *data, size_t len)
{
    noInterrupts();
    if (_output->_timeout && millis() > _output->_timeout) {
        _output->end();
    }

    size_t keep = _output->_timeout == 0 ? 0 : 32;
    if (_output->_output.length() == 0 && keep == 0) {
        // __DBG_printf("length=0 keep=0");
        interrupts();
        return 0;
    }
    else if (_output->_output.length() > keep) {
        auto avail = std::clamp<int>(_output->_output.length() - (keep ? 4 : 0), 0, len);
        // __DBG_printf("avail=%u len=%u keep=%u", avail, _output->_output.length(), keep);
        if (avail) {
            memcpy(data, _output->_output.c_str(), avail);
            _output->_output = _output->_output.c_str() + avail;
            interrupts();
            return avail;
        }
    }
    interrupts();
    // __DBG_printf("retry keep=%u", keep);
    return RESPONSE_TRY_AGAIN;
}
/*

    if (millis() > _timeout) {
        auto &json = getJsonObject();
        if (!json.size()) {
            auto &rows = json.addArray('l');
            for(auto &svc: *_services) {
                auto &row = rows.addObject(6);

                svc.domain.toLowerCase();
                row.add('h', svc.domain);

                String name = svc.domain;
                auto pos = name.indexOf('.');
                if (pos != -1) {
                    name.remove(pos);
                }
                name.toUpperCase();
                row.add('n', name);

                row.add('a', implode_cb(',', svc.addresses, [](const IPAddress &addr) {
                    return addr.toString();
                }));

                auto version = String('-');
                for(const auto &item: svc.map) {
                    if (item.first.length() == 1) {
                        char ch = item.first.charAt(0);
                        switch(ch) {
                            case 'v':
                                version = item.second;
                                break;
                            case 'b':
                            case 't':
                                row.add(ch, item.second);
                                break;
                        }
                    }
                    // if (item.first.equals(String('v'))) {
                    //     version = item.second;
                    // }
                    // else if (item.first.equals(String('b'))) {
                    //     row.add('b', item.second);
                    // }
                    // else if (item.first.equals(String('t'))) {
                    //     row.add('t', item.second);
                    // }
                }
                row.add('v', version);
            }
        }
        return AsyncJsonResponse::_fillBuffer(data, len);
    }
    else {
        return RESPONSE_TRY_AGAIN;
    }
}
*/
#endif

#endif

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

#if DEBUG_ASYNC_WEB_RESPONSE_DIR_RESPONSE
#include <debug_helper_enable.h>
#endif

AsyncDirResponse::AsyncDirResponse(const ListDir &dir, const String &dirName) :
    AsyncBaseResponse(true),
    _state(0),
    _dir(dir),
    _next(_dir.next()),
    _dirName(dirName)
{
    _code = 200;
    _contentType = FSPGM(mime_application_json);
    append_slash(_dirName);
    __LDBG_printf("dir=%s hiddenFiles=%u", _dirName.c_str(), _dir.showHiddenFiles());

}

bool AsyncDirResponse::_sourceValid() const
{
    return true;
}

size_t AsyncDirResponse::_sendBufferPartially(uint8_t *data, uint8_t *dataPtr, size_t len)
{
    size_t fill = len - (dataPtr - data);
    if (fill > _buffer.length()) {
        fill = _buffer.length();
    }
    if (fill) {
        memcpy(dataPtr, _buffer.c_str(), fill);
        dataPtr += fill;
        if (fill == _buffer.length()) {
            _buffer = PrintString();
        }
        else {
            _buffer.remove(0, fill);
        }
    }

    __LDBG_IF(
        DEBUG_OUTPUT.printf_P(PSTR("sending state=%u send=%u len=%u buffer=%u fill=%u data="), _state, (dataPtr - data), len, _buffer.length(), fill);
        printable_string(DEBUG_OUTPUT, data, (dataPtr - data), (dataPtr - data));
        DEBUG_OUTPUT.println();
    );
    return (dataPtr - data);
}

size_t AsyncDirResponse::_fillBuffer(uint8_t *data, size_t len)
{
    auto dataPtr = data;
    size_t space = len;
    __LDBG_printf("data=%p capacity=%u buffer=%u state=%u next=%u", data, len, _buffer.length(), _state, _next);

    // do we have something left in _buffer?
    if (_buffer.length()) {
        size_t bufferLen = _buffer.length();
        if (bufferLen >= len) {
            goto sendBuffer;
        }

        // we have more space available in the data buffer
        memcpy(data, _buffer.c_str(), bufferLen);
        _buffer = PrintString();
        dataPtr += bufferLen;
        space -= bufferLen;
        __LDBG_printf("state=%u capacity=%u space=%u", _state, len, space);
    }

    if (_state == 0) { // fill _buffer
        FSInfo info;
        SPIFFS_info(info);

        // if (String_endsWith(_dirName, '/')) {
        //     _dirName.remove(_dirName.length() - 1, 1);
        // }

        _buffer.printf_P(PSTR("{\"total\":\"%s\",\"total_b\":%d,\"used\":\"%s\",\"used_b\":%d,\"usage\":\"%.2f%%\",\"dir\":\"%s\",\"files\":["),
            formatBytes(info.totalBytes).c_str(), info.totalBytes,
            formatBytes(info.usedBytes).c_str(), info.usedBytes,
            (float)info.usedBytes / (float)info.totalBytes * 100.0,
            _dirName.c_str()
        );

        if (_next) {
            _state = 1; // read directory
            __LDBG_printf("set state=%u", _state);
        }
        else {
            _state = 2; // end
            __LDBG_printf("set state=%u", _state);
            _buffer.print(F("]}")); // empty directory
        }

        if (_buffer.length() >= space) {
            goto sendBuffer;
        }
    }

    if (_state == 1) {
        __LDBG_IF(auto maxDirs = 0xff);
        auto tmp_dir = sys_get_temp_dir();
        String path;
        while (_next) {
            path = std::move(_dir.fileName());
            String name = path.substring(_dirName.length());
            String location = url_encode(path);
            __LDBG_printf("dir=%s dir=%u file=%u name=%s", path.c_str(), _dir.isDirectory(), _dir.isFile(), name.c_str());

            if (_dir.isDirectory()) {
                remove_trailing_slash(name);
                remove_trailing_slash(location);

                _buffer.printf_P(PSTR("{\"f\":\"%s\",\"n\":\"%s\",\"m\":%d,\"d\":1"),
                    location.c_str(),
                    name.c_str(),
                    path.startsWith(tmp_dir) ? TYPE_TMP_DIR : (_dir.isMapping() ? TYPE_MAPPED_DIR : TYPE_REGULAR_DIR)
                );
            }
            else if (_dir.isFile()) {

                _buffer.printf_P(PSTR("{\"f\":\"%s\",\"n\":\"%s\",\"s\":\"%s\",\"sb\":%d,\"m\":%d,\"d\":0"),
                    location.c_str(),
                    name.c_str(),
                    formatBytes(_dir.fileSize()).c_str(),
                    _dir.fileSize(),
                    _dir.isMapping() ? TYPE_MAPPED_FILE : TYPE_REGULAR_FILE
                );
            }
            if (_dir.isMapping()) {
                _buffer.print(F(",\"t\":\""));
                _buffer.strftime_P(PSTR("%Y-%m-%d %H:%M"), _dir.fileTime());
                _buffer.print('"');
            }

            _next = _dir.next();
            if (_next) {
                _buffer.print(F("},"));
            } else {
                _state = 2; // end
                __LDBG_printf("set state=%u", _state);
                _buffer.print(F("}]}"));
            }

            __LDBG_IF(
                // limit to avoid wdt triggering
                if (--maxDirs == 0) {
                    break;
                }
            );

            size_t bufferLen = _buffer.length();
            if (bufferLen >= space) {
                break;
            }

            // cleanup _buffer for more directories
            memcpy(dataPtr, _buffer.c_str(), bufferLen);
            _buffer.remove(0, bufferLen); // does not change capacity
            dataPtr += bufferLen;
            space -= bufferLen;
        }
    }

sendBuffer:
    __LDBG_printf("state=%u capacity=%u space=%u buffer=%u", _state, len, space, _buffer.length());
    return _sendBufferPartially(data, dataPtr, len); // send what fits in data
}

#if DEBUG_ASYNC_WEB_RESPONSE_DIR_RESPONSE
#include <debug_helper_disable.h>
#endif


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
            __LDBG_printf("AsyncNetworkScanResponse %d >= %d, EOF", (int)_position, (int)n);
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
                tmp.replace(String('"'), F("\\\""));
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
                KFCFWConfiguration::getWiFiEncryptionType(WiFi.encryptionType(_position))
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
        // __LDBG_printf("chunk %d %-*.*s", ll, ll, ll, data);
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

// AsyncBufferResponse::AsyncBufferResponse(const String & contentType, Buffer * buffer, AwsTemplateProcessor templateCallback) : AsyncBaseResponse(false)
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
    __LDBG_delete(_webTemplate);
}

AsyncSpeedTestResponse::AsyncSpeedTestResponse(const String &contentType, uint32_t size) :
    AsyncBaseResponse(false),
    _size(sizeof(_header)),
    _header({})
{
    uint16_t width = sqrt(size / 2);
    _size += width * width * 2;
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


AsyncFillBufferCallbackResponse::AsyncFillBufferCallbackResponse(Callback callback) :
    AsyncBaseResponse(true),
    _callback(callback),
    _finished(false),
    _async(new bool())
{
    _code = 200;
    _contentType = FSPGM(mime_text_html);
    *_async = true; // mark this as alive
    _callback(_async, false, this);
}

AsyncFillBufferCallbackResponse::~AsyncFillBufferCallbackResponse()
{
    if (_async) {
        *_async = false; // mark this as dead
    }
}

void AsyncFillBufferCallbackResponse::finished(bool *async, AsyncFillBufferCallbackResponse *response)
{
    if (*async) {
        response->_async = nullptr;
        response->_finished = true;
    }
    __LDBG_delete(async);
}

bool AsyncFillBufferCallbackResponse::_sourceValid() const
{
    return true;
}

size_t AsyncFillBufferCallbackResponse::_fillBuffer(uint8_t *data, size_t len)
{
    if (_finished || _buffer.length()) { // finished or any new data?
        if (len > _buffer.length()) {
            len = _buffer.length();
        }
        if (len) {
            memcpy(data, _buffer.begin(), len);
            _buffer.remove(0, len);
            if (_async) {
                _callback(_async, true, this);
            }
        }
        return len;
    }
    else {
        if (_async) {
            _callback(_async, true, this);
        }
        return RESPONSE_TRY_AGAIN;
    }
}


AsyncResolveZeroconfResponse::AsyncResolveZeroconfResponse(const String &value) : AsyncFillBufferCallbackResponse([value](bool *async, bool fillBuffer, AsyncFillBufferCallbackResponse *response) {
    if (*async) { // indicator that "response" still exists
        if (!fillBuffer) { // we don't do refills
            reinterpret_cast<AsyncResolveZeroconfResponse *>(response)->_doStuff(async, value);
        }
    }
})
{
}

void AsyncResolveZeroconfResponse::_doStuff(bool *async, const String &value)
{
    uint32_t start = millis();
    if (!config.resolveZeroConf(String(), value, 0, [this, async, value, start](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) {
        if (*async) {
            PrintHtmlEntitiesString str;
            switch(type) {
                case MDNSResolver::ResponseType::RESOLVED:
                    str.print(F("Result for "));
                    break;
                default:
                case MDNSResolver::ResponseType::TIMEOUT:
                    str.print(F("Timeout resolving "));
                    break;
            }
            str.print(value);
            String result;
            PGM_P resultType;
            if (address.isSet()) {
                result = address.toString();
                resultType = SPGM(Address);

            } else {
                result = hostname;
                resultType = SPGM(Hostname);
            }
            str.printf_P(PSTR(HTML_S(br) HTML_S(br) "%s: %s" HTML_S(br) "Port: %u"), resultType, result.c_str(), port);
            if (result != resolved) {
                str.printf_P(PSTR(HTML_S(br) "Resolved: %s"), resolved.c_str());
            }
            str.printf_P(PSTR(HTML_S(br) "Timeout: %u / %ums"), get_time_diff(start, millis()), KFCConfigurationClasses::System::Device::getConfig().zeroconf_timeout);

            _buffer = std::move(str);
        }
        finished(async, this);
    })) {
        if (*async) {
            PrintHtmlEntitiesString str;
            str.printf_P(PSTR("Required Format:" HTML_S(br) "%s<service>.<proto>:<address|value[:port value]>|<fallback[:port]>}"), SPGM(_var_zeroconf));
            _buffer = std::move(str);
        }
        finished(async, this);
    }
}
