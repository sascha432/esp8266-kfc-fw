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

// print formatted output directly into the output window. Print::printf() formats into a 64 byte
// stack buffer and allocates for anything longer, this formats into the window and only allocates
// when the output does not fit into the stack buffer
static void _windowPrintf_P(PrintBuffer &out, PGM_P format, ...)
{
    char buf[128];
    va_list arg;
    va_start(arg, format);
    auto len = vsnprintf_P(buf, sizeof(buf), format, arg);
    va_end(arg);
    if (len < 0) {
        return;
    }
    if (len < static_cast<int>(sizeof(buf))) {
        out.write(reinterpret_cast<const uint8_t *>(buf), len);
        return;
    }
    // very long output, format it into the window itself
    if (!out.reserve(out.length() + len + 1)) {
        __DBG_printf_E("memory allocation failed");
        return;
    }
    va_start(arg, format);
    len = vsnprintf_P(reinterpret_cast<char *>(out.end()), out.size() - out.length(), format, arg);
    va_end(arg);
    if (len > 0) {
        out.advance(len);
    }
}

AsyncBaseResponse::AsyncBaseResponse(bool chunked) :
    _out(),
    _outSent(0),
    _sourceDone(false)
{
    if (chunked) {
        // change those 2 values for chunked
        _sendContentLength = false;
        _chunked = true;
    }
}

void AsyncBaseResponse::__assembleHead(uint8_t version)
{
    // the whole response is built in the output window, the head needs no extra String and the
    // window is only allocated once
    if (!_out.reserve(kMaxFrameSize)) {
        __DBG_printf_E("memory allocation failed");
    }
    _windowPrintf_P(_out, PSTR("HTTP/1.%d %d %s\r\n"), version, _code, _responseCodeToString(_code));

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
        _windowPrintf_P(_out, PSTR("%s: %s\r\n"), header->name().c_str(), header->value().c_str());
    }
    _headers.free();

    if (version) {
        _windowPrintf_P(_out, PSTR("%s: %s\r\n"), PSTR("Accept-Ranges"), PSTR("none"));
        if (_chunked) {
            _windowPrintf_P(_out, PSTR("%s: %s\r\n"), PSTR("Transfer-Encoding"), PSTR("chunked"));
        }
    }

    for(const auto &header : _httpHeaders) {
        header->printTo(_out);
    }
    _httpHeaders.clear();

    _out.println();
    _headLength = _out.length();
    _outSent = 0;
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
    __LDBG_printf("ack: state=%u space=%u sent=%u written=%u acked=%u left=%u", (unsigned)_state, (unsigned)request->client()->space(), (unsigned)_sentLength, (unsigned)_writtenLength, (unsigned)_ackedLength, (unsigned)_outPending());

    // The space() of a connection is the free room of its send buffer and it changes while the
    // response runs: producing the content can take seconds (rendering a form, resolving a
    // template) and the stack can be short of buffers (MQTT, websocket, ...). Handing more than the
    // current space() to write() silently drops the excess -> the response is truncated, the
    // accounting is wrong and the connection stays open (the client waits for data that was never
    // queued, the abandoned connection keeps buffers and PCBs busy). So only what fits is handed to
    // the socket and the rest stays in the window for the next call
    auto written = _flushOut(request);

    // This method is called when the client acknowledged data or when the connection was polled -
    // both mean that the send buffer has room again. The buffer is filled up to its current space
    // (tcp_sndbuf, about 5.7 KB) instead of handing over a single frame: with one frame per call the
    // window stays almost empty and the transfer waits for an acknowledgement after every 1 KB,
    // which costs most of the possible rate (measured: 15 KB/s instead of 300 KB/s). Producing the
    // content can be slow (a template token, a rendered form), so one call gives up after
    // kMaxProduceTime and the next acknowledgement or poll continues where the call stopped
    auto deadline = millis() + kMaxProduceTime;

    for (;;) {
        if (_state == RESPONSE_HEADERS) {
            if (_outPending()) {
                break; // the head was not completely handed over yet
            }
            _state = RESPONSE_CONTENT;
        }

        if (_state == RESPONSE_CONTENT) {
            if (_outPending()) {
                break; // no room yet, wait for the next acknowledgement
            }
            if (_sourceDone) {
                _state = RESPONSE_WAIT_ACK;
                break;
            }

            // one frame at a time, see kMaxFrameSize
            auto payloadMax = kMaxFrameSize - (_chunked ? (kChunkHeaderSize + 2) : 0);
            if (_sendContentLength) {
                auto remaining = (_contentLength > _sentLength) ? (_contentLength - _sentLength) : 0;
                if (remaining < payloadMax) {
                    payloadMax = remaining;
                }
                if (!payloadMax) {
                    // everything was sent, only the acknowledgement is missing
                    _sourceDone = true;
                    _state = RESPONSE_WAIT_ACK;
                    break;
                }
            }
            if (!_out.reserve(kMaxFrameSize)) {
                __DBG_printf_E("memory allocation failed");
                _state = RESPONSE_FAILED;
                request->client()->close();
                return written;
            }
            // the content is written behind the chunk header and the header is put in front of it,
            // so the content never has to be moved
            auto payload = _out.end() + (_chunked ? kChunkHeaderSize : 0);
            auto readLen = _fillBuffer(payload, payloadMax);
            if (readLen == RESPONSE_TRY_AGAIN) {
                break; // the content is not ready yet, nothing was added to the window
            }
            if (_chunked) {
                char hdr[kChunkHeaderSize + 1];
                auto hdrLength = snprintf_P(hdr, sizeof(hdr), PSTR("%05x\r\n"), readLen);
                if (hdrLength >= static_cast<int>(kChunkHeaderSize)) {
                    hdrLength = kChunkHeaderSize;
                }
                memcpy(_out.end(), hdr, hdrLength);
                payload[readLen] = '\r';
                payload[readLen + 1] = '\n';
                _out.advance(kChunkHeaderSize + readLen + 2);
            }
            else {
                _out.advance(readLen);
            }
            _sentLength += readLen;
            if (!readLen) {
                // chunked: the empty chunk terminated the body - unknown content length: end of stream
                _sourceDone = true;
            }
            __LDBG_printf("frame: readLen=%u sent=%u written=%u pending=%u", (unsigned)readLen, (unsigned)_sentLength, (unsigned)_writtenLength, (unsigned)_outPending());

            written += _flushOut(request);
            // The send buffer can be full although the content is complete. Only the acknowledgement
            // of the last bytes ends the response, otherwise it would finish although most of it was
            // never handed to the client
            if (_sourceDone && !_outPending()) {
                _state = RESPONSE_WAIT_ACK;
                break;
            }
            if (_outPending()) {
                break; // the frame did not fit into the send buffer, wait for room
            }
            if (_ackedLength >= _writtenLength) {
                break; // nothing is in flight that would call this method again
            }
            if (static_cast<int32_t>(millis() - deadline) >= 0) {
                break; // keep the network task responsive, the next call continues
            }
            continue; // the send buffer has room, hand over another frame
        }

        if (_state == RESPONSE_WAIT_ACK) {
            // The response is complete when everything was handed to the socket and acknowledged by
            // the client. The acknowledgement is required before the connection is closed: closing it
            // while the stack still has data it could not send makes `AsyncClient::_close()` call
            // `abort()` (`ERR_MEM` from `tcp_close()`), and the RST can drop the tail of the response
            if (!_outPending() && _ackedLength >= _writtenLength) {
                _state = RESPONSE_END;
                // Every response announces "Connection: close" (`HttpConnectionHeader`), so the
                // connection is closed here instead of waiting for the client to give up - it would
                // sit in the read until its own socket timeout to see the end of the response, and an
                // abandoned connection keeps a PCB and buffers of the stack busy. The disconnect
                // handler deletes the request and this response, nothing of this object may be
                // touched afterwards
                request->client()->close();
            }
        }
        break;
    }
    return written;
}

size_t AsyncBaseResponse::_flushOut(AsyncWebServerRequest *request)
{
    auto pending = _outPending();
    if (!pending) {
        return 0;
    }
    auto len = std::min(request->client()->space(), pending);
    if (len > kMaxFrameSize) {
        // never hand more than one segment to the network stack, the rest stays in the window
        len = kMaxFrameSize;
    }
    if (!len) {
        return 0;
    }
    // ASYNC_WRITE_FLAG_COPY (0x01 on the ESP32 and the ESP8266 stack) makes the stack own a copy of
    // the bytes: the window is reused for the next frame and, without the flag, the ESP8266 stack
    // keeps a pointer into it until the data was acknowledged
    auto written = request->client()->write(reinterpret_cast<const char *>(_out.begin() + _outSent), len, ASYNC_WRITE_FLAG_COPY);
    _outSent += written;
    _writtenLength += written;
    if (_outSent >= _out.length()) {
        // everything was handed over, the window is reused from the beginning
        _out.setLength(0);
        _outSent = 0;
    }
    __LDBG_printf("flush: len=%u written=%u left=%u space=%u", (unsigned)len, (unsigned)written, (unsigned)_outPending(), (unsigned)request->client()->space());
    return written;
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
    auto readLen = _content.read(data, len);
    __LDBG_printf("fillBuffer(%u) -> %d", (unsigned)len, (int)readLen);
    return readLen;
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
    // a directory entry is a few hundred bytes, the reservation keeps the buffer at one allocation
    _buffer.reserve(256);
    append_slash(_dirName);
    __LDBG_printf("dir=%s hiddenFiles=%u", _dirName.c_str(), _dir.showHiddenFiles());
}

bool AsyncDirResponse::_sourceValid() const
{
    return true;
}

size_t AsyncDirResponse::_fillBuffer(uint8_t *data, size_t len)
{
    auto dataPtr = data;
    auto space = len;
    __LDBG_printf("data=%p capacity=%u buffer=%u state=%u next=%u", data, len, _buffer.length(), _state, _next);

    while (space) {
        // send what was generated before (a fragment that did not fit into the previous buffer)
        if (_buffer.length()) {
            size_t fill = std::min(space, _buffer.length());
            memcpy(dataPtr, _buffer.c_str(), fill);
            _buffer.remove(0, fill); // does not change capacity
            dataPtr += fill;
            space -= fill;
            if (!space) {
                break;
            }
        }
        if (_state == StateType::END) {
            break;
        }
        if (_state == StateType::FILL) { // the beginning of the response
            FSInfo info;
            getFSInfo(info);

            char bufTotalBytes[16];
            char bufUsedBytes[16];
            formatBytes(bufTotalBytes, sizeof(bufTotalBytes), info.totalBytes);
            formatBytes(bufUsedBytes, sizeof(bufUsedBytes), info.usedBytes);

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
            continue;
        }

        // StateType::READ_DIR, one entry per iteration
        if (!_next) {
            _state = StateType::END;
            __LDBG_printf("set state=%u", _state);
            continue;
        }

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
                path.startsWith(sys_get_temp_dir()) ? PathType::TMP_DIR : (_dir.isMapping() ? PathType::MAPPED_DIR : PathType::DIR)
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
    }

    __LDBG_printf("state=%u capacity=%u space=%u buffer=%u send=%u", _state, len, space, _buffer.length(), (dataPtr - data));
    return (dataPtr - data); // send what fits in data
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
