/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // NTP CLient

        namespace NTPClientConfigNs {

            struct __attribute__packed__ NTPClientConfig {
                using Type = NTPClientConfig;

                CREATE_UINT16_BITFIELD_MIN_MAX(refreshInterval, 16, 5, 720 * 60, 1440, 5);
                uint32_t getRefreshIntervalMillis() const {
                    return refreshInterval * 60U * 1000U;
                }

                NTPClientConfig() : refreshInterval(kDefaultValueFor_refreshInterval) {}

            };

            class NTPClient : public KFCConfigurationClasses::ConfigGetterSetter<NTPClientConfig, _H(MainConfig().plugins.ntpclient.cfg) CIF_DEBUG(, &handleNameNtpClientConfig_t)>
            {
            public:
                static void defaults();
                static bool isEnabled();

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server1, 0, 64);
                #if SNTP_MAX_SERVERS > 1
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server2, 0, 64);
                #endif
                #if SNTP_MAX_SERVERS > 2
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server3, 0, 64);
                #endif
                #if SNTP_MAX_SERVERS > 3
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server4, 0, 64);
                #endif
                CREATE_STRING_GETTER_SETTER(MainConfig().plugins.ntpclient, TimezoneName, 64);
                CREATE_STRING_GETTER_SETTER(MainConfig().plugins.ntpclient, PosixTimezone, 64);

                static String getServer(uint8_t num);
                static constexpr uint8_t kServersMax = SNTP_MAX_SERVERS;
                static_assert(kServersMax <= 4, "limited to 4 servers");

            };

        }
    }
}
