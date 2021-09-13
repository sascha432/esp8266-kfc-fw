/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::DisplayConfigNS::Display::defaults()
    {
        setConfig(DisplayConfig::Config_t());
    }
}
