/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    System::WebServer::WebServerConfig_t::WebServerConfig_t() :
        is_https(false),
#if SECURITY_LOGIN_ATTEMPTS
        login_attempts(kDefaultValueFor_login_attempts),
        login_timeframe(kDefaultValueFor_login_timeframe),
        login_rewrite_interval(kDefaultValueFor_login_rewrite_interval),
        login_storage_timeframe(kDefaultValueFor_login_storage_timeframe),
#endif
        __port(kPortAuto)
    {
    }

}
