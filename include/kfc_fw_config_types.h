/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>
#include <stl_ext/array.h>

#if defined(ESP32)

#include <esp_wifi_types.h>

static constexpr auto kWiFiEncryptionTypeDefault = WIFI_AUTH_WPA2_PSK;
static constexpr auto kWiFiEncryptionTypes = stdex::array_of<const uint8_t>(WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_ENTERPRISE);

#elif defined(ESP8266)

#include <WiFiServer.h>

static constexpr auto kWiFiEncryptionTypeDefault = ENC_TYPE_CCMP;
static constexpr auto kWiFiEncryptionTypes = stdex::array_of<const uint8_t>(ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO);

#else

#error Platform not supported

#endif
using WiFiEncryptionType = decltype(kWiFiEncryptionTypeDefault);


enum class SyslogProtocolType : uint8_t {
    MIN = 0,
    NONE = MIN,
    UDP,
    TCP,
    TCP_TLS,
    FILE,
    MAX
};

using SyslogProtocol = SyslogProtocolType;

#undef DISABLED

namespace WebServerTypes {

    enum class ModeType : uint8_t {
        MIN = 0,
        DISABLED = 0,
        UNSECURE,
        SECURE,
        MAX
    };

}
