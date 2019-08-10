/**
 * Author: sascha_lammers@gmx.de
 */

#if 0

#pragma once

#include "web_server.h"
#include "templates.h"

class DimmerModuleWebHandler : public AsyncWebHandler {
public:
    DimmerModuleWebHandler(const __FlashStringHelper *uri) : AsyncWebHandler() {
        _uri = reinterpret_cast<PGM_P>(uri);
    }

    virtual bool canHandle(AsyncWebServerRequest *request) override;
    virtual void handleRequest(AsyncWebServerRequest *request) override;

    static void setup();

private:
    void _handleRequest(AsyncWebServerRequest *request, const String &uri);

    PGM_P _uri;
};

class DimmerTemplate : public WebTemplate {
public:
    virtual String process(const String &key) override;
};

#endif
