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
    onRequest(_onRequest);
    onUpload([this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        this->_handleUpload(request, filename, index, data, len, final);
    });
}

void AsyncFileUploadWebHandler::_handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (WebServer::Plugin::getInstance().isAuthenticated(request) == false) {
        if (index == 0) {
            request->send(403);
        }
        request->client()->close();
        return;
    }

    if (index == 0) {
        request->_tempFile = tmpfile(sys_get_temp_dir(), F("_fm_tmp_upload"));
        request->onDisconnect([this, request]() {
            _cleanUp(request);
        });
        if (!request->_tempFile) {
            _cleanUp(request);
            request->send(503);
            request->client()->close();
            return;
        }
    }

    if (!request->_tempFile || request->_tempFile.write(data, len) != len) {
        _cleanUp(request);
        request->send(507);
        request->client()->close();
        return;
    }
    else if (final) {
        __DBG_printf("filename=%s index=%d len=%d final=%d tmp_file=%s size=%u", filename.c_str(), index, len, final, __S(request->_tempFile.fullName()), request->_tempFile.size());
    }
}

void AsyncFileUploadWebHandler::_cleanUp(AsyncWebServerRequest *request)
{
    if (request->_tempFile && request->_tempFile.fullName()) {
        String filename = request->_tempFile.fullName();
        __DBG_printf("removing temporary file %s", filename.c_str());
        request->_tempFile.close();
        KFCFS.remove(filename);
    }
    else {
        __DBG_printf("temporary file closed");
    }
}
