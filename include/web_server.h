
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if WEBSERVER_SUPPORT

#ifndef DEBUG_WEB_SERVER
#define DEBUG_WEB_SERVER                    0
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <HttpHeaders.h>
#include <SpeedBooster.h>
#include <KFCJson.h>
#include <HeapStream.h>
#include "plugins.h"
#include "web_socket.h"

class FileMapping;
class FailureCounterContainer;

#if IOT_WEATHER_STATION
extern void __weatherStationDetachCanvas(bool release);
extern void __weatherStationAttachCanvas();
#endif

class WebServerSetCPUSpeedHelper : public SpeedBooster {
public:
    WebServerSetCPUSpeedHelper();
};

class WebTemplate;

namespace WebServer {

    class AsyncWebServerEx;
    class HandlerStorage;
    class RestHandler;
    class RestRequest;
    struct HandlerStorage;

    enum class AuthType {
        UNKNOWN = -1,
        NONE,
        ANY,
        SID = ANY,
        SID_COOKIE,
        PASSWORD,
        BEARER,
    };

    using UpdateFirmwareCallback = std::function<void(size_t position, size_t size)>;
    using FailureCounterContainerPtr = std::unique_ptr<FailureCounterContainer>;
    using RestHandlerVector = std::vector<RestHandler>;
    using RestHandlerCallback = std::function<AsyncWebServerResponse *(AsyncWebServerRequest *request, RestRequest &rest)>;
    using AsyncWebServerExPtr = std::unique_ptr<AsyncWebServerEx>;
    using HandlerStoragePtr = std::unique_ptr<HandlerStorage>;

    struct HandlerStorage {
        RestHandlerVector _restCallbacks;
        // AsyncWebServer::RewritesList *rewritesList;
        std::vector<std::unique_ptr<AsyncWebHandler>> _handlers;

        void moveHandlers(LinkedList<AsyncWebHandler*> &handlers) {
            for(auto handler: handlers) {
                _handlers.emplace_back(handler);
            }
            handlers.setOnRemove(nullptr); // removing the callback will destroy the list but keep the handler objects
        }
    };

    struct UploadStatus
    {
        WebServerSetCPUSpeedHelper setCPUSpeed;
        AsyncWebServerResponse *response;
        bool error;
        uint8_t command;
        size_t size;

        UploadStatus() : response(nullptr), error(0), command(0), size(0) {}
    };

    class RestHandler {
    public:
        using Callback = RestHandlerCallback;
        using Vector = RestHandlerVector;

        // the callback is responsible for adding no cache no store headers
        RestHandler(const __FlashStringHelper *url, Callback callback);
        const __FlashStringHelper *getURL() const;
        AsyncWebServerResponse *invokeCallback(AsyncWebServerRequest *request, RestRequest &rest);

    private:
        const __FlashStringHelper *_url;
        Callback _callback;
    };

    class RestRequest {
    public:
        RestRequest(AsyncWebServerRequest *request, const RestHandler &handler, AuthType auth);

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
        using AsyncWebHandler::AsyncWebHandler;

        virtual bool canHandle(AsyncWebServerRequest *request) override;
        virtual void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;
        virtual void handleRequest(AsyncWebServerRequest *request) override;
        virtual bool isRequestHandlerTrivial() override {
            return false; // parse the incoming POST request
        }
    };

    class AsyncUpdateWebHandler : public AsyncWebHandler {
    public:
        using AsyncWebHandler::AsyncWebHandler;

        virtual bool canHandle(AsyncWebServerRequest *request) override;
//        virtual void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;
        virtual void handleRequest(AsyncWebServerRequest *request) override;
        virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override;
        virtual bool isRequestHandlerTrivial() override {
            return true; // do not parse any POST data since it is a huge file upload
        }
    };

    class Plugin : public PluginComponent {
    public:
        Plugin();

        virtual void setup(SetupModeType mode) override;
        virtual void reconfigure(const String &source) override;
        virtual void shutdown() override;
        virtual void getStatus(Print &output) override;
        virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

    public:
        static Plugin &getInstance();
        // check return value for nullptr before using
        static AsyncWebServerEx *getWebServerObject();

        static bool addHandler(AsyncWebHandler* handler);
        static AsyncCallbackWebHandler *addHandler(const String &uri, ArRequestHandlerFunction onRequest);
        static void addRestHandler(RestHandler &&handler);

        // AsyncWebServer custom handlers
        static void handlerNotFound(AsyncWebServerRequest *request);
        static void handlerUpdate(AsyncWebServerRequest *request);
        static void handlerUploadUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

        // install a callback to display the progress of the update
        void setUpdateFirmwareCallback(UpdateFirmwareCallback callback);

    public:
        static FailureCounterContainer *getLoginFailureContainer();

    public:
        static const __FlashStringHelper *getAuthTypeStr(AuthType type) {
            switch(type) {
                case AuthType::SID:
                    return F("Session");
                case AuthType::SID_COOKIE:
                    return F("Session Cookie");
                case AuthType::BEARER:
                    return F("Bearer Token");
                case AuthType::PASSWORD:
                    return F("Password");
                default:
                    break;
            }
            return F("Not Authorized");
        }

        bool isRunning() const;
        AuthType getAuthenticated(AsyncWebServerRequest *request) const;
        bool isAuthenticated(AsyncWebServerRequest *request) const;

    private:
        friend AsyncRestWebHandler;
        friend AsyncUpdateWebHandler;

        FailureCounterContainerPtr _loginFailures;
        UpdateFirmwareCallback _updateFirmwareCallback;
        AsyncWebServerExPtr _server;

    private:
        void begin(bool restart = false);
        void end();
        bool _isPublic(const String &pathString) const;
        bool _clientAcceptsGzip(AsyncWebServerRequest *request) const;

        bool _handleFileRead(String path, bool client_accepts_gzip, AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        bool _sendFile(const FileMapping &mapping, const String &formName, HttpHeaders &httpHeaders, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);

        void _handlerSpeedTest(AsyncWebServerRequest *request, bool zip, HttpHeaders &headers);
        void _handlerWebUI(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _handlerImportSettings(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _handlerExportSettings(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _handlerAlerts(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _addRestHandler(RestHandler &&handler);

    };

    inline FailureCounterContainer *Plugin::getLoginFailureContainer()
    {
        return getInstance()._loginFailures.get();
    }

    inline void Plugin::addRestHandler(RestHandler &&handler)
    {
        getInstance()._addRestHandler(std::move(handler));
    }

    inline bool Plugin::isRunning() const
    {
        return (_server != nullptr);
    }

    inline bool Plugin::isAuthenticated(AsyncWebServerRequest *request) const
    {
        return getAuthenticated(request) > AuthType::NONE;
    }


    class AsyncWebServerEx : public AsyncWebServer {
    public:
        using AsyncWebServer::AsyncWebServer;

    private:
        friend Plugin;
        friend AsyncRestWebHandler;

        RestHandlerVector _restCallbacks;
    };

}

// allows to compare AuthType against boolean
// AuthType == false: not authorized
// AuthType == true: authorized (ANY method)
inline bool operator ==(const WebServer::AuthType &auth, bool value) {
    return (static_cast<int>(auth) >= static_cast<int>(WebServer::AuthType::ANY)) == value;
}

#endif
