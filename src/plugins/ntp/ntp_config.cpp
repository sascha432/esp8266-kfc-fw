/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::NTPClient::defaults()
    {
        NtpClientConfig_t cfg = {};
        setConfig(cfg);
        setServer1(F("pool.ntp.org"));
        setServer2(F("time.nist.gov"));
        setServer3(F("time.windows.com"));
        setTimezoneName(F("Etc/Universal"));
        setPosixTimezone(F("UTC0"));
    }

    bool Plugins::NTPClient::isEnabled() {
        return System::Flags::get().is_ntp_client_enabled;
    }

    const char *Plugins::NTPClient::getServer(uint8_t num) {
        switch(num) {
            case 0:
                return getServer1();
            case 1:
                return getServer2();
            case 2:
                return getServer3();
            default:
                return nullptr;
        }
    }
}
