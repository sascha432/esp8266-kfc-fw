/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "web_server.h"

using namespace WebServer;

// ------------------------------------------------------------------------
// RestRequest
// ------------------------------------------------------------------------

RestRequest::RestRequest(AsyncWebServerRequest *request, const RestHandler &handler, AuthType auth) :
    _request(request),
    _handler(handler),
    _auth(auth),
    _stream(),
    _reader(_stream),
    _readerError(false)
{
    _reader.initParser();
}

AuthType RestRequest::getAuth() const
{
    return _auth;
}

AsyncWebServerResponse *RestRequest::createResponse(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = nullptr;
    // respond with a json message and status 200
    if (!(request->method() & WebRequestMethod::HTTP_POST)) {
        response = request->beginResponse(200, FSPGM(mime_application_json), PrintString(F("{\"status\":%u,\"message\":\"%s\"}"), 405, AsyncWebServerResponse::responseCodeToString(405)));
    }
    else if (_auth == false) {
        response = request->beginResponse(200, FSPGM(mime_application_json), PrintString(F("{\"status\":%u,\"message\":\"%s\"}"), 401, AsyncWebServerResponse::responseCodeToString(401)));
    }
    else if (_readerError) {
        response = request->beginResponse(200, FSPGM(mime_application_json), PrintString(F("{\"status\":%u,\"message\":\"%s\"}"), 400, AsyncWebServerResponse::responseCodeToString(400)));
    }
    else {
        return getHandler().invokeCallback(request, *this);
    }
    // add no cache and no store to any 200 response
    HttpHeaders headers;
    headers.addNoCache(true);
    headers.setAsyncWebServerResponseHeaders(response);
    return response;
}

RestHandler &RestRequest::getHandler()
{
    return _handler;
}

JsonMapReader &RestRequest::getJsonReader()
{
    return _reader;
}

void RestRequest::writeBody(uint8_t *data, size_t len)
{
    if (!_readerError) {
        _stream.setData(data, len);
        if (!_reader.parseStream()) {
            __LDBG_printf("json parser: %s", _reader.getLastErrorMessage().c_str());
            _readerError = true;
        }
    }
}

bool RestRequest::isUriMatch(const __FlashStringHelper *uri) const
{
    if (!uri) {
        return false;
    }
    String baseUrl = _handler.getURL();
    append_slash(baseUrl);
    baseUrl += uri;

    return _request->url().equals(baseUrl);
}


// ------------------------------------------------------------------------
// AsyncRestWebHandler
// ------------------------------------------------------------------------

bool AsyncRestWebHandler::canHandle(AsyncWebServerRequest *request)
{
    __LDBG_printf("url=%s auth=%d", request->url().c_str(), Plugin::getInstance().isAuthenticated(request));

    auto &url = request->url();
    for(const auto &handler: Plugin::getInstance()._server->_restCallbacks) {
        if (url == FPSTR(handler.getURL())) {
            request->addInterestingHeader(FSPGM(Authorization));

            // emulate AsyncWebServerRequest dtor using onDisconnect callback
            request->_tempObject = new RestRequest(request, handler, Plugin::getInstance().getAuthenticated(request));
            request->onDisconnect([this, request]() {
                if (request->_tempObject) {
                    delete reinterpret_cast<RestRequest *>(request->_tempObject);
                    request->_tempObject = nullptr;
                }
            });
            return true;
        }
    }
    return false;
}

void AsyncRestWebHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    __LDBG_printf("url=%s data='%*.*s' idx=%u len=%u total=%u temp=%p", request->url().c_str(), len, len, data, index, len, total, request->_tempObject);
    if (request->_tempObject) {
        auto &rest = *reinterpret_cast<RestRequest *>(request->_tempObject);
        // wondering why enum class == true compiles? -> global custom operator
        if (rest.getAuth() == true) {
            rest.writeBody(data, len);
        }
    }
    else {
        request->send(500);
    }
}

void AsyncRestWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    __LDBG_printf("url=%s json temp=%p is_post_metod=%u", request->url().c_str(), request->_tempObject, (request->method() & WebRequestMethod::HTTP_POST));
    if (request->_tempObject) {
        auto &rest = *reinterpret_cast<RestRequest *>(request->_tempObject);
#if DEBUG_WEB_SERVER
        rest.getJsonReader().dump(DEBUG_OUTPUT);
#endif
        request->send(rest.createResponse(request));
    }
    else {
        request->send(500);
    }
}


// ------------------------------------------------------------------------
// RestRequest
// ------------------------------------------------------------------------

RestHandler::RestHandler(const __FlashStringHelper *url, Callback callback) : _url(url), _callback(callback)
{

}

const __FlashStringHelper *RestHandler::getURL() const
{
    return _url;
}

AsyncWebServerResponse *RestHandler::invokeCallback(AsyncWebServerRequest *request, RestRequest &rest)
{
    return _callback(request, rest);
}
