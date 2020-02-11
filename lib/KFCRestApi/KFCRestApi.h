
/**
  Author: sascha_lammers@gmx.de
*/

#if HAVE_KFC_RESTAPI

#pragma once

#include <Arduino_compat.h>
#include <KFCJson.h>
#include <HttpHeaders.h>
#include <HeapStream.h>

#ifndef DEBUG_KFC_REST_API
#define DEBUG_KFC_REST_API                                  0
#endif

#ifndef KFC_REST_API_USE_HTTP_CLIENT
#define KFC_REST_API_USE_HTTP_CLIENT				        1
#endif

#if DEBUG_KFC_REST_API
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class asyncHTTPrequest;

class KFCRestAPI {
public:
    class HttpRequest {
    public:
        typedef std::function<void(int16_t status, HttpRequest &request)> Callback_t;

        HttpRequest(KFCRestAPI &api, JsonBaseReader *json, Callback_t callback);
        ~HttpRequest();

        void setStream(Stream *stream) {
            _json->setStream(stream);
        }

        bool parseStream() {
            return _json->parseStream();
        }

        asyncHTTPrequest &getRequest() {
            return *_request;
        }

        const String &getMessage() const;
        int16_t getCode() const;
        void setMessage(const String &message);
        void finish(int16_t code);

        void setUri(const String &uri);

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

        JsonBaseReader *getJsonReader() {
            return _json;
        }

        JsonVariableReader::ElementGroup::Vector *getElementsGroup() {
            return reinterpret_cast<JsonVariableReader::Reader *>(_json)->getElementGroups();
        }

    private:
        asyncHTTPrequest *_request;
        JsonBaseReader *_json;
        Callback_t _callback;
        String _message;
        int16_t _code;
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
    void _createRestApiCall(const String &endPointUri, const String &body, JsonBaseReader *json, HttpRequest::Callback_t callback);

protected:
    HttpRequestVector _requests;
    HttpHeaders _headers;
};

#include <debug_helper_disable.h>

#endif

