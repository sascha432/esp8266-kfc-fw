/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::PingConfig::PingConfig_t::PingConfig_t() : interval(kIntervalDefault), timeout(kTimeoutDefault), count(4), console(true), service(false)
    {
    }

    void Plugins::Ping::defaults()
    {
        PingConfig_t cfg = {};
        setConfig(cfg);
        setHost1(FSPGM(_var_gateway));
        setHost2(FSPGM(ip_8_8_8_8, "8.8.8.8"));
        setHost3(FSPGM(www_google_com, "www.google.com"));
    }

    void Plugins::Ping::setHost(uint8_t num, const char *hostname)
    {
        switch(num) {
            case 0:
                setHost1(hostname);
                break;
            case 1:
                setHost2(hostname);
                break;
            case 2:
                setHost3(hostname);
                break;
            case 3:
                setHost4(hostname);
                break;
            case 4:
                setHost5(hostname);
                break;
            case 5:
                setHost6(hostname);
                break;
            case 6:
                setHost7(hostname);
                break;
            case 7:
                setHost8(hostname);
                break;
        }
    }

    const char *Plugins::Ping::getHost(uint8_t num) {
        switch(num) {
            case 0:
                return getHost1();
            case 1:
                return getHost2();
            case 2:
                return getHost3();
            case 3:
                return getHost4();
            case 4:
                return getHost5();
            case 5:
                return getHost6();
            case 6:
                return getHost7();
            case 7:
                return getHost8();
            default:
                return nullptr;
        }
    }

    uint8_t Plugins::Ping::getHostCount()
    {
        uint8_t count = 0;
        for(uint8_t i = 0; i < kHostsMax; i++) {
            auto hostname = getHost(i);
            while(isspace(*hostname)) {
                hostname++;
            }
            if (*hostname) {
                count++;
            }
        }
        return count;
    }

    void Plugins::Ping::removeEmptyHosts()
    {
        for(uint8_t i = 0; i < kHostsMax; i++) {
            String host = getHost(i);
            auto length = host.length();
            host.trim();
            if (host.length() == 0) { // empty, find a replacement
                for(uint8_t j = i + 1; j < kHostsMax; j++) {
                    String host2 = getHost(j);
                    host2.trim();
                    if (host2.length() != 0) {
                        setHost(i, host2.c_str());
                        setHost(j, emptyString.c_str());
                        break;
                    }
                }
            }
            else if (length != host.length()) { // has been trimmed, update host
                setHost(i, host.c_str());
            }
        }
    }

}
