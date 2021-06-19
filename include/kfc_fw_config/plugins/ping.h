/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // Ping

        class PingConfig {
        public:
            typedef struct __attribute__packed__ PingConfig_t {
                using Type = PingConfig_t;

                CREATE_UINT16_BITFIELD_MIN_MAX(interval, 16, 1, (24 * 60) * 30, 5);
                CREATE_UINT16_BITFIELD_MIN_MAX(timeout, 16, 100, 60000, 5000);
                CREATE_UINT8_BITFIELD_MIN_MAX(count, 6, 0, 63, 4);
                CREATE_UINT8_BITFIELD_MIN_MAX(console, 1, 0, 1, true);
                CREATE_UINT8_BITFIELD_MIN_MAX(service, 1, 0, 1, false);

                PingConfig_t();

            } PingConfig_t;
        };

        class Ping : public PingConfig, public KFCConfigurationClasses::ConfigGetterSetter<PingConfig::PingConfig_t, _H(MainConfig().plugins.ping.cfg) CIF_DEBUG(, &handleNamePingConfig_t)>
        {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host1, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host2, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host3, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host4, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host5, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host6, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host7, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host8, 0, 64);

            static void setHost(uint8_t num, const char *);
            static const char *getHost(uint8_t num);
            static void removeEmptyHosts(); // trim hostnames and move empty hostnames to the end of the list
            static uint8_t getHostCount();
            static constexpr uint8_t kHostsMax = 8;

        };
