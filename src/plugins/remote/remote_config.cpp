/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::RemoteControl::defaults()
    {
        setConfig(RemoteControlConfig::Config_t());
    }

}
