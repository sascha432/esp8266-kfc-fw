/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include <ConfigurationHelper.h>

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Syslog

        namespace SyslogConfigNS {

            struct __attribute__packed__ SyslogClientConfig {
                using Type =  SyslogClientConfig;

                AUTO_DEFAULT_PORT_CONST_SECURE(514, 6514);
                CREATE_ENUM_D_BITFIELD(protocol, SyslogProtocol, SyslogProtocol::UDP);
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, kPortDefault, kPortDefaultSecure, _get_enum_protocol() == SyslogProtocol::TCP_TLS);

                SyslogClientConfig() : protocol(kDefaultValueFor_protocol), __port(kDefaultValueFor___port) {}
            };

            class SyslogClient : public KFCConfigurationClasses::ConfigGetterSetter<SyslogClientConfig, _H(MainConfig().plugins.syslog.cfg) CIF_DEBUG(, &handleNameSyslogConfig_t)>
            {
            public:
                static void defaults();
                static bool isEnabled();
                static bool isEnabled(SyslogProtocol protocol);
                inline static bool isEnabled(int protocol) {
                    return isEnabled(static_cast<SyslogProtocol>(protocol));
                }

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.syslog, Hostname, 1, 128);
            };

        }
    }
}
