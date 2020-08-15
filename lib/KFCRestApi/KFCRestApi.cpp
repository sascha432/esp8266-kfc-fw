/**
  Author: sascha_lammers@gmx.de
*/

#include <EventScheduler.h>
#include <PrintString.h>
#include <LoopFunctions.h>
#include <HeapStream.h>
#include "KFCRestApi.h"

#if DEBUG_KFC_REST_API
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if !KFC_REST_API_USE_HTTP_CLIENT
#if ESP32
#include "asyncHTTPrequest_ESP32.h"
#else
#include "asyncHTTPrequest.h"
#endif
#endif

KFCRestAPI::HttpRequest::HttpRequest(KFCRestAPI &api, JsonBaseReader *json, Callback_t callback) : _json(json), _callback(callback), _message(F("None")), _code(0), _api(api)
{
    _json->initParser();
    _request = __DBG_new(HttpClient);

    String token;
    _api.getBearerToken(token);
    if (token.length()) {
        _api._headers.add(new HttpAuthorization(HttpAuthorization::BEARER, token));
    }
    _api._headers.add(new HttpContentType(FSPGM(mime_application_json)));
}

KFCRestAPI::HttpRequest::~HttpRequest()
{
    _debug_printf_P(PSTR("httpRequestPtr=%p\n"), this);
    __DBG_delete(_request);
    __DBG_delete(_json);
}

const String &KFCRestAPI::HttpRequest::getMessage() const
{
    return KFCRestAPI::HttpRequest::_message;
}

int16_t KFCRestAPI::HttpRequest::getCode() const {
    return _code;
}

void KFCRestAPI::HttpRequest::setMessage(const String &message)
{
    _debug_printf_P(PSTR("msg=%s\n"), message.c_str());
    _message = message;
}

void KFCRestAPI::HttpRequest::finish(int16_t code)
{
    _debug_printf_P(PSTR("code=%d, msg=%s\n"), code, _message.c_str());
    _code = code;
    _callback(code, *this);
}

void KFCRestAPI::HttpRequest::setUri(const String &uri)
{
    _api.getRestUrl(_url);
    _url += uri;
    _debug_printf_P(PSTR("url=%s\n"), _url.c_str());
}

void KFCRestAPI::_onData(void *ptr, HttpClient *request, size_t available)
{
    _debug_printf_P(PSTR("available=%u, httpRequestPtr=%p\n"), available, ptr);
    auto &httpRequest = *reinterpret_cast<HttpRequest *>(ptr);
    uint8_t buffer[64];
    size_t len;
    HeapStream stream(buffer);
    httpRequest.setStream(&stream);
    while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
        _debug_printf_P(PSTR("response(%u): %*.*s\n"), len, len, len, buffer);
        stream.setLength(len);
        if (!httpRequest.parseStream()) {
            request->abort();
            request->onData(nullptr);
            break;
        }
    }
}

void KFCRestAPI::_onReadyStateChange(void *ptr, HttpClient *request, int readyState)
{
    _debug_printf_P(PSTR("readyState=%d, httpRequestPtr=%p\n"), readyState, ptr);
    auto httpRequestPtr = reinterpret_cast<HttpRequest *>(ptr);
    auto &httpRequest = *httpRequestPtr;

    if (readyState == 4) {
        int httpCode;

        request->onData(nullptr); // disable callback, if any data is left, onPoll() will call it again
        if ((httpCode = request->responseHTTPcode()) == 200) {
            // read rest of the data that was not processed by onData()
            if (request->available()) {
                _onData(ptr, request, request->available());
            }
            httpRequest.finish(200);

        } else {

            String message;
            // int code = 0;

            // read response
            uint8_t buffer[64];
            HeapStream stream(buffer);
            JsonCallbackReader reader(stream, [&message](const String& key, const String& value, size_t partialLength, JsonBaseReader& json) {
                if (json.getLevel() == 1 && key.equals(F("message"))) {
                    message = value;
                }
                return true;
            });
            reader.parseStream();

            size_t len;
            while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
                _debug_printf_P(PSTR("response(%u): %*.*s\n"), len, len, len, buffer);
                stream.setLength(len);
                if (!reader.parseStream()) {
                    break;
                }
            }

            if (message.length()) {
                httpRequest.setMessage(message);
            }
            else {
                httpRequest.setMessage(PrintString(F("HTTP error code %d"), httpCode));
            }

            _debug_printf_P(PSTR("http_code=%d code=%d message=%s\n"), httpCode, readyState, message.c_str());
            httpRequest.finish(httpCode);
        }

        LoopFunctions::callOnce([httpRequestPtr]() {
            KFCRestAPI::_removeHttpRequest(httpRequestPtr);
        });

    }
}

void KFCRestAPI::_removeHttpRequest(KFCRestAPI::HttpRequest *httpRequestPtr)
{
    _debug_printf_P(PSTR("httpRequestPtr=%p\n"), httpRequestPtr);

    auto &api = httpRequestPtr->getApi();
    api._requests.erase(std::remove(api._requests.begin(), api._requests.end(), httpRequestPtr), api._requests.end());
    __DBG_delete(httpRequestPtr);

    if (api._autoDelete && api._requests.empty()) {
        _debug_printf_P(PSTR("auto delete\n"));
        __DBG_delete(&api);
    }
}

void KFCRestAPI::_createRestApiCall(const String &endPointUri, const String &body, JsonBaseReader *json, HttpRequest::Callback_t callback)
{
    _debug_printf_P(PSTR("endpoint=%s payload=%s timeout=%u\n"), endPointUri.c_str(), body.c_str(), _timeout);
    auto httpRequestPtr = __DBG_new(HttpRequest, *this, json, callback);
    _debug_printf_P(PSTR("httpRequestPtr=%p\n"), httpRequestPtr);

    auto &httpRequest = *httpRequestPtr;
    _requests.push_back(httpRequestPtr);
    auto &request = httpRequest.getRequest();

    request.onData(_onData, httpRequestPtr);
    request.onReadyStateChange(_onReadyStateChange, httpRequestPtr);
    request.setTimeout(_timeout);
    httpRequest.setUri(endPointUri);

    if (request.open(body.length() ? "POST" : "GET", httpRequest.getUrl())) {

        _headers.setHeadersCallback([&request](const String &name, const String &header) {
            request.setReqHeader(name.c_str(), header.c_str());
        }, true);

        if (request.send(body.c_str())) {

        } else {
            httpRequest.setMessage(PrintString(F("HTTP client send error (%s)"), httpRequest.getUrl()));
            httpRequest.finish(-101);
        }

    }
    else {
        httpRequest.setMessage(PrintString(F("HTTP client open error (%s)"), httpRequest.getUrl()));
        httpRequest.finish(-100);
    }
}
