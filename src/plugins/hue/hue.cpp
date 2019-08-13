/**
 * Author: sascha_lammers@gmx.de
 */

#if HUE_EMULATION

#include "hue.h"
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <KFCForms.h>
//#include <EventScheduler.h>
#include <vector>
#include "plugins.h"
#include "kfc_fw_config.h"
#include "progmem_data.h"
#include "../include/templates.h"
#include "fauxmoESP.h"

#if DEBUG_HUE_EMULATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

HueEmulation *hueEmulation = nullptr;


HueEmulation::HueEmulation() {
    _debug_println(F("HueEmulation::HueEmulation()"));
    _initHue();
}

HueEmulation::~HueEmulation() {
    _debug_println(F("HueEmulation::~HueEmulation()"));
    WiFiCallbacks::remove(WiFiCallbacks::ANY, HueEmulation::wifiCallback);
    LoopFunctions::remove(HueEmulation::loop);
    delete _fauxmo;
}

void HueEmulation::wifiCallback(uint8_t event, void *payload) {

    if (event == WiFiCallbacks::CONNECTED) {
        _debug_printf_P(PSTR("HueEmulation::wifiCallback(): connected, enabling fauxmoESP\n"))
        LoopFunctions::add(HueEmulation::loop);
        hueEmulation->_fauxmo->enable(true);
    }
    else if (event == WiFiCallbacks::DISCONNECTED) {
        _debug_printf_P(PSTR("HueEmulation::wifiCallback(): disconnected, disabling fauxmoESP\n"))
        hueEmulation->_fauxmo->enable(false);
        LoopFunctions::remove(HueEmulation::loop);
    }

}

void HueEmulation::loop() {
    hueEmulation->_fauxmo->handle();
}

void HueEmulation::setState(unsigned char device_id, const char * device_name, bool state, unsigned char value) {
    _debug_printf_P(PSTR("HueEmulation::setState(%u, %s, %d, %u)\n"), (unsigned)device_id, device_name, state, (unsigned)value);

    // Callback when a command from Alexa is received.
    // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
    // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
    // Just remember not to delay too much here, this is a callback, exit as soon as possible.
    // If you have to do something more involved here set a flag and process it in your main loop.

    // if (0 == device_id) digitalWrite(RELAY1_PIN, state);
    // if (1 == device_id) digitalWrite(RELAY2_PIN, state);
    // if (2 == device_id) analogWrite(LED1_PIN, value);

    // debug_printf_P(PSTR("[MAIN] Device #%d (%s) state: %s value: %d\n"), device_id, device_name, state ? "ON" : "OFF", value);

}

bool HueEmulation::onNotFound(AsyncWebServerRequest *request) {

    if (hueEmulation) {
        _debug_printf_P(PSTR("HueEmulation::onNotFound(%s)\n"), request->url().c_str());
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (hueEmulation->_fauxmo->process(request->client(), request->method() == HTTP_GET, request->url(), body)) {
            _debug_printf_P(PSTR("HueEmulation::onNotFound(): handled\n"));
            return true;
        }
    }
    return false;
}

void HueEmulation::onRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

    if (hueEmulation) {
        PrintString str;
        str.write(data, len);
        _debug_printf_P(PSTR("HueEmulation::onRequestBody(%s, %s, %d)\n"), request->url().c_str(), str.c_str(), len);
        if (hueEmulation->_fauxmo->process(request->client(), request->method() == HTTP_GET, request->url(), str)) {
            _debug_printf_P(PSTR("HueEmulation::onRequestBody(): was handled\n"));
        }
    }
}

const String HueEmulation::getStatus() {

    if (hueEmulation) {
        PrintHtmlEntitiesString output;
        output.printf_P(PSTR("Enabled @ Port %d %s" HTML_S(br)), config._H_GET(Config().hue.tcp_port), hueEmulation->_standalone ? PSTR("as standalone server") : PSTR("shared web server"));
        auto devices = HueEmulation::createDeviceList();
        for(const auto &name: devices) {
            auto deviceId = hueEmulation->_fauxmo->getDeviceId(name.c_str());
            output.printf_P(PSTR("%s #%d" HTML_S(br)), name.c_str(), deviceId);
        }
        return output;
    }
    return FSPGM(Disabled);
}

void HueEmulation::_initHue() {

    _fauxmo = _debug_new fauxmoESP();

    auto huePort = config._H_GET(Config().hue.tcp_port);
    auto webServerPort = config._H_GET(Config().flags).webServerMode == HTTP_MODE_UNSECURE ? config._H_GET(Config().http_port) : 0;
    _standalone = (huePort != webServerPort);

    _debug_printf_P(PSTR("HueEmulation::_init(): HUE emulation @ port %u %s\n"), (unsigned)huePort, _standalone ? PSTR("as standalone server") : PSTR("shared with web server"));

    auto devices = createDeviceList();
    for(const auto &name: devices) {
        _fauxmo->addDevice(name.c_str());
    }

    _fauxmo->onSetState(HueEmulation::setState);

    _fauxmo->createServer(_standalone);
    _fauxmo->setPort(huePort);

#if HUE_EMULATION_USE_WIFI_CALLBACK
    WiFiCallbacks::add(WiFiCallbacks::CONNECTED|WiFiCallbacks::DISCONNECTED, HueEmulation::wifiCallback);
    if (WiFi.isConnected()) {
        HueEmulation::wifiCallback(WiFiCallbacks::CONNECTED, nullptr); // simulate event if WiFi is already connected
    }
#else
    _fauxmo->enable(true);
    LoopFunctions::add(HueEmulation::loop);
#endif
}

HueEmulation::HueDeviceVector HueEmulation::createDeviceList() {
    HueEmulation::HueDeviceVector _devices;
    _devices.push_back("test1");
    _devices.push_back("test2");
    return _devices;
    // auto parameter = config.getParameter<char *>(_H(Config().hue.devices));
    // if (parameter) {
        const char *devices = config._H_STR(Config().hue.devices); //getString(parameter->getHandle());
        _debug_printf_P(PSTR("HueEmulation::createDeviceList(): from='%s'\n"), devices);
        const char *ptr = devices;
        const char *startPtr;
        while(*ptr) {
            while(isspace(*ptr)) { // trim white space
                ptr++;
            }
            startPtr = ptr;
            while(*ptr && *ptr != '\n') { // find end of device name
                ptr++;
            }
            if (startPtr != ptr) {
                PrintString str;
                str.write(reinterpret_cast<const uint8_t *>(startPtr), ptr - startPtr);
                _debug_printf_P(PSTR("HueEmulation::createDeviceList(): adding '%s'\n"), str.c_str());
                _devices.push_back(str);
            }
        }
        // parameter->release();
    // }
    return _devices;
}

void hue_setup() {

    if (hueEmulation) {
        delete hueEmulation;
        hueEmulation = nullptr;
    }
    if (config._H_GET(Config().flags).hueEnabled) {
        hueEmulation = _debug_new HueEmulation();
    }
}

void hue_reconfigure(PGM_P source) {
    hue_setup();
}

// String hue_getDevices() {
//     String out;
//     uint8_t n = 0;
//     out = F("<script>window.hueDevices=[");
//     for(String &name: hue_devices) {
//         String escaped = name;
//         if (n++ != 0) {
//             out += ',';
//         }
//         out += '"';
//         escaped.replace("\\", "\\\\");
//         escaped.replace("\"", "\\\"");
//         out += escaped;
//         out += '"';
//     }
//     out += F("];</script>");
//     return out;
// }

void hue_create_settings_form(AsyncWebServerRequest *request, Form &form) {

    form.add<bool>(F("hue_enabled"), _H_STRUCT_FORMVALUE(Config().flags, bool, hueEnabled));

    form.add<uint16_t>(F("tcp_port"), config._H_W_GET(Config().hue.tcp_port));
    form.addValidator(new FormRangeValidator(F("Invalid port"), 1, 65535));

    uint8_t count = 0;
    char name[20];
    auto devices = HueEmulation::createDeviceList();
    for(const auto &deviceName: devices) {
        snprintf_P(name, sizeof(name), PSTR("device_name_%u"), count++);
        form.add<0>(String(name), (char *)deviceName.c_str()); //TODO deviceName only exists inside this loop
    }

    form.finalize();
}

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ hue,
/* setupPriority            */ 100,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ hue_setup,
/* statusTemplate           */ HueEmulation::getStatus,
/* configureForm            */ hue_create_settings_form,
/* reconfigurePlugin        */ hue_reconfigure,
/* reconfigure Dependencies */ SPGM(plugin_config_name_http),
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ nullptr
);

#endif
