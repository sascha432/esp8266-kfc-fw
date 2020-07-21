/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::IOTSwitch::setConfig(const uint8_t *buf, size_t size)
    {
        config.setBinary(_H(MainConfig().plugins.iotswitch), buf, size);
    }

    const uint8_t *Plugins::IOTSwitch::getConfig()
    {
        uint16_t length;
        return config.getBinary(_H(MainConfig().plugins.iotswitch), length);
    }

}
