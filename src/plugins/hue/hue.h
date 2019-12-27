/**
 * Author: sascha_lammers@gmx.de
 */


#if HUE_EMULATION

#include <Arduino_compat.h>
#include <vector>
#include <ESPAsyncWebServer.h>
#include "fauxmoESP.h"

#ifndef DEBUG_HUE_EMULATION
#define DEBUG_HUE_EMULATION                     0
#endif

#ifndef HUE_EMULATION_USE_WIFI_CALLBACK
#if DEBUG_HUE_EMULATION
// enable fauxmo only when WiFi is connected
#define HUE_EMULATION_USE_WIFI_CALLBACK         1
#else
#define HUE_EMULATION_USE_WIFI_CALLBACK         0
#endif
#endif

#ifndef PIO_FRAMEWORK_ARDUINO_LWIP_HIGHER_BANDWIDTH
#error PIO_FRAMEWORK_ARDUINO_LWIP_HIGHER_BANDWIDTH required
#endif


class HueEmulation {
public:
    typedef std::vector<String> HueDeviceVector;

    HueEmulation();
    ~HueEmulation();

    static void wifiCallback(uint8_t event, void *payload);
    static void loop();
    static void setState(unsigned char device_id, const char * device_name, bool state, unsigned char value);

    static bool onNotFound(AsyncWebServerRequest *request);
    static void onRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    virtual void getStatus(Print &output) override;
    static HueDeviceVector createDeviceList();

private:
    void _initHue();

    fauxmoESP *_fauxmo;
    uint16_t _port;
    bool _standalone;
};

#endif
