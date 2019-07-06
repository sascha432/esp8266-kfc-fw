/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include "debug_helper.h"

// handle authentication and upload file into temporary directory until it has been processed or can be discarded

class AsyncFileUploadWebHandler : public AsyncCallbackWebHandler {
public:
    AsyncFileUploadWebHandler(const String &uri, ArRequestHandlerFunction _onRequest = nullptr) : AsyncCallbackWebHandler() {
        setUri(uri);
        setMethod(HTTP_POST);
        onRequest(_onRequest);
        onUpload([this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            this->_handleUpload(request, filename, index, data, len, final);
        });
    }

    static void markTemporaryFileAsProcessed(AsyncWebServerRequest *request);

private:
    void _cleanUp(AsyncWebServerRequest *request);
    void _handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
};
