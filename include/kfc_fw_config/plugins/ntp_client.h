/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"


        // --------------------------------------------------------------------
        // NTP CLient

        class NTPClientConfig {
        public:
            typedef struct __attribute__packed__ NtpClientConfig_t {
                using Type = NtpClientConfig_t;

                CREATE_UINT16_BITFIELD_MIN_MAX(refreshInterval, 16, 5, 720 * 60, 15, 5);
                uint32_t getRefreshIntervalMillis() const {
                    return refreshInterval * 60U * 1000U;
                }

                NtpClientConfig_t() : refreshInterval(kDefaultValueFor_refreshInterval) {}

            } NtpClientConfig_t;
        };

        class NTPClient : public NTPClientConfig, public KFCConfigurationClasses::ConfigGetterSetter<NTPClientConfig::NtpClientConfig_t, _H(MainConfig().plugins.ntpclient.cfg) CIF_DEBUG(, &handleNameNtpClientConfig_t)>
        {
        public:
            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server1, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server2, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server3, 0, 64);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.ntpclient, TimezoneName, 64);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.ntpclient, PosixTimezone, 64);

            static const char *getServer(uint8_t num);
            static constexpr uint8_t kServersMax = 3;

        };
