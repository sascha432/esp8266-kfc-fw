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

    inline AsyncJsonResponse *finalize() {
        updateLength();
        return this;
    }

private:
    JsonUnnamedObject _json;
    JsonBuffer _jsonBuffer;
};

#if ESP32

class AsyncMDNSResponse : public AsyncJsonResponse {
public:
    AsyncMDNSResponse() {
        _contentLength = 0;
        _sendContentLength = false;
        _chunked = true;
    }
    ~AsyncMDNSResponse() {
    }

    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;
};

#else

#if MDNS_PLUGIN

class AsyncMDNSResponse : public AsyncJsonResponse {
public:
    AsyncMDNSResponse(MDNSResponder::hMDNSServiceQuery serviceQuery, MDNSPlugin::ServiceInfoVector *services, int timeout) : _serviceQuery(serviceQuery), _services(services), _timeout(millis() + timeout) {
        _contentLength = 0;
        _sendContentLength = false;
        _chunked = true;
    }
    ~AsyncMDNSResponse() {
        MDNS.removeServiceQuery(_serviceQuery);
        delete _services;
    }

    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    MDNSResponder::hMDNSServiceQuery _serviceQuery;
    MDNSPlugin::ServiceInfoVector *_services;
    uint32_t _timeout;
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

    bool _sourceValid() const;

    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    uint8_t _state;
    ListDir _dir;
    bool _next;
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

class AsyncResolveZeroconfResponse : public AsyncBaseResponse {
public:
    AsyncResolveZeroconfResponse(const String &value);
    virtual ~AsyncResolveZeroconfResponse();

    bool _sourceValid() const;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    String _response;
    bool _finished;
    bool *_async;
};
