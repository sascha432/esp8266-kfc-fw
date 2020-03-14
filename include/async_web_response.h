/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_ASYNC_WEB_RESPONSE
#define DEBUG_ASYNC_WEB_RESPONSE            0
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <KFCJson.h>
#include <vector>
#include <map>
#include <functional>
#include <KFCTimezone.h>
#include <Buffer.h>
#include <PrintHtmlEntities.h>
#include <TemplateDataProvider.h>
#include <SSIProxyStream.h>
#include "web_server.h"
#include "../include/templates.h"
#include "../lib/KFCGfx/include/bitmap_header.h"

class HttpHeaders;

class AsyncBaseResponse : public AsyncWebServerResponse {
public:
    AsyncBaseResponse(bool chunked);

    virtual void _respond(AsyncWebServerRequest* request);
    virtual size_t _ack(AsyncWebServerRequest* request, size_t len, uint32_t time);

    virtual size_t _fillBuffer(uint8_t* buf, size_t maxLen) = 0;

protected:
    friend HttpHeaders;

    void __assembleHead(uint8_t version);

    WebServerSetCPUSpeedHelper _setCPUSpeed;
    HttpHeadersVector _httpHeaders;
    String _head;
};

class AsyncJsonResponse : public AsyncBaseResponse {
public:
    AsyncJsonResponse();

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

    JsonUnnamedObject &getJsonObject();
    void updateLength();

private:
    JsonUnnamedObject _json;
    JsonBuffer _jsonBuffer;
};

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

    AsyncDirResponse(const Dir &dir, const String &dirName);

    bool _sourceValid() const;

    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    uint8_t _state;
    bool _next;
    Dir _dir;
    String _dirName;
};

class AsyncNetworkScanResponse : public AsyncBaseResponse {
public:
    AsyncNetworkScanResponse(bool hidden);
    virtual ~AsyncNetworkScanResponse();

    bool _sourceValid() const;
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
