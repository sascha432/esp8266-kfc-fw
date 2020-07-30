
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if WEBSERVER_SUPPORT

#ifndef DEBUG_WEB_SERVER
#define DEBUG_WEB_SERVER 0
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <HttpHeaders.h>
#include <SpeedBooster.h>
#include <KFCJson.h>
#include <HeapStream.h>
#include "failure_counter.h"
#include "plugins.h"

class FileMapping;

class WebServerSetCPUSpeedHelper : public SpeedBooster {
public:
    WebServerSetCPUSpeedHelper();
};

class WebTemplate;

class WebServerPlugin : public PluginComponent {
public:
    WebServerPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;

private:
    void begin();
    void end();
    bool _isPublic(const String &pathString) const;
    bool _clientAcceptsGzip(AsyncWebServerRequest *request) const;

    bool _handleFileRead(String path, bool client_accepts_gzip, AsyncWebServerRequest *request);
    bool _sendFile(const FileMapping &mapping, HttpHeaders &httpHeaders, bool client_accepts_gzip, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);

public:
    typedef std::function<void(size_t position, size_t size)> UpdateFirmwareCallback_t;

    enum class AuthType {
        UNKNOWN =           -1,
        NONE,
        SID,
        SID_COOKIE,
        PASSWORD,
        BEARER,
    };

    class RestRequest;

    class RestHandler {
    public:
        typedef std::function<AsyncWebServerResponse *(AsyncWebServerRequest *request, RestRequest &rest)> Callback;
        typedef std::vector<RestHandler> Vector;

        RestHandler(const __FlashStringHelper *url, Callback callback);
        const __FlashStringHelper *getURL() const;
        AsyncWebServerResponse *invokeCallback(AsyncWebServerRequest *request, RestRequest &rest);

    private:
        const __FlashStringHelper *_url;
        Callback _callback;
    };

    class RestRequest {
    public:
        RestRequest(AsyncWebServerRequest *request, const WebServerPlugin::RestHandler &handler, AuthType auth);

        AuthType getAuth() const;

        RestHandler &getHandler();
        JsonMapReader &getJsonReader();

        AsyncWebServerResponse *createResponse(AsyncWebServerRequest *request);
        void writeBody(uint8_t *data, size_t len);

        bool isUriMatch(const __FlashStringHelper *uri) const;

    private:
        AsyncWebServerRequest *_request;
        RestHandler _handler;
        AuthType _auth;
        HeapStream _stream;
        JsonMapReader _reader;
        bool _readerError;
    };

    class AsyncRestWebHandler : public AsyncWebHandler {
    public:
        AsyncRestWebHandler();
        virtual bool canHandle(AsyncWebServerRequest *request) override;
        virtual void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;
        virtual void handleRequest(AsyncWebServerRequest *request) override;
    };

public:
    static WebServerPlugin &getInstance();
    static AsyncWebServer *getWebServerObject();

    static bool addHandler(AsyncWebHandler* handler);
    static AsyncCallbackWebHandler *addHandler(const String &uri, ArRequestHandlerFunction onRequest);
    static AsyncRestWebHandler *addRestHandler(RestHandler &&handler);

    void setUpdateFirmwareCallback(UpdateFirmwareCallback_t callback);

private:
    UpdateFirmwareCallback_t _updateFirmwareCallback;
    RestHandler::Vector _restCallbacks;
    AsyncWebServer *_server;
public:
    FailureCounterContainer _loginFailures;

// AsyncWebServer handlers
public:
    static void handlerNotFound(AsyncWebServerRequest *request);
    static void handlerScanWiFi(AsyncWebServerRequest *request);
    static void handlerLogout(AsyncWebServerRequest *request);
    static void handlerAlive(AsyncWebServerRequest *request);
    static void handlerSyncTime(AsyncWebServerRequest *request);
    static void handlerAlerts(AsyncWebServerRequest *request);
    static void handlerWebUI(AsyncWebServerRequest *request);
    static void handlerSpeedTest(AsyncWebServerRequest *request, bool zip);
    static void handlerSpeedTestZip(AsyncWebServerRequest *request);
    static void handlerSpeedTestImage(AsyncWebServerRequest *request);
    static void handlerImportSettings(AsyncWebServerRequest *request);
    static void handlerExportSettings(AsyncWebServerRequest *request);
    static void handlerUpdate(AsyncWebServerRequest *request);
    static void handlerUploadUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

public:
    static const __FlashStringHelper *getAuthTypeStr(AuthType type) {
        switch(type) {
            case AuthType::SID:
                return F("Session");
            case AuthType::SID_COOKIE:
                return F("Session Cookie");
            case AuthType::BEARER:
                return F("Bearer Toklen");
            case AuthType::PASSWORD:
                return F("Password");
            default:
                return F("Not Authorized");
        }
    }

    bool isRunning() const;
    AuthType isAuthenticated(AsyncWebServerRequest *request) const;
};

inline bool operator ==(const WebServerPlugin::AuthType &auth, bool invert) {
    auto result = static_cast<int>(auth) >= static_cast<int>(WebServerPlugin::AuthType::SID);
    return invert ? result : !result;
}

#endif
