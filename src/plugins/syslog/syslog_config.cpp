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

namespace KFCConfigurationClasses {

    void Plugins::SyslogClient::defaults()
    {
        __CDBG_printf("Plugins::SyslogClient::defaults handle=%04x", kConfigStructHandle);
        SyslogConfig_t cfg = {};
        setConfig(cfg);
        System::Flags::getWriteable().is_syslog_enabled = false;
        __CDBG_dump(SyslogClient, cfg);
        __CDBG_dumpString(Hostname);
    }

    bool Plugins::SyslogClient::isEnabled()
    {
        return System::Flags::get().is_syslog_enabled && isEnabled(getConfig().protocol_enum);
    }

    bool Plugins::SyslogClient::isEnabled(SyslogProtocolType protocol)
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

}
