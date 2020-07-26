/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::SyslogClient::defaults()
    {
        __CDBG_printf("Plugins::SyslogClient::defaults handle=%04x", kConfigStructHandle);
        SyslogConfig_t cfg = {};
        setConfig(cfg);
        System::Flags::getWriteable().syslogEnabled = false;
        __CDBG_dump(SyslogClient, cfg);
        __CDBG_dumpString(Hostname);
    }

    bool Plugins::SyslogClient::isEnabled()
    {
        return System::Flags::get().mqttEnabled && isEnabled(getConfig().protocol_enum);
    }

    bool Plugins::SyslogClient::isEnabled(SyslogProtocolType protocol)
    {
        return (protocol == SyslogClient::SyslogProtocolType::UDP || protocol == SyslogClient::SyslogProtocolType::TCP || protocol == SyslogClient::SyslogProtocolType::TCP_TLS);
    }

}
