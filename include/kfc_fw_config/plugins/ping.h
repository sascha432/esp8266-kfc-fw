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

                uint16_t interval;                      // minutes
                uint16_t timeout;                       // ms
                CREATE_UINT8_BITFIELD(count, 6);        // number of pings
                CREATE_UINT8_BITFIELD(console, 1);      // ping console @ utilities
                CREATE_UINT8_BITFIELD(service, 1);      // ping monitor background service

                static constexpr uint8_t kRepeatCountMin = 1;
                static constexpr uint8_t kRepeatCountMax = (1 << kBitCountFor_count) - 1;
                static constexpr uint8_t kRepeatCountDefault = 4;
                static constexpr uint16_t kIntervalMin = 1;
                static constexpr uint16_t kIntervalMax = (24 * 60) * 30;
                static constexpr uint16_t kIntervalDefault = 5;
                static constexpr uint16_t kTimeoutMin = 100;
                static constexpr uint16_t kTimeoutMax = 60000;
                static constexpr uint16_t kTimeoutDefault = 5000;

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
