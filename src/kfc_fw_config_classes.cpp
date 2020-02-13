/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"
#include "kfc_fw_config_classes.h"

/*

Network::WiFiConfig::getSoftApSSID();
Network::WiFiConfig::getSoftApPassword();


*/

namespace KFCConfigurationClasses {

    System::Flags::Flags() : _flags()
    {
        _flags.isFactorySettings = true;
        _flags.isDefaultPassword = true;
        _flags.atModeEnabled = true;
#if defined(ESP32) && WEBSERVER_TLS_SUPPORT
        _flags.webServerMode = HTTP_MODE_SECURE;
#else
        _flags.webServerMode = HTTP_MODE_UNSECURE;
#endif
#if defined(ESP8266)
        _flags.webServerPerformanceModeEnabled = true;
#endif
        _flags.ledMode = true;
        _flags.useStaticIPDuringWakeUp = true;
#if SERIAL2TCP
        _flags.serial2TCPMode = SERIAL2TCP_MODE_DISABLED;
#endif
    }

    System::Flags System::Flags::read()
    {
        return Flags(config._H_GET(Config().flags));
    }

    void System::Flags::write()
    {
        config._H_SET(Config().flags, _flags);
    }


    Network::Settings::Settings()
    {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        _localIp = IPAddress(192, 168, 4, mac[5] <= 1 || mac[5] >= 253 ? (mac[4] <= 1 || mac[4] >= 253 ? (mac[3] <= 1 || mac[3] >= 253 ? mac[3] : rand() % 98 + 1) : mac[4]) : mac[5]);
        _gateway = IPAddress(192, 168, 4, 1);
        _subnet = IPAddress(255, 255, 255, 0);
        _dns1 = IPAddress(8, 8, 8, 8);
        _dns2 = IPAddress(8, 8, 4, 4);
    }

    void Network::Settings::write()
    {
        config._H_SET(MainConfig().network.settings, *this);
    }

    Network::Settings Network::Settings::read()
    {
        return config._H_GET(MainConfig().network.settings);
    }

    void Network::Settings::defaults()
    {
        auto settings = Settings();
        settings.write();

        auto flags = System::Flags();
        flags->wifiMode = WIFI_AP;
        flags->stationModeDHCPEnabled = true;
        flags.write();
    }

    Network::SoftAP::SoftAP()
    {
        _address = IPAddress(192, 168, 4, 1);
        _subnet = IPAddress(255, 255, 255, 0);
        _gateway = IPAddress(192, 168, 4, 1);
        _dhcpStart = IPAddress(192, 168, 4, 2);
        _dhcpEnd = IPAddress(192, 168, 4, 100);
        _encryption = WIFI_DEFAULT_ENCRYPTION;
        _channel = 7;
    }

    Network::SoftAP Network::SoftAP::read()
    {
        return config._H_GET(MainConfig().network.softAp);
    }

    void Network::SoftAP::defaults()
    {
        auto softAp = SoftAP();
        softAp.write();

        auto flags = System::Flags();
        flags->softAPDHCPDEnabled = true;
        flags.write();
    }

    void Network::SoftAP::write()
    {
        config._H_SET(MainConfig().network.softAp, *this);
    }

    const char *Network::WiFiConfig::getSSID()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._ssid);
    }

    const char *Network::WiFiConfig::getPassword()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._password);
    }

    void Network::WiFiConfig::setSSID(const String &ssid)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._ssid, ssid);
    }

    void Network::WiFiConfig::setPassword(const String &password)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._password, password);
    }


    const char *Network::WiFiConfig::getSoftApSSID()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._softApSsid);
    }

    const char *Network::WiFiConfig::getSoftApPassword()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._softApPassword);
    }

    void Network::WiFiConfig::setSoftApSSID(const String &ssid)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._softApSsid, ssid);
    }

    void Network::WiFiConfig::setSoftApPassword(const String &password)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._softApPassword, password);
    }


};
