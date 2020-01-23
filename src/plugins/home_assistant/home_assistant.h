/**
  Author: sascha_lammers@gmx.de
*/

#if HOME_ASSISTANT_INTEGRATION

#pragma once

#include <Arduino_compat.h>
#include <JsonCallbackReader.h>
#include "RestApiJsonReader.h"
#include "plugins.h"

#ifndef DEBUG_HOME_ASSISTANT
#define DEBUG_HOME_ASSISTANT                            1
#endif

class asyncHTTPrequest;

class HassPlugin : public PluginComponent {
public:
    class HttpRequest {
    public:
        typedef std::function<void(uint16_t status, HttpRequest *request)> Callback;

        HttpRequest(Callback callback) : _callback(callback) {
            _json.initParser();
            _request = new asyncHTTPrequest();
        }
        ~HttpRequest() {
            delete _request;
        }

        void setStream(Stream *stream) {
            _json.setStream(stream);
        }

        bool parseStream() {
            return _json.parseStream();
        }

        asyncHTTPrequest &getRequest() {
            return *_request;
        }

        void finish(uint16_t status) {
            _callback(status, this);
        }

    private:
        asyncHTTPrequest *_request;
        RestApiJsonReader _json;
        Callback _callback;
    };

public:
    HassPlugin() {
        REGISTER_PLUGIN(this, "HassPlugin");
    }

    virtual PGM_P getName() const {
        return PSTR("hass");
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

public:
    typedef std::vector<HttpRequest *> HttpRequestVector;

    static void _onData(void *ptr, asyncHTTPrequest *request, size_t available);
    static void _onReadyStateChange(void *, asyncHTTPrequest *request, int readyState);
    static void _removeHttpRequest(HttpRequest *httpRequestPtr);

private:
    void _createRestApiCall(HttpRequest::Callback callback);

private:
    HttpRequestVector _requests;
};

#endif
