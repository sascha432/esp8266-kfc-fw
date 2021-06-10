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

    bool Plugins::NTPClient::isEnabled()
    {
        return System::Flags::getConfig().is_ntp_client_enabled;
    }

    static char *trim(const char *hostname, bool alloc)
    {
        char *server = const_cast<char *>(hostname);
        while (isspace(*server) && *server) {
            server++;
        }
        if (!*server) {
            return nullptr;
        }
        auto end = server + strlen(server) - 1;
        while (isspace(*end) && end >= server) {
            end--;
        }
        if (!*server) {
            return nullptr;
        }
        if (alloc) {
            auto size = end - hostname + 2;
            __DBG_printf("getserver %s %p:%p=%u", hostname, server, end, size);
            auto ptr = reinterpret_cast<char *>(malloc(size));
            if (!ptr) {
                return nullptr;
            }
            server = strncpy(ptr, server, size - 1);
            server[size] = 0;
            __DBG_printf("newptr %s", server);
        }
        else {
            *end = 0;
        }
        return server;
    }

    char *Plugins::NTPClient::getServer(uint8_t num, bool alloc) {
        switch(num) {
            case 0:
                return trim(getServer1(), alloc);
            #if SNTP_MAX_SERVERS > 1
            case 1:
                return trim(getServer2(), alloc);
            #endif
            #if SNTP_MAX_SERVERS > 2
            case 2:
                return trim(getServer3(), alloc);
            #endif
            #if SNTP_MAX_SERVERS > 3
            case 3:
                return trim(getServer4(), alloc);
            #endif
            default:
                return nullptr;
        }
    }
}
