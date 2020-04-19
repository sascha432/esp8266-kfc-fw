
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
#include "failure_counter.h"
#include "progmem_data.h"
#include "plugins.h"

class FileMapping;

class WebServerSetCPUSpeedHelper : public SpeedBooster {
public:
    WebServerSetCPUSpeedHelper();
};

class WebTemplate;

String network_scan_html(int8_t num_networks);

class WebServerPlugin : public PluginComponent {
public:
    WebServerPlugin() : _updateFirmwareCallback(nullptr), _server(nullptr) {
        REGISTER_PLUGIN(this);
    }
    virtual PGM_P getName() const {
        return SPGM(http);
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Web server");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return PRIO_HTTP;
    }
    virtual bool allowSafeMode() const override {
        return true;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override {
        return false;
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return NONE;
    }
    virtual PGM_P getConfigureForm() const override {
        return PSTR("remote");
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

private:
    void begin();
    void end();
    bool _isPublic(const String &pathString) const;
    bool _clientAcceptsGzip(AsyncWebServerRequest *request) const;

    bool _handleFileRead(String path, bool client_accepts_gzip, AsyncWebServerRequest *request);
    bool _sendFile(const FileMapping &mapping, HttpHeaders &httpHeaders, bool client_accepts_gzip, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);

public:
    typedef std::function<void(size_t position, size_t size)> UpdateFirmwareCallback_t;

    typedef struct {
        int httpCode;
        AsyncWebServerResponse *response;
    } RestResponse_t;

    typedef std::function<RestResponse_t(AbstractJsonValue &json, AbstractJsonValue::JsonVariantEnum_t type)> RestCallback_t;
    typedef std::vector<RestCallback_t> RestCallbackVector;

public:
    static WebServerPlugin &getInstance();
    static AsyncWebServer *getWebServerObject();

    static bool addHandler(AsyncWebHandler* handler);
    static bool addHandler(const String &uri, ArRequestHandlerFunction onRequest);

    static bool addRestHandler(const String &uri, RestCallback_t callback);

    void setUpdateFirmwareCallback(UpdateFirmwareCallback_t callback);

private:
    UpdateFirmwareCallback_t _updateFirmwareCallback;
    RestCallbackVector _restCallbacks;
    AsyncWebServer *_server;
    FailureCounterContainer _loginFailures;

// AsyncWebServer handlers
public:
    static void handlerNotFound(AsyncWebServerRequest *request);
    static void handlerScanWiFi(AsyncWebServerRequest *request);
    static void handlerLogout(AsyncWebServerRequest *request);
    static void handlerAlive(AsyncWebServerRequest *request);
    static void handlerSyncTime(AsyncWebServerRequest *request);
    static void handlerWebUI(AsyncWebServerRequest *request);
    static void handlerSpeedTest(AsyncWebServerRequest *request, bool zip);
    static void handlerSpeedTestZip(AsyncWebServerRequest *request);
    static void handlerSpeedTestImage(AsyncWebServerRequest *request);
    static void handlerImportSettings(AsyncWebServerRequest *request);
    static void handlerExportSettings(AsyncWebServerRequest *request);
    static void handlerUpdate(AsyncWebServerRequest *request);
    static void handlerUploadUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

public:
    bool isRunning() const;
    bool isAuthenticated(AsyncWebServerRequest *request) const;
};

inline bool web_server_is_authenticated(AsyncWebServerRequest *request) {
    return WebServerPlugin::getInstance().isAuthenticated(request);
}

inline void web_server_add_handler(AsyncWebHandler* handler) {
    WebServerPlugin::addHandler(handler);
}

inline void web_server_add_handler(const String &uri, ArRequestHandlerFunction onRequest) {
    WebServerPlugin::addHandler(uri, onRequest);
}

AsyncWebServer *get_web_server_object();

#endif
