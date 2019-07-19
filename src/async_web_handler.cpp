/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <HttpHeaders.h>
#include "async_web_handler.h"
#include "web_server.h"
#include "misc.h"

PROGMEM_STRING_DECL(text_plain);

 AsyncFileUploadWebHandler::AsyncFileUploadWebHandler(const String &uri, ArRequestHandlerFunction _onRequest) : AsyncCallbackWebHandler() {
    setUri(uri);
    setMethod(HTTP_POST);
    onRequest(_onRequest);
    onUpload([this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        this->_handleUpload(request, filename, index, data, len, final);
    });
}

void AsyncFileUploadWebHandler::markTemporaryFileAsProcessed(AsyncWebServerRequest *request) {
    *(char *)request->_tempObject = 0;
}

void AsyncFileUploadWebHandler::_handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0 && !request->_tempObject) {
        if (!web_server_is_authenticated(request)) {
            request->send(403);
        } else {
            request->_tempFile = tmpfile(sys_get_temp_dir(), F("_fm_tmp_upload"));
            if (request->_tempFile) {
                request->_tempObject = strdup(request->_tempFile.name());
                if (!request->_tempObject) {
                    request->_tempFile.close();
                    SPIFFS.remove(request->_tempFile.name());
                    request->send(500, FSPGM(text_plain), F("Out of memory"));
                } else {
                    request->onDisconnect([this, request]() {
                        this->_cleanUp(request);
                    });
                }
            } else {
                request->send(500);
                request->client()->abort();
            }
        }
    }
    if (request->_tempObject) {
        if (!request->_tempFile) {
            this->_cleanUp(request);
            request->send(503, FSPGM(text_plain), F("Failed to create temporary file"));
        } else if (request->_tempFile.write(data, len) != len) {
            this->_cleanUp(request);
            request->send(507, FSPGM(text_plain), F("Not enough space available"));
        } else if (final) {
            debug_printf_P(PSTR("AsyncFileUploadWebHandler::_handleUpload(%s, index %d, len %d, final %d): temp. file %s size %d\n"), filename.c_str(), index, len, final, request->_tempFile.name(), request->_tempFile.size());
            request->_tempFile.close();
            // todo rename
        }
    }
}

void AsyncFileUploadWebHandler::_cleanUp(AsyncWebServerRequest *request) {
    if (request->_tempObject) {

        request->_tempFile.close();

        if (*(char *)request->_tempObject) {
            debug_printf_P(PSTR("AsyncFileUploadWebHandler::_cleanUp(): Deleting %s\n"), request->_tempObject);
            SPIFFS.remove((char *)request->_tempObject);
        } else {
            debug_println(F("AsyncFileUploadWebHandler::_cleanUp(): Temporary file already removed"));
        }

        free(request->_tempObject);
        request->_tempObject = nullptr;
    }
}
