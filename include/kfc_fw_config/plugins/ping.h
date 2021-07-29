/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        namespace PingConfigNS {

            // --------------------------------------------------------------------
            // Ping

            struct __attribute__packed__ PingConfig {
                using Type = PingConfig;

                CREATE_UINT16_BITFIELD_MIN_MAX(interval, 16, 1, (24 * 60) * 30, 5);
                CREATE_UINT16_BITFIELD_MIN_MAX(timeout, 16, 100, 60000, 5000);
                CREATE_UINT8_BITFIELD_MIN_MAX(count, 6, 0, 63, 4);
                CREATE_UINT8_BITFIELD_MIN_MAX(console, 1, 0, 1, true);
                CREATE_UINT8_BITFIELD_MIN_MAX(service, 1, 0, 1, false);

                PingConfig();
            };

            class Ping : public KFCConfigurationClasses::ConfigGetterSetter<PingConfig, _H(MainConfig().plugins.ping.cfg) CIF_DEBUG(, &handleNamePingConfig_t)>
            {
            public:
                static void defaults();

                static constexpr uint8_t kHostsMax = 8;

                using HostnameType = KFCConfigurationClasses::ConfigStringArray<_H(MainConfig().plugins.ping), kHostsMax, 64>;

                static HostnameType getHosts() {
                    return HostnameType();
                }
            };

        }
    }
}
