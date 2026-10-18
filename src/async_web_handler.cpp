/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <HttpHeaders.h>
#include "async_web_handler.h"
#include "web_server.h"
#include "misc.h"
#include "../src/plugins/plugins.h"

 AsyncFileUploadWebHandler::AsyncFileUploadWebHandler(const String &uri, ArRequestHandlerFunction _onRequest) :
    AsyncCallbackWebHandler()
 {
    setUri(uri);
    setMethod(HTTP_POST);
    if (_onRequest) {
        // a failed upload is answered here, after the body was received: closing the connection from
        // the upload callback deletes the request on the ESP32 (AsyncTCP closes synchronously) while
        // the multipart parser still uses it
        onRequest([_onRequest](AsyncWebServerRequest *request) {
            auto httpCode = _getError(request);
            if (httpCode) {
                WebServer::Plugin::send(httpCode, request);
                return;
            }
            _onRequest(request);
        });
    }
    onUpload([this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        this->_handleUpload(request, filename, index, data, len, final);
    });
}

void AsyncFileUploadWebHandler::_setError(AsyncWebServerRequest *request, uint16_t httpCode)
{
    if (!request->_tempObject) {
        // the request releases _tempObject with free()
        request->_tempObject = malloc(sizeof(uint16_t));
        if (!request->_tempObject) {
            return;
        }
    }
    *reinterpret_cast<uint16_t *>(request->_tempObject) = httpCode;
}

uint16_t AsyncFileUploadWebHandler::_getError(AsyncWebServerRequest *request)
{
    return request->_tempObject ? *reinterpret_cast<uint16_t *>(request->_tempObject) : 0;
}

void AsyncFileUploadWebHandler::_handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (_getError(request)) {
        return; // the upload failed, the rest of the body is ignored
    }

    if (WebServer::Plugin::getInstance().isAuthenticated(request) == false) {
        _setError(request, 403);
        return;
    }

    if (index == 0) {
        request->_tempFile = tmpfile(sys_get_temp_dir(), F("_fm_tmp_upload"));
        request->onDisconnect([this, request]() {
            _cleanUp(request);
        });
        if (!request->_tempFile) {
            _cleanUp(request);
            _setError(request, 503);
            return;
        }
    }

    if (!request->_tempFile || request->_tempFile.write(data, len) != len) {
        _cleanUp(request);
        _setError(request, 507);
        return;
    }
    else if (final) {
        __DBG_printf("filename=%s index=%d len=%d final=%d tmp_file=%s size=%u", filename.c_str(), index, len, final, __S(fullName(request->_tempFile)), request->_tempFile.size());
    }
}

void AsyncFileUploadWebHandler::_cleanUp(AsyncWebServerRequest *request)
{
    if (request->_tempFile && fullName(request->_tempFile)) {
        auto filename = fullName(request->_tempFile);
        __LDBG_printf("removing temporary file %s", filename);
        request->_tempFile.close();
        KFCFS.remove(filename);
    }
    else {
        __LDBG_printf("temporary file closed");
    }
}
