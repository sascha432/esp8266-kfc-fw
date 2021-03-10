/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if FILE_MANAGER

#ifndef DEBUG_FILE_MANAGER
#define DEBUG_FILE_MANAGER              0
#endif

#include <Arduino_compat.h>
#include "async_web_response.h"
#include "web_server.h"

class AsyncWebServerRequest;
class AsyncWebHandler;

class FileManager {
public:
    FileManager();
    FileManager(AsyncWebServerRequest *request, bool isAuthenticated, const String &uri);

    void handleRequest();
    uint16_t list();
    uint16_t mkdir();
    uint16_t upload();
    uint16_t view(bool isDownload = false);
    uint16_t remove();
    uint16_t rename();

    static void addHandler(AsyncWebServer *server);

 private:

    bool _isValidData();

    void _sendResponse(uint16_t httpStatusCode = 0);

    // String &require(const String name);
    bool _requireAuthentication();
    String _requireFile(const String &name, bool mustExist);
    File _requireFile(const String &name);
    String _requireDir(const String &name);
    ListDir _getDir(const String &path);
    String _requireArgument(const String &name);
    String _getArgument(const String &name);

    void normalizeFilename(String &filename);

private:
    bool _isAuthenticated;
    uint8_t _errors;
    String _uri;
    AsyncWebServerRequest *_request;
    AsyncWebServerResponse *_response;
    HttpHeaders _headers;

    WebServerSetCPUSpeedHelper _setCPUSpeed;
};

class FileManagerWebHandler : public AsyncWebHandler {
public:
    FileManagerWebHandler(const __FlashStringHelper *uri) : AsyncWebHandler() {
        _uri = uri;
    }

    virtual bool canHandle(AsyncWebServerRequest *request) override;
    virtual void handleRequest(AsyncWebServerRequest *request) override;

    static void onRequestHandler(AsyncWebServerRequest *request);

private:
    const __FlashStringHelper *_uri;
};

#endif
