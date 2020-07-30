/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::Serial2TCP::defaults()
    {
        Serial2Tcp_t cfg = {};
        setConfig(cfg);
    }

    bool Plugins::Serial2TCP::isEnabled()
    {
        using KFCConfigurationClasses::System;
        return System::Flags::getConfig().serial2TCPEnabled;
    }

}
