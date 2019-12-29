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
#include "web_server.h"
#include "../include/templates.h"
#include "fs_mapping.h"
#include "bitmap_header.h"

class AsyncJsonResponse : public AsyncAbstractResponse {
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

class AsyncProgmemFileResponse : public AsyncAbstractResponse {
public:
    AsyncProgmemFileResponse(const String &contentType, FSMapping *mapping, AwsTemplateProcessor templateCallback = nullptr);

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    File _content;
    WebServerSetCPUSpeedHelper _setCPUSpeed;
};

class AsyncTemplateResponse : public AsyncProgmemFileResponse {
public:
    AsyncTemplateResponse(const String &contentType, FSMapping *mapping, WebTemplate *webTemplate);
    virtual ~AsyncTemplateResponse();

    void process(const String &key, PrintHtmlEntitiesString &output) {
        _webTemplate->process(key, output);
    }

private:
    WebTemplate *_webTemplate;
    WebServerSetCPUSpeedHelper _setCPUSpeed;
};

class AsyncSpeedTestResponse : public AsyncAbstractResponse {
public:
    AsyncSpeedTestResponse(const String &contentType, uint32_t size); // size is pow(floor(sqrt(size / 2), 2) + 54

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override;

private:
    int32_t _size;
    BitmapFileHeader_t _header;
    WebServerSetCPUSpeedHelper _setCPUSpeed;
};

class AsyncBufferResponse : public AsyncAbstractResponse {
public:
    AsyncBufferResponse(const String &contentType, Buffer *buffer, AwsTemplateProcessor templateCallback = nullptr);
    virtual ~AsyncBufferResponse();

    virtual bool _sourceValid() const override;
    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override;

private:
    Buffer *_content;
    size_t _position;
    WebServerSetCPUSpeedHelper _setCPUSpeed;
};

class AsyncDirWrapper {
public:
    typedef std::vector<String> AsyncDirWrapperVector;

    typedef enum {
        INVALID = 0,
        DIR,
        FILE
    } TypeEnum_t;

public:
    AsyncDirWrapper();
    AsyncDirWrapper(const String &dirName);
    virtual ~AsyncDirWrapper();

    void setVirtualRoot(const String &path);

    String &getDirName();;

    bool isValid() const;
    bool _fileInside(const String &path);
    bool isDir() const;
    bool isFile() const;
    bool next();

    File openFile(const char *mode);
    String fileName();
    String mappedFile();
    bool isMapped() const;
    void getModificatonTime(char *modified, size_t size, PGM_P format);
    size_t fileSize();

private:
    bool _isValid;
    bool _firstNext;
    TypeEnum_t _type;
    Dir _dir;
    String _dirName;
    String _fileName;
    String _virtualRoot;
    const FSMapping *_curMapping;
    FileMappingsListIterator _iterator;
    FileMappingsListIterator _begin;
    FileMappingsListIterator _end;
    AsyncDirWrapperVector _dirs;
};

class AsyncDirResponse : public AsyncAbstractResponse {
public:
    static const uint8_t TYPE_TMP_DIR =         2;
    static const uint8_t TYPE_MAPPED_DIR =      1;
    static const uint8_t TYPE_REGULAR_DIR =     0;
    static const uint8_t TYPE_MAPPED_FILE =     1;
    static const uint8_t TYPE_REGULAR_FILE =    0;

    AsyncDirResponse(const AsyncDirWrapper &dir);

    bool _sourceValid() const;

    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    uint8_t _state;
    bool _next;
    AsyncDirWrapper _dir;
    String _dirName;
    size_t _dirNameLen;
    WebServerSetCPUSpeedHelper _setCPUSpeed;
};

class AsyncNetworkScanResponse : public AsyncAbstractResponse {
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
    WebServerSetCPUSpeedHelper _setCPUSpeed;
};
