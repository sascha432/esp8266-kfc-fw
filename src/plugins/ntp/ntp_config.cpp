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
        if (server == end) {
            return nullptr;
        }
        end++; // move to \0
        if (alloc) {
            // allocate memory for the trimmed hostname name
            auto size = end - server;
            auto ptr = new char[size + 1];
            if (!ptr) {
                return nullptr;
            }
            // copy trimmed hostname to server
            std::copy(server, end, ptr);
            server = ptr;
            server[size] = 0;
            // __DBG_printf("newptr '%s' %u", server, size);
        }
        else {
            // return trimmed hostname
            *end = 0;
            // __DBG_printf("newsvr '%s' %u", server, strlen(server));
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
