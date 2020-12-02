/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::RemoteControl::defaults()
    {
        config._H_SET(MainConfig().plugins.remotecontrol, Plugins::RemoteControl());
    }

    // Plugins::RemoteControl Plugins::RemoteControl::get()
    // {
    //     return config._H_GET(MainConfig().plugins.remotecontrol);
    // }

}
