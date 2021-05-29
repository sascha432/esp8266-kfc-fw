/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // Syslog

        class SyslogClientConfig {
        public:
            using SyslogProtocolType = ::SyslogProtocolType;

            AUTO_DEFAULT_PORT_CONST_SECURE(514, 6514);

            typedef struct __attribute__packed__ SyslogConfig_t {
                union __attribute__packed__ {
                    SyslogProtocolType protocol_enum;
                    uint8_t protocol;
                };
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, kPortDefault, kPortDefaultSecure, protocol_enum == SyslogProtocolType::TCP_TLS);

#if 0
                template<class T>
                void dump() {
                    CONFIG_DUMP_STRUCT_INFO(T);
                    CONFIG_DUMP_STRUCT_VAR(protocol);
                    CONFIG_DUMP_STRUCT_VAR(__port);
                }
#endif

                SyslogConfig_t() : protocol_enum(SyslogProtocolType::TCP), __port(kDefaultValueFor___port) {}

            } SyslogConfig_t;
        };

        class SyslogClient : public SyslogClientConfig, public KFCConfigurationClasses::ConfigGetterSetter<SyslogClientConfig::SyslogConfig_t, _H(MainConfig().plugins.syslog.cfg) CIF_DEBUG(, &handleNameSyslogConfig_t)>
        {
        public:
            static void defaults();
            static bool isEnabled();
            static bool isEnabled(SyslogProtocolType protocol);

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.syslog, Hostname, 1, 128);
        };
