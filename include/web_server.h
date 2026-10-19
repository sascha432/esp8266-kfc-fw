
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <global.h>

#ifndef DEBUG_WEB_SERVER
#    define DEBUG_WEB_SERVER (0 || defined(DEBUG_ALL))
#endif

// enable logging HTTP requests to Serial
#ifndef WEBSERVER_LOG_SERIAL
#    define WEBSERVER_LOG_SERIAL 1
#endif

// adds the free heap and the largest free block to the serial access log (diagnostic)
#ifndef WEBSERVER_LOG_HEAP
#    define WEBSERVER_LOG_HEAP 0
#endif

// free heap and largest free block that have to be left after a template page (a settings form, the status
// page, ...) and its response were created, otherwise the page is not rendered and the request is answered
// with 503 (Retry-After). Measured on
// led_strip_xmas_serial (WEBSERVER_LOG_HEAP + UMM_STATS_FULL, docs/webui_crash_report.md OPEN-3): rendering a
// form page costs 3.1-3.6 KB on top of the form (GET) and up to 4.7 KB with the validation errors of a POST
// (1 KB frame buffer, the TCP send buffer, PrintArgs), and the ESP8266 SDK crashes in the WiFi code when a
// beacon arrives while it cannot allocate (seen with 1.4 KB free and with 5.9 KB free but a 648 byte largest
// block). 8 KB free keeps 3.3+ KB for the SDK, the block limit catches the fragmented heap. The form object
// is the variable part: ~325 byte per field, a 44 field form takes 14 KB and does not fit next to 1024 pixels
// (~20 KB free) - such a form has to be split into pages, the guard only keeps the device alive
#ifndef WEBSERVER_FORM_MIN_FREE_HEAP
#    if ESP8266
#        define WEBSERVER_FORM_MIN_FREE_HEAP 8192
#    else
#        define WEBSERVER_FORM_MIN_FREE_HEAP 32768
#    endif
#endif
#ifndef WEBSERVER_FORM_MIN_FREE_BLOCK
#    if ESP8266
#        define WEBSERVER_FORM_MIN_FREE_BLOCK 4096
#    else
#        define WEBSERVER_FORM_MIN_FREE_BLOCK 16384
#    endif
#endif
// free heap that is required to handle a file request at all. The ESP8266 core aborts on any `new` that fails,
// and with 2-3 KB free (a page rendered next to another one, the SDK's own buffers) the request path itself
// fails (seen: `HttpHeaders::addNoCache()` of a status page next to a refused form POST). A 503 costs ~300 byte
#ifndef WEBSERVER_REQUEST_MIN_FREE_HEAP
#    if ESP8266
#        define WEBSERVER_REQUEST_MIN_FREE_HEAP 4096
#    else
#        define WEBSERVER_REQUEST_MIN_FREE_HEAP 16384
#    endif
#endif
// free heap that is required before a settings form is created. Creating a form costs ~325 byte per field, the
// biggest forms of the firmware (network, 44 field visualizer) take 14 KB, and a `new` that fails while the
// form is built aborts the device (the core has no exceptions, docs/webui_crash_report.md OPEN-3: 3 reboots in
// 12 runs with 13-15 KB free). A form bigger than this limit has to be split into pages
#ifndef WEBSERVER_FORM_CREATE_MIN_FREE_HEAP
#    if ESP8266
#        define WEBSERVER_FORM_CREATE_MIN_FREE_HEAP 16384
#    else
#        define WEBSERVER_FORM_CREATE_MIN_FREE_HEAP 49152
#    endif
#endif

#if !ENABLE_ARDUINO_OTA && !WEBSERVER_KFC_OTA
#    error OTA not enabled
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <HttpHeaders.h>
#include <KFCJson.h>
#include <HeapStream.h>
#include "plugins.h"
#include "web_socket.h"
#ifdef ENABLE_ARDUINO_OTA
#    include <ArduinoOTA.h>
#endif

#if DEBUG_WEB_SERVER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

class FileMapping;

#if WEBSERVER_LOG_HEAP
    // free heap low water mark since the previous call, printed in the access log. ESP8266 with
    // -D UMM_STATS_FULL only (umm_malloc keeps the minimum of the free blocks), 0 otherwise
    uint32_t webServerHeapLowWaterMark();
#endif

#if SECURITY_LOGIN_ATTEMPTS
    class FailureCounterContainer;
#endif

// #if IOT_WEATHER_STATION
// extern void __weatherStationDetachCanvas(bool release);
// extern void __weatherStationAttachCanvas();
// #endif

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
    #if SECURITY_LOGIN_ATTEMPTS
        using FailureCounterContainerPtr = std::unique_ptr<FailureCounterContainer>;
    #endif
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

    enum class ResetOptionsType {
        NONE,
        FSR,
        FFS,
        FSR_FFS
    };

    struct UploadStatus
    {
        AsyncWebServerResponse *response;
        bool error;
        uint8_t command;
        size_t size;
        bool authenticated;
        uint16_t progress;
        ResetOptionsType resetOptions;

        UploadStatus() :
            response(nullptr),
            error(0),
            command(0),
            size(0),
            authenticated(false),
            progress(~0),
            resetOptions(ResetOptionsType::NONE)
        {
        }
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
        KFCJson::JsonMapReader &getJsonReader();

        AsyncWebServerResponse *createResponse(AsyncWebServerRequest *request);
        void writeBody(uint8_t *data, size_t len);

        bool isUriMatch(const __FlashStringHelper *uri) const;

    private:
        AsyncWebServerRequest *_request;
        RestHandler _handler;
        AuthType _auth;
        HeapStream _stream;
        KFCJson::JsonMapReader _reader;
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

    #if WEBSERVER_KFC_OTA

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
            UploadStatus *_validateSession(AsyncWebServerRequest *request, bool sendError);
        };

    #endif

    enum class WebSocketAction {
        NONE = 0,
        DISCARD,
        SAVE,
        APPLY
    };

    // create AsyncWebServerRequest from post data
    class AsyncWebServerRequestParser : public AsyncWebServerRequest
    {
    public:
        AsyncWebServerRequestParser(const String &postData) :
            AsyncWebServerRequest(nullptr, nullptr)
        {
            // create fake post request
            _method = HTTP_POST;
            _version = 1;
            // parse body
            _parseState = PARSE_REQ_BODY;
            _contentLength = postData.length();
            auto ptr = reinterpret_cast<const uint8_t *>(postData.c_str());
            auto size = _contentLength;
            // prevent the parser from ending the state
            _contentLength += 2;
            // feed data
            while(size--) {
                _parsePlainPostChar(*ptr++);
            }
            // terminate last key value pair
            _parsePlainPostChar(uint8_t(0));
            // request done, nothing else to do
            _parseState = PARSE_REQ_END;
        }

        LinkedList<AsyncWebParameter *> &_getParams()
        {
            return _params;
        }
    };

    class Plugin : public PluginComponent {
    public:
        Plugin();

        virtual void setup(SetupModeType mode, const DependenciesPtr &dependencies) override;
        virtual void reconfigure(const String &source) override;
        virtual void shutdown() override;
        virtual void getStatus(Print &output) override;
        virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
        void handleFormData(const String &formName, AsyncWebServerRequest *request, PluginComponent &plugin);

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
        static void send(AsyncWebServerRequest *request, AsyncWebServerResponse *response);

        // returns true for any html page
        static bool isHtmlContentType(AsyncWebServerRequest *request, HttpHeaders *headers = nullptr);

        // install a callback to display the progress of the update
        void setUpdateFirmwareCallback(UpdateFirmwareCallback callback);

    public:
        #if SECURITY_LOGIN_ATTEMPTS
            static FailureCounterContainer *getLoginFailureContainer();
        #endif

        static void executeDelayed(AsyncWebServerRequest *request, std::function<void()> callback);

    public:
        static const __FlashStringHelper *getAuthTypeStr(AuthType type);

        bool isRunning() const;
        static AuthType getAuthenticated(AsyncWebServerRequest *request);
        static bool isAuthenticated(AsyncWebServerRequest *request);

    private:
        friend AsyncRestWebHandler;
        #if WEBSERVER_KFC_OTA
            friend AsyncUpdateWebHandler;
        #endif

        #if SECURITY_LOGIN_ATTEMPTS
            FailureCounterContainerPtr _loginFailures;
        #endif
        UpdateFirmwareCallback _updateFirmwareCallback;
        AsyncWebServerExPtr _server;

    private:
        void begin(bool restart);
        void end();
        void ArduinoOTAdestroy();
        #if MDNS_PLUGIN
            void _addMDNS();
        #endif
        bool _isPublic(const String &pathString) const;
        bool _clientAcceptsGzip(AsyncWebServerRequest *request) const;

    public:
        static void _logRequest(AsyncWebServerRequest *request, AsyncWebServerResponse *response);

#if ENABLE_ARDUINO_OTA

    public:
        void ArduinoOTAbegin();
        void ArduinoOTAend();
        void ArduinoOTADumpInfo(Print &output);
        const __FlashStringHelper *ArduinoOTAErrorStr(ota_error_t err);

        static bool ArduinoOTAEnabled() {
            return getInstance()._AOTAInfo._runnning;
        }

        struct ArduinoOTAInfo {

            static constexpr int kNoError = -1;

            ota_error_t _error;
            uint32_t _progress;
            uint32_t _size;
            bool _runnning;
            bool _inProgress;
            bool _rebootPending;

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

        bool _handleFileRead(const String &path, bool client_accepts_gzip, AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        bool _sendFile(const FileMapping &mapping, const String &formName, HttpHeaders &httpHeaders, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);
        AsyncWebServerResponse *_beginFileResponse(const FileMapping &mapping, const String &formName, HttpHeaders &httpHeaders, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);
        // true if there is enough heap to create a settings form (WEBSERVER_FORM_CREATE_MIN_FREE_HEAP)
        static bool _canCreateForm();
        // true if there is enough heap to handle a file request at all (WEBSERVER_REQUEST_MIN_FREE_HEAP)
        static bool _canHandleRequest();
        // true if the template page that was just created can be rendered: enough heap
        // (WEBSERVER_FORM_MIN_FREE_HEAP, WEBSERVER_FORM_MIN_FREE_BLOCK) and, for a form, its configuration was
        // read (Configuration::hasNvsReadFailed())
        static bool _canRenderTemplate(bool isForm);
        // plain 503 response for a request that cannot be served with the free heap
        static AsyncWebServerResponse *_beginLowMemoryResponse(AsyncWebServerRequest *request);

        #if WEBSERVER_SPEED_TEST
            void _handlerSpeedTest(AsyncWebServerRequest *request, bool zip, HttpHeaders &headers);
        #endif
        void _handlerWebUI(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _handlerImportSettings(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _handlerExportSettings(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _handlerAlerts(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);
        void _addRestHandler(RestHandler &&handler);

    };

    #if SECURITY_LOGIN_ATTEMPTS
        inline FailureCounterContainer *Plugin::getLoginFailureContainer()
        {
            return getInstance()._loginFailures.get();
        }
    #endif

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

    inline void Plugin::send(AsyncWebServerRequest *request, AsyncWebServerResponse *response)
    {
        _logRequest(request, response);
        request->send(response);
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

    extern "C" Plugin webServerPlugin;

    inline Plugin &Plugin::getInstance()
    {
        return webServerPlugin;
    }

    inline AsyncWebServerEx *Plugin::getWebServerObject()
    {
        return webServerPlugin._server.get();
    }

}

// allows to compare AuthType against boolean
// AuthType == false: not authorized
// AuthType == true: authorized (ANY method)
inline bool operator ==(const WebServer::AuthType &auth, bool value) {
    return (static_cast<int>(auth) >= static_cast<int>(WebServer::AuthType::ANY)) == value;
}

#if DEBUG_WEB_SERVER
#    include <debug_helper_disable.h>
#endif
