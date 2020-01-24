/**
  Author: sascha_lammers@gmx.de
*/

#if HAVE_KFC_RESTAPI

#include <EventScheduler.h>
#include <asyncHTTPrequest.h>
#include <PrintString.h>
#include "KFCRestApi.h"
#include "progmem_data.h"

#if DEBUG_HOME_ASSISTANT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

KFCRestAPI::HttpRequest::HttpRequest(KFCRestAPI &api, Callback_t callback) : _callback(callback), _message(F("None")), _api(api)
{
    _json.initParser();
    _request = new asyncHTTPrequest();

    String token;
    _api.getBearerToken(token);
    if (token.length()) {
        _api._headers.add(new HttpAuthorization(HttpAuthorization::BEARER, token));
    }
    _api._headers.add(new HttpContentType(FSPGM(mime_application_json)));
}

KFCRestAPI::HttpRequest::~HttpRequest()
{
    _debug_println(F("~HttpRequest"));
    delete _request;
}

String &KFCRestAPI::HttpRequest::getMessage() {
    return _message;
}

void KFCRestAPI::HttpRequest::setMessage(const String &message)
{
    _debug_printf_P(PSTR("HttpRequest::setMessage(): msg=%s\n"), message.c_str());
    _message = message;
}

void KFCRestAPI::HttpRequest::finish(int16_t code)
{
    _callback(code, *this);
}

void KFCRestAPI::HttpRequest::setUri(const __FlashStringHelper *uri)
{
    _api.getRestUrl(_url);
    _url += uri;
    _debug_printf_P(PSTR("HttpRequest::setUri(): url=%s\n"), _url.c_str());
}

void KFCRestAPI::_onData(void *ptr, asyncHTTPrequest *request, size_t available)
{
    _debug_printf_P(PSTR("KFCRestAPI::_onData(): available=%u\n"), available);
    auto &httpRequest = *reinterpret_cast<HttpRequest *>(ptr);
    uint8_t buffer[64];
    size_t len;
    HeapStream stream(buffer);
    httpRequest.setStream(&stream);
    while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
        stream.setLength(len);
        if (!httpRequest.parseStream()) {
            request->abort();
            request->onData(nullptr);
            break;
        }
    }
}

void KFCRestAPI::_onReadyStateChange(void *ptr, asyncHTTPrequest *request, int readyState)
{
    _debug_printf_P(PSTR("KFCRestAPI::_onReadyStateChange(): readyState=%d\n"), readyState);
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
            int code = 0;

            // read response
            uint8_t buffer[64];
            HeapStream stream(buffer);
            httpRequest.setStream(&stream);
            httpRequest.parseStream();

            size_t len;
            while((len = request->responseRead(buffer, sizeof(buffer))) > 0) {
                stream.setLength(len);
                if (!httpRequest.parseStream()) {
                    break;
                }
            }

            httpRequest.setMessage(PrintString(F("HTTP error code %d, message %s"), httpCode, ""));

            _debug_printf_P(PSTR("KFCRestAPI::_onReadyStateChange(), http code=%d, code=%d, message=%s\n"), httpCode, code, message.c_str());
            httpRequest.finish(httpCode);
        }

        // delete object with a delay and inside the main loop, otherwise the program crashes in random places
        Scheduler.addTimer(100, false, [httpRequestPtr](EventScheduler::TimerPtr timer) {
            KFCRestAPI::_removeHttpRequest(httpRequestPtr);
        });
    }
}

void KFCRestAPI::_removeHttpRequest(HttpRequest *httpRequestPtr)
{
    auto &api = httpRequestPtr->getApi();
    _debug_printf_P(PSTR("KFCRestAPI::_removeHttpRequest()\n"));
    api._requests.erase(std::find(api._requests.begin(), api._requests.end(), httpRequestPtr));
    delete httpRequestPtr;
}

void KFCRestAPI::_createRestApiCall(const __FlashStringHelper *endPointUri, HttpRequest::Callback_t callback)
{
    _debug_printf_P(PSTR("KFCRestAPI::_createRestApiCall(): endpoint=%s\n"), endPointUri);
    auto httpRequestPtr = new HttpRequest(*this, callback);
    auto &httpRequest = *httpRequestPtr;
    _requests.emplace_back(httpRequestPtr);
    auto &request = httpRequest.getRequest();

    request.onData(_onData, httpRequestPtr);
    request.onReadyStateChange(_onReadyStateChange, httpRequestPtr);
    request.setTimeout(15);
    httpRequest.setUri(endPointUri);

    _headers.setHeadersCallback([&request](const String &name, const String &header) {
        request.setReqHeader(name.c_str(), header.c_str());
    }, true);

    auto &body = httpRequest.getBody();
    if (!request.open(body.length() ? "POST" : "GET", httpRequest.getUrl())) {
        httpRequest.setMessage(PrintString(F("HTTP client open error (%s)"), httpRequest.getUrl()));
        httpRequest.finish(-100);
    }
    else if (!request.send(body.c_str())) {
        httpRequest.setMessage(PrintString(F("HTTP client send error (%s)"), httpRequest.getUrl()));
        httpRequest.finish(-101);
    }
}

#endif
