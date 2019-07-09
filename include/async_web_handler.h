/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>

// handle authentication and upload file into temporary directory until it has been processed or can be discarded

class AsyncFileUploadWebHandler : public AsyncCallbackWebHandler {
public:
    AsyncFileUploadWebHandler(const String &uri, ArRequestHandlerFunction _onRequest = nullptr);

    static void markTemporaryFileAsProcessed(AsyncWebServerRequest *request);

private:
    void _cleanUp(AsyncWebServerRequest *request);
    void _handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
};
