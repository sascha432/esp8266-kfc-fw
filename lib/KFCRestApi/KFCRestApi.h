
/**
  Author: sascha_lammers@gmx.de
*/

#if HAVE_KFC_RESTAPI

#pragma once

#include <Arduino_compat.h>
#include <JsonCallbackReader.h>
#include <HttpHeaders.h>
#include <HeapStream.h>
#include "RestApiJsonReader.h"

#ifndef DEBUG_KFC_REST_API
#define DEBUG_KFC_REST_API                                  1
#endif

#if _WIN32 || _WIN64
#define KFC_REST_APITIMEZONE_USE_HTTP_CLIENT			    1
#elif defined(ESP32)
#define KFC_REST_API_USE_HTTP_CLIENT				        1
#else
#define KFC_REST_API_USE_HTTP_CLIENT				        0
#include <asyncHTTPrequest.h>
#endif


class KFCRestAPI {
public:
    class HttpRequest {
    public:
        typedef std::function<void(int16_t status, HttpRequest &request)> Callback_t;

        HttpRequest(KFCRestAPI &api, Callback_t callback);
        ~HttpRequest();

        void setStream(Stream *stream) {
            _json.setStream(stream);
        }

        bool parseStream() {
            return _json.parseStream();
        }

        asyncHTTPrequest &getRequest() {
            return *_request;
        }

        String &getMessage();
        void setMessage(const String &message);
        void finish(int16_t code);

        void setUri(const __FlashStringHelper *uri);

        String &getUrlString() {
            return _url;
        }

        const char *getUrl() const {
            return _url.c_str();
        }

        String &getBody() {
            return _body;
        }

        KFCRestAPI &getApi() {
            return _api;
        }

    private:
        asyncHTTPrequest *_request;
        RestApiJsonReader _json;
        Callback_t _callback;
        String _message;
        String _url;
        String _body;
        KFCRestAPI &_api;
    };

public:
    virtual void getRestUrl(String &url) const = 0;
    virtual void getBearerToken(String &token) const = 0;

public:
    KFCRestAPI() : _headers(false) {
    }

    typedef std::vector<HttpRequest *> HttpRequestVector;

    static void _onData(void *ptr, asyncHTTPrequest *request, size_t available);
    static void _onReadyStateChange(void *ptr, asyncHTTPrequest *request, int readyState);
    static void _removeHttpRequest(HttpRequest *httpRequestPtr);

protected:
    void _createRestApiCall(const __FlashStringHelper *endPointUri, HttpRequest::Callback_t callback);

protected:
    HttpRequestVector _requests;
    HttpHeaders _headers;
};

#endif
