/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace KFCConfigurationClasses::Plugins::SyslogConfigNS;

void SyslogClient::defaults()
{
    __CDBG_printf("SyslogClient::defaults handle=%04x", kConfigStructHandle);
    setConfig(SyslogClientConfig());
    setHostname(CREATE_ZERO_CONF(F("syslog"), FSPGM(tcp), FSPGM(address)));
    System::Flags::getWriteableConfig().is_syslog_enabled = false;
    __CDBG_dump(SyslogClient, cfg);
    __CDBG_dumpString(Hostname);
}

bool SyslogClient::isEnabled()
{
    return System::Flags::getConfig().is_syslog_enabled && isEnabled(getConfig().protocol);
}

bool SyslogClient::isEnabled(SyslogProtocolType protocol)
{
    switch (protocol)
    {
        case SyslogProtocolType::UDP:
        case SyslogProtocolType::TCP:
        case SyslogProtocolType::TCP_TLS:
            return true;
        default:
            return false;
    }
}
