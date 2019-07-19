/**
 * Author: sascha_lammers@gmx.de
 */


#if HUE_EMULATION

#include <Arduino.h>
// __AUTOMATED_HEADERS_STRART
#if defined(ESP32)
#include <WiFi.h>
#include <SPIFFS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <FS.h>
#else
#error Platform not supported
#endif
// __AUTOMATED_HEADERS_END
#include <vector>
#include <ESPAsyncWebServer.h>
#include "fauxmoESP.h"

#ifndef DEBUG_HUE_EMULATION
#define DEBUG_HUE_EMULATION 1
#endif

typedef std::vector<String> HueDeviceList;

void hue_setup_emulation();
void hue_update_config();
void hue_register_port(uint16_t);
bool hue_onRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
bool hue_onNotFound(AsyncWebServerRequest *request);
String hue_getDevices();
void hue_clearDevices();
HueDeviceList &hue_getDeviceNames();

class HueSettingsForm : public SettingsForm {
public:
    HueSettingsForm(AsyncWebServerRequest *request);
};

#endif
