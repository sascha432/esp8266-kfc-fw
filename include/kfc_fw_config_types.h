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

enum class SyslogProtocolType : uint8_t {
    MIN = 0,
    NONE = MIN,
    UDP,
    TCP,
    TCP_TLS,
    FILE,
    MAX
};
