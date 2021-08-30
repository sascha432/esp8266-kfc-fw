/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

using namespace KFCConfigurationClasses::Plugins::SwitchConfigNS;

void IotSwitch::setConfig(const uint8_t *buf, size_t size)
{
    config.setBinary(_H(MainConfig().plugins.iotswitch), buf, size);
}

const uint8_t *IotSwitch::getConfig(uint16_t &length)
{
    return config.getBinary(_H(MainConfig().plugins.iotswitch), length);
}
