/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// shared types of the mDNS resolver
// they do not require the framework mDNS header, so this file is safe to include
// from headers that are used by libraries as well (e.g. kfc_fw_config.h)

#include <Arduino_compat.h>
#include <functional>

// ESP8266 discovery (mdns_query.cpp): it is only started with MDNS_DISCOVERY_MIN_FREE_HEAP free heap, and an
// answer that arrives with less than MDNS_DISCOVERY_STOP_FREE_HEAP free is dropped. Every answer adds ~150 byte
// to the response
#ifndef MDNS_DISCOVERY_MIN_FREE_HEAP
#    define MDNS_DISCOVERY_MIN_FREE_HEAP 8192
#endif

#ifndef MDNS_DISCOVERY_STOP_FREE_HEAP
#    define MDNS_DISCOVERY_STOP_FREE_HEAP 4096
#endif

namespace MDNSResolver {

    enum class ResponseType {
        NONE = 0,
        TIMEOUT,
        RESOLVED,
    };

    using ResolvedCallback = std::function<void(const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, ResponseType type)>;

}
