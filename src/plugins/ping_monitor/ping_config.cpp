/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::PingConfigNS::PingConfig::PingConfig() :
        interval(kDefaultValueFor_interval),
        timeout(kDefaultValueFor_timeout),
        count(kDefaultValueFor_count),
        console(kDefaultValueFor_console),
        service(kDefaultValueFor_service)
    {
    }

    void Plugins::PingConfigNS::Ping::defaults()
    {
        setConfig(PingConfig());
        auto hosts = getHosts();
        hosts.clear();
        hosts.append(FSPGM(_var_gateway));
        hosts.append(F("8.8.8.8"));
        hosts.append(F("www.google.com"));
    }
}
