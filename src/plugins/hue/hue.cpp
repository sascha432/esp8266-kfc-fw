/**
 * Author: sascha_lammers@gmx.de
 */

// lib_deps = fauxmoESP

#if HUE_EMULATION

#include <vector>
#include <LoopFunctions.h>
#include "event_scheduler.h"
#include "kfc_fw_config.h"
#include "iot_hue.h"
#include "progmem_data.h"
#include "templates.h"
#include "fauxmoESP.h"

#if DEBUG_HUE_EMULATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


fauxmoESP *fauxmo = nullptr;
HueDeviceList hue_devices;
static uint16_t hue_port = 0; // points to the port the web server callbacks are registered to, usually 80 or 433. if the port does not match with the fauxmo port, a standalove server is opened

void hue_onWifiConnected(uint8_t event, void *) {

    if (event == WIFI_CB_EVENT_CONNECTED) {
        if (_Config.getOptions().isHueEmulation()) {
            _debug_println(F("Enabling fauxmoESP"));
            fauxmo->enable(true);
            dimmer.begin();
        } else {
            _debug_println(F("Disabling fauxmoESP"));
            fauxmo->enable(false);
            dimmer.end();
        }
    } else if (WIFI_CB_EVENT_DISCONNECTED) {
        _debug_println(F("Disabling fauxmoESP after wifi disconnect"));
        dimmer.end();
        fauxmo->enable(false);
    }
}

void hue_register_port(uint16_t port) {
    hue_port = port;
}

void hue_loop(void *) {
    fauxmo->handle();
}

void hue_setup_emulation() {

    WebTemplate::registerVariable(F("HUE_STATUS"), hue_getStatus);
    WebTemplate::registerForm(F("hue.html"), [](AsyncWebServerRequest *request) {
        return reinterpret_cast<Form *>(new HueSettingsForm(request));
    }, [](Form *) {
        hue_update_config();
    });

    if (!_Config.getOptions().isHueEmulation()) {
        _debug_println(F("HUE emulation disabled"));
        hue_clearDevices();
        return;
    }

    fauxmo = _debug_new fauxmoESP();

    _debug_printf_P(PSTR("Starting HUE emulation @ port %d %s\n"), _Config.get().hue.tcp_port, _Config.get().hue.tcp_port != hue_port ? "as standalone server" : "shared web server");
    fauxmo->createServer(_Config.get().hue.tcp_port != hue_port); // web server already exists and supportrs callbacks
    fauxmo->setPort(_Config.get().hue.tcp_port);

    hue_getDeviceNames(); // fills devices vector if empty

    for(auto &name: hue_devices) {
        fauxmo->addDevice(name.c_str());
    }

    fauxmo->onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {

        // Callback when a command from Alexa is received.
        // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
        // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
        // Just remember not to delay too much here, this is a callback, exit as soon as possible.
        // If you have to do something more involved here set a flag and process it in your main loop.

        // if (0 == device_id) digitalWrite(RELAY1_PIN, state);
        // if (1 == device_id) digitalWrite(RELAY2_PIN, state);
        // if (2 == device_id) analogWrite(LED1_PIN, value);

        debug_printf_P(PSTR("[MAIN] Device #%d (%s) state: %s value: %d\n"), device_id, device_name, state ? "ON" : "OFF", value);
    });

    add_wifi_event_callback(hue_onWifiConnected, WIFI_CB_EVENT_CONNECTED|WIFI_CB_EVENT_DISCONNECTED);
    LoopFunctions.add(hue_loop);
}

void hue_update_config() {
    _debug_println(F("hue_update_config"));
    if (fauxmo) {
        LoopFunctions.remove(hue_loop);
        delete fauxmo;
        fauxmo = nullptr;
        hue_clearDevices();
    }
    hue_setup_emulation();
}

bool hue_onRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

    if (fauxmo) {
        if (fauxmo->process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) {
            return true;
        }
    }
    return false;
}

bool hue_onNotFound(AsyncWebServerRequest *request) {

    if (fauxmo) {
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (fauxmo->process(request->client(), request->method() == HTTP_GET, request->url(), body)) {
            return true;
        }
    }
    return false;
}

String hue_getStatus() {

    if (_Config.getOptions().isHueEmulation()) {
        PrintString out;
        out.printf_P(PSTR("Enabled @ Port %d %s\n"), _Config.get().hue.tcp_port, _Config.get().hue.tcp_port != hue_port ? "as standalone server" : "shared web server" HTML_E('br'));
        for(String &name: hue_devices) {
            int id = fauxmo->getDeviceId(name.c_str());
            out.printf_P(PSTR("%s #%d" HTML_E('br')), name.c_str(), id);
        }
    }
    return SPGM(Disabled);
}

String hue_getDevices() {
    String out;
    uint8_t n = 0;
    out = F("<script>window.hueDevices=[");
    for(String &name: hue_devices) {
        String escaped = name;
        if (n++ != 0) {
            out += ',';
        }
        out += '"';
        escaped.replace("\\", "\\\\");
        escaped.replace("\"", "\\\"");
        out += escaped;
        out += '"';
    }
    out += F("];</script>");
    return out;
}

void hue_clearDevices() {
    hue_devices.clear();
}

HueDeviceList &hue_getDeviceNames() {
    if (hue_devices.empty()) {
        char *str = strdup(_Config.get().hue.devices);
        if (str) {
        char *p;
            p = strtok(str, "\n");
            while(p) {
                if (*p) {
                    hue_devices.push_back(p);
                }
                p = strtok(NULL, "\n");
            }
            free(str);
        }
    }
    return hue_devices;
}

HueSettingsForm::HueSettingsForm(AsyncWebServerRequest *request) : SettingsForm(request) {

    add<ConfigFlags_t, 2>(F("hue_enabled"), &_Config.get().flags, array_of<ConfigFlags_t>(FormBitValue_UNSET_ALL, FLAGS_HUE_ENABLED));

    add<uint16_t>(F("tcp_port"), &_config.hue.tcp_port);
    addValidator(new FormRangeValidator(F("Invalid port"), 1, 65535));

    int n = 0;
    char name[20];
    for(auto &deviceName: hue_getDeviceNames()) {
        snprintf_P(name, sizeof(name), PSTR("device_name_%u"), n++);
        _add(new FormString<16>(String(name), (char *)deviceName.c_str()));
    }

    finalize();
}

#endif
