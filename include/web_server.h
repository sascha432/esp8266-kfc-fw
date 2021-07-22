
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if WEBSERVER_SUPPORT

#ifndef DEBUG_WEB_SERVER
#define DEBUG_WEB_SERVER 0
#endif

// enable logging HTTP requests to Serial
#ifndef WEBSERVER_LOG_SERIAL
#define WEBSERVER_LOG_SERIAL 1
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <HttpHeaders.h>
#include <KFCJson.h>
#include <HeapStream.h>
#include "plugins.h"
#include "web_socket.h"
#ifdef ENABLE_ARDUINO_OTA
#include <ArduinoOTA.h>
#endif

class FileMapping;
class FailureCounterContainer;

#if IOT_WEATHER_STATION
extern void __weatherStationDetachCanvas(bool release);
extern void __weatherStationAttachCanvas();
#endif

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

    enum class MessageType : uint8_t {
        DANGER,
        WARNING,
        INFO,
        SUCCESS,
        PRIMARY
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
        AsyncWebServerResponse *response;
        bool error;
        uint8_t command;
        size_t size;
        bool authenticated;
        uint16_t progress;

        UploadStatus() : response(nullptr), error(0), command(0), size(0), authenticated(false), progress(~0) {}
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

        inline static const __FlashStringHelper *getURI() {
            return F("/update");
        }

        virtual bool canHandle(AsyncWebServerRequest *request) override;
        virtual void handleRequest(AsyncWebServerRequest *request) override;
        virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override;
        virtual bool isRequestHandlerTrivial() override {
            return false;
        }

    private:
        UploadStatus *_validateSession(AsyncWebServerRequest *request, int index);
    };

    class Plugin : public PluginComponent {
    public:
        Plugin();

        virtual void setup(SetupModeType mode, const DependenciesPtr &dependencies) override;
        virtual void reconfigure(const String &source) override;
        virtual void shutdown() override;
        virtual void getStatus(Print &output) override;
        virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

    public:
        static Plugin &getInstance();
        // check return value for nullptr before using
        static AsyncWebServerEx *getWebServerObject();

        static bool addHandler(AsyncWebHandler* handler, const __FlashStringHelper *uri);
        static AsyncCallbackWebHandler *addHandler(const String &uri, ArRequestHandlerFunction onRequest);
        static void addRestHandler(RestHandler &&handler);

        // AsyncWebServer custom handlers
        static void handlerNotFound(AsyncWebServerRequest *request);

        // if webTemplate is not nullptr, the object will be destroyed even if an error occurs
        // returns false on error
        static bool sendFileResponse(uint16_t code, const String &path, AsyncWebServerRequest *request, HttpHeaders &headers, WebTemplate *webTemplate = nullptr);

        static void message(AsyncWebServerRequest *request, MessageType type, const String &message, const String &title, HttpHeaders &headers);
        static void message(AsyncWebServerRequest *request, MessageType type, const String &msg, const String &title);

        static void send(uint16_t httpCode, AsyncWebServerRequest *request, const String &message = String());

        // returns true for any html page
        static bool isHtmlContentType(AsyncWebServerRequest *request, HttpHeaders *headers = nullptr);

        // install a callback to display the progress of the update
        void setUpdateFirmwareCallback(UpdateFirmwareCallback callback);

    public:
        static FailureCounterContainer *getLoginFailureContainer();

        static void executeDelayed(AsyncWebServerRequest *request, std::function<void()> callback);

    public:
        static const __FlashStringHelper *getAuthTypeStr(AuthType type);

        bool isRunning() const;
        static AuthType getAuthenticated(AsyncWebServerRequest *request);
        static bool isAuthenticated(AsyncWebServerRequest *request);

        static uint16_t getRunningResponses() {
            return AsyncWebServer::_responseCounter;
        }
        static uint16_t getRunningRequests() {
            return AsyncWebServer::_requestCounter;
        }
        static uint16_t getRunningRequestsAndResponses() {
            return AsyncWebServer::_requestCounter + AsyncWebServer::_responseCounter;
        }
        static uint32_t getRunningRequestsAndResponsesUint32() {
            return (AsyncWebServer::_requestCounter << 16) | AsyncWebServer::_responseCounter;
        }

    private:
        friend AsyncRestWebHandler;
        friend AsyncUpdateWebHandler;

        FailureCounterContainerPtr _loginFailures;
        UpdateFirmwareCallback _updateFirmwareCallback;
        AsyncWebServerExPtr _server;

    private:
        void begin(bool restart);
        void end();
        bool _isPublic(const String &pathString) const;
        bool _clientAcceptsGzip(AsyncWebServerRequest *request) const;

        static void _logRequest(AsyncWebServerRequest *request, AsyncWebServerResponse *response);

#if ENABLE_ARDUINO_OTA

    public:
        void ArduinoOTAbegin();
        void ArduinoOTAend();
        void ArduinoOTADumpInfo(Print &output);
        const __FlashStringHelper *ArduinoOTAErrorStr(ota_error_t err);

        struct ArduinoOTAInfo {

            static constexpr int kNoError = -1;

            ota_error_t _error;
            uint32_t _progress;
            uint32_t _size;
            bool _runnning: 1;
            bool _inProgress: 1;
            bool _rebootPending: 1;

            ArduinoOTAInfo() : _error(static_cast<ota_error_t>(kNoError)), _progress(0), _size(0), _runnning(false), _inProgress(false), _rebootPending(false) {}

            void start() {
                _error = static_cast<ota_error_t>(kNoError);
                _progress = 0;
                _size = 0;
                _inProgress = true;
                _rebootPending = false;
            }

            void stop(int error = kNoError) {
                if (!*this) {
                    // after the first error the status cannot be changed anymore
                    return;
                }
                _error = static_cast<ota_error_t>(error);
                _inProgress = false;
                _rebootPending = false;
                if (error == kNoError) {
                    _rebootPending = true;
                }
            }

            void update(uint32_t progress, uint32_t size) {
                _progress = progress;
                _size = size;
            }

            operator bool() const {
                return _error == static_cast<ota_error_t>(kNoError);
            }
        };
        ArduinoOTAInfo _AOTAInfo;

#endif

        bool _handleFileRead(String path, bool client_accepts_gzip, AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        bool _sendFile(const FileMapping &mapping, const String &formName, HttpHeaders &httpHeaders, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);
        AsyncWebServerResponse *_beginFileResponse(const FileMapping &mapping, const String &formName, HttpHeaders &httpHeaders, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);

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

    inline bool Plugin::isAuthenticated(AsyncWebServerRequest *request)
    {
        return getAuthenticated(request) > AuthType::NONE;
    }

    inline const __FlashStringHelper *Plugin::getAuthTypeStr(AuthType type)
    {
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

    inline void Plugin::message(AsyncWebServerRequest *request, MessageType type, const String &msg, const String &title)
    {
        HttpHeaders headers;
        headers.addNoCache();
        message(request, type, msg, title, headers);
    }

    inline bool Plugin::_sendFile(const FileMapping &mapping, const String &formName, HttpHeaders &headers, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate)
    {
        auto response = _beginFileResponse(mapping, formName, headers, client_accepts_gzip, isAuthenticated, request, webTemplate);
        if (response) {
            _logRequest(request, response);
            request->send(response);
            return true;
        }
        _logRequest(request, response);
        return false;
    }

#if WEBSERVER_LOG_SERIAL == 0
    inline void Plugin::_logRequest(AsyncWebServerRequest *request, AsyncWebServerResponse *response)
    {
    }
#endif


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
