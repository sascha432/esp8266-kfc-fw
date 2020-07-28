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

typedef uint32_t ConfigFlags_t;

typedef struct __attribute__packed__ {
    ConfigFlags_t is_factory_settings: 1;
    ConfigFlags_t is_default_password: 1;
    ConfigFlags_t is_softap_enabled: 1;
    ConfigFlags_t is_softap_ssid_hidden: 1;
    ConfigFlags_t is_softap_standby_mode_enabled: 1;
    ConfigFlags_t is_softap_dhcpd_enabled: 1;
    ConfigFlags_t is_station_mode_enabled: 1;
    ConfigFlags_t is_station_mode_dhcp_enabled: 1;
    ConfigFlags_t use_static_ip_during_wakeup: 1;
    ConfigFlags_t is_led_on_when_connected: 1;
    ConfigFlags_t is_at_mode_enabled: 1;
    ConfigFlags_t is_mdns_enabled: 1;
    ConfigFlags_t is_ntp_client_enabled: 1;
    ConfigFlags_t is_syslog_enabled: 1;
    ConfigFlags_t is_web_server_enabled: 1;
    ConfigFlags_t is_webserver_performance_mode_enabled: 1;
    ConfigFlags_t is_mqtt_enabled: 1;
    ConfigFlags_t is_rest_api_enabled: 1;
    ConfigFlags_t is_serial2tcp_enabled: 1;
    ConfigFlags_t is_webui_enabled: 1;
    ConfigFlags_t is_webalerts_enabled: 1;
    ConfigFlags_t is_ssdp_enabled: 1;
    ConfigFlags_t __reserved: 10;

    uint8_t getWifiMode() const {
        return (is_station_mode_enabled ? WIFI_STA : 0) | (is_softap_enabled ? WIFI_AP : 0);
    }
    void setWifiMode(uint8_t mode) {
        is_station_mode_enabled = (mode & WIFI_STA);
        is_softap_enabled = (mode & WIFI_AP);
    }

} ConfigFlags;

static constexpr size_t ConfigFlagsSize = sizeof(ConfigFlags);

static_assert(ConfigFlagsSize == sizeof(uint32_t), "size exceeded");
