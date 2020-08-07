/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::Ping::defaults()
    {
        PingConfig_t cfg = {};
        setConfig(cfg);
        setHost1(F("${gateway}"));
        setHost2(F("8.8.8.8"));
        setHost3(F("www.google.com"));
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
            default:
                return nullptr;
        }
    }
}
