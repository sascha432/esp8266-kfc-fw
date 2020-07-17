/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include <kfc_fw_config_classes.h>

namespace KFCConfigurationClasses {

    void Plugins::Serial2TCP::defaults()
    {
        Serial2Tcp_t cfg = { 2323, KFC_SERIAL_RATE, ModeType::SERVER, SerialPortType::SERIAL0, 0, 0, false, true, 30, 30, 300 };
        setConfig(cfg);
    }

    bool Plugins::Serial2TCP::isEnabled()
    {
        using KFCConfigurationClasses::System;
        return System::Flags::get().serial2TCPEnabled;
    }

    Plugins::Serial2TCP::Serial2Tcp_t Plugins::Serial2TCP::getConfig()
    {
        return config._H_GET(MainConfig().plugins.serial2tcp.cfg);
    }

    void Plugins::Serial2TCP::setConfig(const Serial2Tcp_t &params)
    {
        config._H_SET(MainConfig().plugins.serial2tcp.cfg, params);
    }

    Plugins::Serial2TCP::Serial2Tcp_t &Plugins::Serial2TCP::getWriteableConfig()
    {
        return config._H_W_GET(MainConfig().plugins.serial2tcp.cfg);
    }

    const char *Plugins::Serial2TCP::getHostname()
    {
        return config._H_STR(MainConfig().plugins.serial2tcp.hostname);
    }

    const char *Plugins::Serial2TCP::getUsername()
    {
        return config._H_STR(MainConfig().plugins.serial2tcp.username);
    }

    const char *Plugins::Serial2TCP::getPassword()
    {
        return config._H_STR(MainConfig().plugins.serial2tcp.password);
    }

    void Plugins::Serial2TCP::setHostname(const char *hostname)
    {
        config._H_SET_STR(MainConfig().plugins.serial2tcp.hostname, hostname);
    }

    void Plugins::Serial2TCP::setUsername(const char *username)
    {
        config._H_SET_STR(MainConfig().plugins.serial2tcp.username, username);
    }

    void Plugins::Serial2TCP::setPassword(const char *password)
    {
        config._H_SET_STR(MainConfig().plugins.serial2tcp.password, password);
    }

}
