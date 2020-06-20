/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"
#include "kfc_fw_config_classes.h"
#include "progmem_data.h"

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

        _flags.wifiMode = WIFI_AP_STA;
        _flags.stationModeDHCPEnabled = true;
        _flags.softAPDHCPDEnabled = true;

        _flags.mqttAutoDiscoveryEnabled = true;
        _flags.mqttMode = MQTT_MODE_DISABLED;

        _flags.ntpClientEnabled = true;

#if defined(ESP32) && WEBSERVER_TLS_SUPPORT
        _flags.webServerMode = HTTP_MODE_SECURE;
#else
        _flags.webServerMode = HTTP_MODE_UNSECURE;
#endif
        _flags.webServerPerformanceModeEnabled = true;

        _flags.ledMode = true;

        _flags.useStaticIPDuringWakeUp = true;

        _flags.serial2TCPMode = SERIAL2TCP_MODE_DISABLED;

        _flags.syslogProtocol = SYSLOG_PROTOCOL_TCP;

        _flags.serial2TCPMode = SERIAL2TCP_MODE_DISABLED;

        _flags.apStandByMode = true;
    }

    System::Flags System::Flags::read()
    {
        return Flags(config._H_GET(Config().flags));
    }

    ConfigFlags System::Flags::get()
    {
        return config._H_GET(Config().flags);
    }

    void System::Flags::write()
    {
        config._H_SET(Config().flags, _flags);
    }

    void System::Device::defaults()
    {
        DeviceSettings_t settings;
        settings._safeModeRebootTime = 0;
        settings._webUIKeepLoggedInDays = 30;
        config._H_SET(MainConfig().system.device.settings, settings);

        setSafeModeRebootTime(0);
        setTitle(FSPGM(KFC_Firmware));
    }

    const char *System::Device::getName()
    {
        return config._H_STR(Config().device_name);
    }

    const char *System::Device::getTitle()
    {
        return config._H_STR(Config().device_title);
    }

    const char *System::Device::getPassword()
    {
        return config._H_STR(Config().device_pass);
    }

    const char *System::Device::getToken()
    {
        return config._H_STR(Config().device_token);
    }

    void System::Device::setName(const String &name)
    {
        config._H_SET_STR(Config().device_name, name);
    }

    void System::Device::setTitle(const String &title)
    {
        config._H_SET_STR(Config().device_title, title);
    }

    void System::Device::setPassword(const String &password)
    {
        config._H_SET_STR(Config().device_pass, password);
    }

    void System::Device::setToken(const String &token)
    {
        config._H_SET_STR(Config().device_token, token);
    }

    void System::Device::setSafeModeRebootTime(uint16_t minutes)
    {
        auto settings = config._H_GET(MainConfig().system.device.settings);
        settings._safeModeRebootTime = minutes;
        config._H_SET(MainConfig().system.device.settings, settings);
    }

    uint16_t System::Device::getSafeModeRebootTime()
    {
        return config._H_GET(MainConfig().system.device.settings)._safeModeRebootTime;
    }

    uint8_t System::Device::getWebUIKeepLoggedInDays()
    {
        return config._H_GET(MainConfig().system.device.settings)._webUIKeepLoggedInDays;
    }

    uint32_t System::Device::getWebUIKeepLoggedInSeconds()
    {
        return getWebUIKeepLoggedInDays() * 86400;
    }

    void System::Device::setWebUIKeepLoggedInDays(uint8_t days)
    {
        auto settings = config._H_GET(MainConfig().system.device.settings);
        settings._webUIKeepLoggedInDays = days;
        config._H_SET(MainConfig().system.device.settings, settings);
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
        flags->apStandByMode = true;
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
        _encryption = WiFiEncryptionTypeDefault;
        _channel = 7;
    }

    Network::SoftAP Network::SoftAP::read()
    {
        return (Network::SoftAP)config._H_GET(MainConfig().network.softAp);
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


    void Plugins::HomeAssistant::setApiEndpoint(const String &endpoint, uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint, endpoint);
                break;
            case 1:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint1, endpoint);
                break;
            case 2:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint2, endpoint);
                break;
            case 3:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint3, endpoint);
                break;
            default:
                __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        }
    }

    void Plugins::HomeAssistant::setApiToken(const String &token, uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token, token);
                break;
            case 1:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token1, token);
                break;
            case 2:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token2, token);
                break;
            case 3:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token3, token);
                break;
            default:
                __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        }
    }

    const char *Plugins::HomeAssistant::getApiEndpoint(uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint);
            case 1:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint1);
            case 2:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint2);
            case 3:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint3);

        }
        __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        return nullptr;
    }

    const char *Plugins::HomeAssistant::getApiToken(uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                return config._H_STR(MainConfig().plugins.homeassistant.token);
            case 1:
                return config._H_STR(MainConfig().plugins.homeassistant.token1);
            case 2:
                return config._H_STR(MainConfig().plugins.homeassistant.token2);
            case 3:
                return config._H_STR(MainConfig().plugins.homeassistant.token3);

        }
        __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        return nullptr;
    }

    void Plugins::HomeAssistant::getActions(ActionVector &actions)
    {
        uint16_t length;
        auto data = config.getBinary(_H(MainConfig().plugins.homeassistant.actions), length);
        if (data) {
            auto endPtr = data + length;
            while(data + sizeof(ActionHeader_t) <= endPtr) {
                auto header = reinterpret_cast<const ActionHeader_t *>(data);
                data += sizeof(ActionHeader_t);
                auto valuesLen = header->valuesLen * sizeof(int32_t);
                if (data + header->entityLen + valuesLen > endPtr) {
                    break;
                }
                String str = PrintString(data, header->entityLen);
                data += header->entityLen;
                Action::ValuesVector values(header->valuesLen);
                memcpy(values.data(), data, valuesLen);
                data += valuesLen;
                actions.emplace_back(header->id, header->apiId, header->action, values, str);
            }
        }
    }

    void Plugins::HomeAssistant::setActions(ActionVector &actions)
    {
        Buffer buffer;
        for(auto &action: actions) {
            ActionHeader_t header;
            header.entityLen = action.getEntityId().length();
            if (action.getId() && header.entityLen) {
                header.id = action.getId();
                header.action = action.getAction();
                header.valuesLen = action.getNumValues();
                header.apiId = action.getApiId();
                buffer.writeObject(header);
                buffer.write(action.getEntityId());
                buffer.writeVector(action.getValues());
            }
        }
        config.setBinary(_H(MainConfig().plugins.homeassistant.actions), buffer.get(), buffer.length());
    }

    Plugins::HomeAssistant::Action Plugins::HomeAssistant::getAction(uint16_t id)
    {
        ActionVector actions;
        getActions(actions);
        for(auto &action: actions) {
            if (id == action.getId()) {
                return action;
            }
        }
        return Action();
    }

    const __FlashStringHelper *Plugins::HomeAssistant::getActionStr(ActionEnum_t action)
    {
        switch(action) {
            case TURN_ON:
                return F("Turn On");
            case TURN_OFF:
                return F("Turn Off");
            case SET_BRIGHTNESS:
                return F("Set Brightness");
            case CHANGE_BRIGHTNESS:
                return F("Change Brightness");
            case TOGGLE:
                return F("Toggle");
            case TRIGGER:
                return F("Trigger");
            case SET_KELVIN:
                return F("Set Kelvin");
            case SET_MIREDS:
                return F("Set Mireds");
            case SET_RGB_COLOR:
                return F("Set RGB Color");
            case MEDIA_NEXT_TRACK:
                return F("Next Track");
            case MEDIA_PAUSE:
                return F("Media Pause");
            case MEDIA_PLAY:
                return F("Media Play");
            case MEDIA_PLAY_PAUSE:
                return F("Media Play/Pause");
            case MEDIA_PREVIOUS_TRACK:
                return F("Previous Track");
            case MEDIA_STOP:
                return F("Media Stop");
            case VOLUME_DOWN:
                return F("Volume Down");
            case VOLUME_MUTE:
                return F("Volume Mute");
            case VOLUME_UNMUTE:
                return F("Volume Unmute");
            case VOLUME_SET:
                return F("Volume Set");
            case VOLUME_UP:
                return F("Volume Up");
            case MQTT_SET:
                return F("MQTT Set");
            case MQTT_TOGGLE:
                return F("MQTT Toggle");
            case MQTT_INCR:
                return F("MQTT Increase");
            case MQTT_DECR:
                return F("MQTT Decrease");
            default:
                return F("None");
        }
    }

    void Plugins::RemoteControl::defaults()
    {
        config._H_SET(MainConfig().plugins.remotecontrol, Plugins::RemoteControl());
    }

    Plugins::RemoteControl Plugins::RemoteControl::get()
    {
        return config._H_GET(MainConfig().plugins.remotecontrol);
    }


    void Plugins::WeatherStation::defaults()
    {
        Plugins::WeatherStation ws;
        ::config._H_SET_STR(MainConfig().plugins.weatherstation.openweather_api_key, F("GET_YOUR_API_KEY_openweathermap.org"));
        ::config._H_SET_STR(MainConfig().plugins.weatherstation.openweather_api_query, F("New York,US"));
        ::config._H_SET(MainConfig().plugins.weatherstation.config, ws.config);
    }

    const char *Plugins::WeatherStation::getApiKey()
    {
        return ::config._H_STR(MainConfig().plugins.weatherstation.openweather_api_key);
    }

    const char *Plugins::WeatherStation::getQueryString()
    {
        return ::config._H_STR(MainConfig().plugins.weatherstation.openweather_api_query);
    }

    Plugins::WeatherStation::WeatherStationConfig_t &Plugins::WeatherStation::getWriteableConfig()
    {
        return ::config._H_W_GET(MainConfig().plugins.weatherstation.config);
    }

    Plugins::WeatherStation::WeatherStationConfig_t Plugins::WeatherStation::getConfig()
    {
        return ::config._H_GET(MainConfig().plugins.weatherstation.config);
    }

};
