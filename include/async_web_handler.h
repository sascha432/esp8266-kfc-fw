/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>

// handle authentication and upload file into temporary directory
// close and delete the file when done
// if the file is left open, it will be deleted automatically on disconnect

class AsyncFileUploadWebHandler : public AsyncCallbackWebHandler {
public:
    AsyncFileUploadWebHandler(const String &uri, ArRequestHandlerFunction _onRequest = nullptr);

private:
    void _cleanUp(AsyncWebServerRequest *request);
    void _handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
};
