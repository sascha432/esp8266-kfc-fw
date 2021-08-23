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
    const char *server = nullptr;
    switch(num) {
        case 0:
            server = getServer1();
            break;
        #if SNTP_MAX_SERVERS > 1
        case 1:
            server = getServer2();
            break;
        #endif
        #if SNTP_MAX_SERVERS > 2
        case 2:
            server = getServer3();
            break;
        #endif
        #if SNTP_MAX_SERVERS > 3
        case 3:
            server = getServer4();
            break;
        #endif
        default:
            __DBG_printf_E("invalid server num=%u", num);
            break;
    }
    if (!server) {
        return String();
    }
    return String(server).trim();
}
