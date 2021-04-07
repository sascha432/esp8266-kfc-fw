/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_ASYNC_WEB_RESPONSE
#define DEBUG_ASYNC_WEB_RESPONSE            0
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
// #include <KFCJson.h>
#include <vector>
#include <map>
#include <functional>
#include <ListDir.h>
#include <Buffer.h>
#include <PrintHtmlEntities.h>
#include <TemplateDataProvider.h>
#include <SSIProxyStream.h>
#include "web_server.h"
#include "../include/templates.h"
#include "./plugins/mdns/mdns_plugin.h"
#include "bitmap_header.h"

class HttpHeaders;

class AsyncBaseResponse : public AsyncWebServerResponse {
public:
    AsyncBaseResponse(bool chunked);

    virtual void _respond(AsyncWebServerRequest* request);
    virtual size_t _ack(AsyncWebServerRequest* request, size_t len, uint32_t time);
    virtual size_t _fillBuffer(uint8_t* buf, size_t maxLen) = 0;

    void setHttpHeaders(HttpHeadersVector &&headers) {
        _httpHeaders.setHeaders(std::move(headers));
    }

protected:
    virtual void __assembleHead(uint8_t version);

    WebServerSetCPUSpeedHelper _setCPUSpeed;
    HttpHeaders _httpHeaders;
    String _head;
};

// class AsyncJsonResponse : public AsyncBaseResponse {
// public:
//     AsyncJsonResponse();

//     virtual bool _sourceValid() const override;
//     virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

//     JsonUnnamedObject &getJsonObject();

// protected:
//     virtual void __assembleHead(uint8_t version) override;

// private:
//     JsonUnnamedObject _json;
//     JsonBuffer _jsonBuffer;
// };

#if ESP32

class AsyncMDNSResponse : public AsyncBaseResponse {
public:
    AsyncMDNSResponse() :
        AsyncBaseResponse(true)
    {
        _contentLength = 0;
    }
    ~AsyncMDNSResponse() {
    }

    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;
};

#else

#if MDNS_PLUGIN

class AsyncMDNSResponse : public AsyncBaseResponse {
public:
    AsyncMDNSResponse(MDNSPlugin::Output *output) :
        AsyncBaseResponse(true),
        _output(output)
    {
        _contentLength = 0;
    }
    ~AsyncMDNSResponse() {
        delete _output;
    }

    virtual bool _sourceValid() const override {
        return true;
    }

    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    MDNSPlugin::Output *_output;
};

#endif

#endif

class AsyncProgmemFileResponse : public AsyncBaseResponse {
public:
    AsyncProgmemFileResponse(const String &contentType, const File &file, TemplateDataProvider::ResolveCallback callback = nullptr);

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    File _contentWrapped;
    TemplateDataProvider _provider;
    SSIProxyStream _content;
};

class AsyncTemplateResponse : public AsyncProgmemFileResponse {
public:
    AsyncTemplateResponse(const String &contentType, const File &file, WebTemplate *webTemplate, TemplateDataProvider::ResolveCallback callback = nullptr);
    virtual ~AsyncTemplateResponse();

private:
    WebTemplate *_webTemplate;
};

class AsyncSpeedTestResponse : public AsyncBaseResponse {
public:
    AsyncSpeedTestResponse(const String &contentType, uint32_t size); // size is pow(floor(sqrt(size / 2), 2) + 54

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override;

private:
    int32_t _size;
    BitmapFileHeader_t _header;
};

// class AsyncBufferResponse : public AsyncBaseResponse {
// public:
//     AsyncBufferResponse(const String &contentType, Buffer *buffer, AwsTemplateProcessor templateCallback = nullptr);
//     virtual ~AsyncBufferResponse();

//     virtual bool _sourceValid() const override;
//     virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override;

// private:
//     Buffer *_content;
//     size_t _position;
// };

class AsyncDirResponse : public AsyncBaseResponse {
public:
    static const uint8_t TYPE_TMP_DIR =         2;
    static const uint8_t TYPE_MAPPED_DIR =      1;
    static const uint8_t TYPE_REGULAR_DIR =     0;
    static const uint8_t TYPE_MAPPED_FILE =     1;
    static const uint8_t TYPE_REGULAR_FILE =    0;

    AsyncDirResponse(const ListDir &dir, const String &dirName);
    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    size_t _sendBufferPartially(uint8_t *data, uint8_t *dataPtr, size_t len);

private:
    uint8_t _state;
    ListDir _dir;
    bool _next;
    String _dirName;
    PrintString _buffer;
};

class AsyncNetworkScanResponse : public AsyncBaseResponse {
public:
    AsyncNetworkScanResponse(bool hidden);
    virtual ~AsyncNetworkScanResponse();

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

    static bool isLocked();
    static void setLocked(bool locked = true);

private:
    uint16_t _strcpy_P_safe(char *&dst, PGM_P str, int16_t &space);

    uint8_t _position;
    bool _hidden;
    bool _done;
    static bool _locked;
};

class AsyncFillBufferCallbackResponse : public AsyncBaseResponse {
public:
    // the destructor writes false into *async when executed
    // (*async) ? "ALIVE do Stuff" : "AsyncCallbackResponse has been destroyed... that's it we can go home"
    // the callback is responsible to deal with the async object. when done adding data to the buffer, call
    // AsyncCallbackResponse::finished(async, response). it updates the response and deletes the async object
    using Callback = std::function<void(bool *async, bool fillBuffer, AsyncFillBufferCallbackResponse *response)>;

public:
    AsyncFillBufferCallbackResponse(Callback callback);
    virtual ~AsyncFillBufferCallbackResponse();

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

    Buffer &getBuffer() {
        return _buffer;
    }
    static void finished(bool *async, AsyncFillBufferCallbackResponse *response);

protected:
    Buffer _buffer;
    Callback _callback;
    bool _finished;
    bool *_async;
};


class AsyncResolveZeroconfResponse : public AsyncFillBufferCallbackResponse {
public:
    AsyncResolveZeroconfResponse(const String &value);

private:
    void _doStuff(bool *async, const String &value);
};
