/**
  Author: sascha_lammers@gmx.de
*/

#include "async_web_response.h"
#include <PrintHtmlEntitiesString.h>
#include <MicrosTimer.h>
#include <misc.h>
#include <save_crash.h>
#include "fs_mapping.h"
#include "web_server.h"
#include "stl_ext/algorithm.h"
#include "../src/plugins/plugins.h"

#if DEBUG_ASYNC_WEB_RESPONSE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
// enable partial debugging here
#    define DEBUG_ASYNC_WEB_RESPONSE_DIR_RESPONSE 0
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
    }
    else {
        _httpHeaders.remove(F("Content-Length"));
    }
    if (_contentType.length()) {
        _httpHeaders.replace<HttpContentType>(_contentType);
    }

    _httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::ConnectionType::CLOSE);

    for(const auto &header : _headers) {
        out.printf_P(PSTR("%s: %s\r\n"), header->name().c_str(), header->value().c_str());
    }
    _headers.free();

    if (version) {
        out.printf_P(PSTR("%s: %s\r\n"), PSTR("Accept-Ranges"), PSTR("none"));
        if (_chunked) {
            out.printf_P(PSTR("%s: %s\r\n"), PSTR("Transfer-Encoding"), PSTR("chunked"));
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
            __DBG_printf_E("memory allocation failed");
            return 0;
        }

        size_t readLen = 0;
        if (_chunked) {
            buf += 6;
            if ((readLen = _fillBuffer(buf + headLen, outLen - 8)) == RESPONSE_TRY_AGAIN) {
                return 0;
            }
            char headBuf[16];
            size_t headLength;
            headLength = snprintf_P(headBuf, sizeof(headBuf), PSTR("%x\r\n"), readLen);
            buf -= headLength;
            memcpy(buf + headLen, headBuf, headLength);
            outLen = readLen + headLen + headLength;
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
    return _content;
}

size_t AsyncProgmemFileResponse::_fillBuffer(uint8_t *data, size_t len)
{
    return _content.read(data, len);
}

#if DEBUG_ASYNC_WEB_RESPONSE_DIR_RESPONSE
#include <debug_helper_enable.h>
#endif

AsyncDirResponse::AsyncDirResponse(const String &dirName, bool showHiddenFiles) :
    AsyncBaseResponse(true),
    _dir(dirName, true, showHiddenFiles),
    _dirName(dirName),
    _state(StateType::FILL),
    _next(_dir.next())
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
        _buffer.remove(0, fill); // does not change capacity
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
        _buffer.remove(0, bufferLen); // does not change capacity
        dataPtr += bufferLen;
        space -= bufferLen;
        __LDBG_printf("state=%u capacity=%u space=%u", _state, len, space);
    }

    if (_state == StateType::FILL) { // fill _buffer
        FSInfo info;
        KFCFS.info(info);

        char bufTotalBytes[16];
        char bufUsedBytes[16];
        formatBytes(bufTotalBytes, sizeof(bufTotalBytes), info.totalBytes);
        formatBytes(bufUsedBytes, sizeof(bufUsedBytes), info.usedBytes);

        // if (String_endsWith(_dirName, '/')) {
        //     _dirName.remove(_dirName.length() - 1, 1);
        // }

        _buffer.printf_P(PSTR("{\"t\":\"%s\",\"T\":%d,\"u\":\"%s\",\"U\":%d,\"p\":\"%.2f%%\",\"d\":\"%s\",\"f\":["),
            bufTotalBytes, info.totalBytes,
            bufUsedBytes, info.usedBytes,
            (info.usedBytes * 100) / static_cast<float>(info.totalBytes),
            _dirName.c_str()
        );

        if (_next) {
            _state = StateType::READ_DIR;
            __LDBG_printf("set state=%u", _state);
        }
        else {
            _state = StateType::END;
            __LDBG_printf("set state=%u", _state);
            _buffer.print(F("]}")); // empty directory
        }

        if (_buffer.length() >= space) {
            goto sendBuffer;
        }
    }

    if (_state == StateType::READ_DIR) {
        auto tmp_dir = sys_get_temp_dir();
        while (_next) {
            const String &path = _dir.fileName();
            const char *name = path.c_str() + _dirName.length();
            __LDBG_printf("dir=%s dir=%u file=%u name=%s", path.c_str(), _dir.isDirectory(), _dir.isFile(), name);

            size_t nameLength = path.length() - _dirName.length();
            if (nameLength && name[nameLength - 1] == '/') {
                nameLength--;
            }

            if (_dir.isDirectory()) {
                size_t pathLength = path.length();
                if (pathLength && name[pathLength - 1] == '/') {
                    pathLength--;
                }

                _buffer.print(F("{\"f\":\""));
                appendUrlEncoded(_buffer, path.c_str(), pathLength);
                _buffer.printf_P(PSTR("\",\"n\":\"%*.*s\",\"m\":%d,\"d\":1"),
                    nameLength, nameLength, name,
                    path.startsWith(tmp_dir) ? PathType::TMP_DIR : (_dir.isMapping() ? PathType::MAPPED_DIR : PathType::DIR)
                );
            }
            else if (_dir.isFile()) {

                char buf[16];
                formatBytes(buf, sizeof(buf), _dir.fileSize());

                _buffer.print(F("{\"f\":\""));
                appendUrlEncoded(_buffer, path.c_str(), path.length());
                _buffer.printf_P(PSTR("\",\"n\":\"%*.*s\",\"s\":\"%s\",\"b\":%d,\"m\":%d,\"d\":0"),
                    nameLength, nameLength, name,
                    buf,
                    _dir.fileSize(),
                    _dir.isMapping() ? PathType::MAPPED_FILE : PathType::FILE
                );

            }

            // add file creation time for directories and files, if available
            if (_dir.isMapping() || _dir.fileTime()) {
                _buffer.print(F(",\"t\":\""));
                _buffer.strftime_P(PSTR("%Y-%m-%d %H:%M\""), _dir.fileTime());
            }

            _next = _dir.next();
            if (_next) {
                _buffer.print(F("},"));
            }
            else {
                _state = StateType::END;
                __LDBG_printf("set state=%u", _state);
                _buffer.print(F("}]}"));
            }

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

int32_t AsyncNetworkScanResponse::_strcpy_P_safe(char *&dst, PGM_P str, int32_t &space)
{
    const int32_t length = static_cast<int32_t>(strlen_P(str));
    if (length >= space) {
        return 0;
    }
    memcpy_P(dst, str, length);
    dst[length] = 0;
    space -= length;
    dst += length;
    return length;
}

size_t AsyncNetworkScanResponse::_fillBuffer(uint8_t *data, size_t len)
{
    if (_position == (uint8_t)-1) {
        __LDBG_printf("AsyncNetworkScanResponse pos == -1, EOF");
        return 0;
    }
    int8_t n = WiFi.scanComplete();
    if (n < 0) {
        if (!isLocked() && n == -2) { // no scan running and no results available
            setLocked();
            WiFi.scanNetworks(true, _hidden);
        }
        _position = -1;
        __LDBG_printf("Scan running");
        int32_t space = (int32_t)len;
        auto dst = reinterpret_cast<char *>(data);
        return _strcpy_P_safe(dst, PSTR("{\"p\":true,\"m\":\"Network scan still running\"}"), space);
    }
    else if (n == 0) {
        _position = -1;
        __LDBG_printf("No networks in range");
        int32_t space = (int32_t)len;
        auto dst = reinterpret_cast<char *>(data);
        return _strcpy_P_safe(dst, PSTR("{\"m\":\"No WiFi networks in range\"}"), space);
    }
    else {
        if (_position >= n) {
            __LDBG_printf("AsyncNetworkScanResponse %d >= %d, EOF", (int)_position, (int)n);
            return 0;
        }
        auto ptr = reinterpret_cast<char *>(data);
        auto sptr = ptr;
        int32_t space = len - 2; // reserve 2 bytes
        if (_position == 0) {
            _strcpy_P_safe(ptr, PSTR("{\"r\":["), space);
        }
        uint16_t l;
        while (_position < n && space > 0) {
            _strcpy_P_safe(ptr, PSTR("{\"t\":\""), space);
            if (WiFi_isHidden(_position)) {
                _strcpy_P_safe(ptr, PSTR("table-secondary"), space);
            }
            else {
                _strcpy_P_safe(ptr, PSTR("has-network-name\",\"d\":\"network-name"), space);
            }
            _strcpy_P_safe(ptr, PSTR("\",\"s\":\""), space);
            if (WiFi_isHidden(_position)) {
                _strcpy_P_safe(ptr, PSTR("<i>HIDDEN</i>"), space);
            }
            else {
                PrintString ssid;
                KFCJson::JsonTools::printToEscaped(ssid, WiFi.SSID(_position));
                if (static_cast<int16_t>(ssid.length()) >= space) {
                    break;
                }
                strcpy(ptr, ssid.c_str());
                ptr += ssid.length();
                space -= ssid.length();
            }
            if ((l = snprintf_P(ptr, space, PSTR("\",\"c\":%d,\"r\":%d,\"b\":\"%s\",\"e\":\"%s\"},"),
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

#if ESP32

AsyncCoreDumpResponse::AsyncCoreDumpResponse(const String &contentType) :
    AsyncBaseResponse(false),
    _size(SaveCrash::CoreDump::getSize()),
    _offset(0)
{
    _code = 200;
    _contentLength = _size;
    _sendContentLength = true;
    _chunked = false;
    _contentType = contentType;
}

bool AsyncCoreDumpResponse::_sourceValid() const
{
    return _size != 0;
}

size_t AsyncCoreDumpResponse::_fillBuffer(uint8_t *buf, size_t maxLen)
{
    // esp_partition_read() requires an aligned destination, use the internal buffer
    auto readLen = std::min(maxLen, sizeof(_buffer));
    auto read = SaveCrash::CoreDump::read(_offset, _buffer, readLen);
    if (read) {
        memcpy(buf, _buffer, read);
        _offset += read;
    }
    return read;
}

#endif


AsyncFillBufferCallbackResponse::AsyncFillBufferCallbackResponse(const Callback &callback) :
    AsyncBaseResponse(true),
    _callback(callback),
    _finished(false),
    _async(new bool())
{
    if (!_async) {
        __DBG_printf_E("memory allocation failed");
    }
    _code = 200;
    _contentType = FSPGM(mime_text_html);
    if (_async) {
        *_async = true; // mark this as alive
    }
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
    delete async;
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
                switch (type) {
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
                if (IPAddress_isValid(address)) {
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
                str.printf_P(PSTR(HTML_S(br) "Timeout: %u / %ums"), get_time_since(start, millis()), KFCConfigurationClasses::System::Device::getConfig().zeroconf_timeout);

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
