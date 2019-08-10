/**
 * Author: sascha_lammers@gmx.de
 */

// replaced by a web socket

#if 0

// web handler for dimmer.html

#include "dimmer_web_handler.h"
#include  <HttpHeaders.h>
#include "dimmer_module.h"
#include "progmem_data.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(dimmer_uri, "/dimmer_ctrl/");
PROGMEM_STRING_DEF(channel, "channel");
PROGMEM_STRING_DEF(level, "level");

bool DimmerModuleWebHandler::canHandle(AsyncWebServerRequest *request) {
    if (strncmp_P(request->url().c_str(), _uri, strlen_P(_uri))) {
        return false;
    }
    //request->addInterestingHeader(F("ANY"));
    return true;
}

void DimmerModuleWebHandler::handleRequest(AsyncWebServerRequest *request) {
    auto uri = request->url().substring(strlen_P(_uri));
    auto dimmer = Driver_DimmerModule::getInstance();
    _debug_printf_P(PSTR("dimmer module '%s' (%s) dimmer %p\n"), uri.c_str(), request->url().c_str(), dimmer);

    if (web_server_is_authenticated(request)) {
        if (dimmer) {
            _handleRequest(request, uri);
        } 
        else {
            request->send(503);
        }
    } else {
        request->send(403);
    }
}

void DimmerModuleWebHandler::setup() {
    if (get_web_server_object()) {
        web_server_add_handler(new DimmerModuleWebHandler(FSPGM(dimmer_uri)));
    }
}

void DimmerModuleWebHandler::_handleRequest(AsyncWebServerRequest *request, const String &uri) {

#if DEBUG_IOT_DIMMER_MODULE
    int channel = request->arg(FSPGM(channel)).toInt();
    int level = request->arg(FSPGM(level)).toInt();
    _debug_printf_P(PSTR("_handleRequest(%s): channel %u, level %u\n"), uri.c_str(), channel, level);
#endif

    if (request->hasArg(FSPGM(channel)) && request->hasArg(FSPGM(level))) {
        auto dimmer = Driver_DimmerModule::getInstance();
        if (dimmer) {
            int channel = request->arg(FSPGM(channel)).toInt();
            int level = request->arg(FSPGM(level)).toInt();
            dimmer->setChannel(channel, level);
        }
    }

    HttpHeaders httpHeaders(false);
    httpHeaders.addNoCache();
    auto response = request->beginResponse_P(200, F("text/plain"), PSTR("OK"));
    httpHeaders.setWebServerResponseHeaders(response);
    request->send(response);
}


String DimmerTemplate::process(const String &key) {
    if (key == F("MAX_BRIGHTNESS")) {
        return String(IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
    }
    else if (key.startsWith(F("CHANNEL")) && key.endsWith(F("_VALUE"))) {
        auto dimmer = Driver_DimmerModule::getInstance();
        if (!dimmer) {
            return _sharedEmptyString;
        }
        auto channel = (key.charAt(7) - '0');
        _debug_printf_P(PSTR("channel %u level %d\n"), channel, dimmer->getChannel(channel));
        if (channel >= IOT_DIMMER_MODULE_CHANNELS) {
            return _sharedEmptyString;
        }
        return String(dimmer->getChannel(channel));
    }
    return WebTemplate::process(key);
}

#endif
