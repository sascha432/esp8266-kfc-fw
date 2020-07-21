/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>
#include <array>

#if defined(ESP32)

#include <esp_wifi_types.h>

using WiFiEncryptionTypeArray = std::array<uint8_t, 6>;
using WiFiEncryptionType = wifi_auth_mode_t;
static constexpr auto kWiFiEncryptionTypeDefault = WIFI_AUTH_WPA2_PSK;

inline WiFiEncryptionTypeArray createWiFiEncryptionTypeArray() {
    return {WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_ENTERPRISE};
}

#elif defined(ESP8266)

#include <WiFiServer.h>

using WiFiEncryptionTypeArray = std::array<uint8_t, 5>;
using WiFiEncryptionType = wl_enc_type;
static constexpr auto kWiFiEncryptionTypeDefault = ENC_TYPE_CCMP;

inline WiFiEncryptionTypeArray createWiFiEncryptionTypeArray() {
    return {ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO};
}

#else

#error Platform not supported

#endif

enum HttpMode_t : uint8_t {
    HTTP_MODE_DISABLED = 0,
    HTTP_MODE_UNSECURE,
    HTTP_MODE_SECURE,
};

enum MQTTMode_t : uint8_t {
    MQTT_MODE_DISABLED = 0,
    MQTT_MODE_UNSECURE,
    MQTT_MODE_SECURE,
};

typedef uint32_t ConfigFlags_t;

struct ConfigFlags {
    ConfigFlags_t isFactorySettings: 1;
    ConfigFlags_t isDefaultPassword: 1; //TODO disable password after 5min if it has not been changed
    ConfigFlags_t ledMode: 1;
    ConfigFlags_t wifiMode: 2;
    ConfigFlags_t atModeEnabled: 1;
    ConfigFlags_t hiddenSSID: 1;
    ConfigFlags_t softAPDHCPDEnabled: 1;
    ConfigFlags_t stationModeDHCPEnabled: 1;
    ConfigFlags_t webServerMode: 2;
    ConfigFlags_t ntpClientEnabled: 1;
    ConfigFlags_t syslogProtocol: 3;
    ConfigFlags_t mqttMode: 2;
    ConfigFlags_t mqttAutoDiscoveryEnabled: 1;
    ConfigFlags_t restApiEnabled: 1;
    ConfigFlags_t serial2TCPEnabled: 1;
    ConfigFlags_t __reserved:2; // free to use
    ConfigFlags_t useStaticIPDuringWakeUp: 1;
    ConfigFlags_t webServerPerformanceModeEnabled: 1;
    ConfigFlags_t apStandByMode: 1;
    ConfigFlags_t disableWebUI: 1;
    ConfigFlags_t disableWebAlerts: 1;
    ConfigFlags_t enableMDNS: 1;
};

static_assert(sizeof(ConfigFlags) == sizeof(uint32_t), "size exceeded");
