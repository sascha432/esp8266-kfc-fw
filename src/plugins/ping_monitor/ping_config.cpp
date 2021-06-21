/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::PingConfig::PingConfig_t::PingConfig_t() :
        interval(kDefaultValueFor_interval),
        timeout(kDefaultValueFor_timeout),
        count(kDefaultValueFor_count),
        console(kDefaultValueFor_console),
        service(kDefaultValueFor_service)
    {
    }

    void Plugins::Ping::defaults()
    {
        setConfig(PingConfig_t());
        auto hosts = getHosts();
        hosts.clear();
        hosts.append(FSPGM(_var_gateway));
        hosts.append(FSPGM(ip_8_8_8_8, "8.8.8.8"));
        hosts.append(FSPGM(www_google_com, "www.google.com"));
    }
}
