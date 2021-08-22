/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

using NTPClient = KFCConfigurationClasses::Plugins::NTPClientConfigNs::NTPClient;
using NTPClientConfig = NTPClient::ConfigStructType;

void NTPClient::defaults()
{
    setConfig(NTPClientConfig());
    setServer1(F("pool.ntp.org"));
    setServer2(F("time.nist.gov"));
    setServer3(F("time.windows.com"));
    setTimezoneName(F("Etc/Universal"));
    setPosixTimezone(F("UTC0"));
}

bool NTPClient::isEnabled()
{
    return System::Flags::getConfig().is_ntp_client_enabled;
}

String NTPClient::getServer(uint8_t num)
{
    switch(num) {
        case 0:
            return String(getServer1()).trim();
        #if SNTP_MAX_SERVERS > 1
        case 1:
            return String(getServer2()).trim();
        #endif
        #if SNTP_MAX_SERVERS > 2
        case 2:
            return String(getServer3()).trim();
        #endif
        #if SNTP_MAX_SERVERS > 3
        case 3:
            return String(getServer4()).trim();
        #endif
        default:
            __DBG_printf_E("invalid server num=%u", num);
            return String();
    }
}
