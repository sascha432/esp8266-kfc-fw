/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <map>
#include <functional>
#include <KFCTimezone.h>
#include <Buffer.h>
#include "templates.h"
#include "fs_mapping.h"

#define DEBUG_ASYNC_WEB_RESPONSE 1

class AsyncProgmemFileResponse : public AsyncAbstractResponse {
public:
    AsyncProgmemFileResponse(const String &contentType, FSMapping *mapping, AwsTemplateProcessor templateCallback = nullptr);
    bool _sourceValid() const;
    virtual size_t _fillBuffer(uint8_t *data, size_t len) override;

private:
    File _content;
};

class AsyncTemplateResponse : public AsyncProgmemFileResponse {
public:
    AsyncTemplateResponse(const String &contentType, FSMapping *mapping, WebTemplate *webTemplate);
    virtual ~AsyncTemplateResponse();

    String process(const String &key) {
        return _webTemplate->process(key);
    }

private:
    WebTemplate *_webTemplate;
};

class AsyncBufferResponse : public AsyncAbstractResponse {
public:
    AsyncBufferResponse(const String &contentType, Buffer *buffer, AwsTemplateProcessor templateCallback = nullptr);
    virtual ~AsyncBufferResponse();

    bool _sourceValid() const;

    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override;


private:
    Buffer *_content;
    size_t _position;
};

class AsyncDirWrapper {
    typedef std::vector<String> AsyncDirWrapperList;

    static const uint8_t DIR_TYPE_INVALID =         0;
    static const uint8_t DIR_TYPE_DIRECTORY =       1;
    static const uint8_t DIR_TYPE_FILE =            2;

public:
    AsyncDirWrapper();
    AsyncDirWrapper(const String &dirName);
    virtual ~AsyncDirWrapper();

    void setVirtualRoot(const String &path);

    String &getDirName();;

    bool isValid();
    bool _fileInside(const String &path);
    bool isDir();
    bool isFile();
    bool next();

    File openFile(const char *mode);
    String fileName();
    String mappedFile();
    bool isMapped();
    void getModificatonTime(char *modified, size_t size, PGM_P format);
    size_t fileSize();

private:
    bool _isValid;
    bool _firstNext;
    uint8_t _type;
    Dir _dir;
    String _dirName;
    String _fileName;
    String _virtualRoot;
    const FSMapping *_curMapping;
    FileMappingsListIterator _iterator;
    FileMappingsListIterator _begin;
    FileMappingsListIterator _end;
    AsyncDirWrapperList _dirs;
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
    int _dirNameLen;
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
    uint8_t _position;
    bool _hidden;
    bool _done;
    static bool _locked;
};
