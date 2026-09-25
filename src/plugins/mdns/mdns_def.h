/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// shared types of the mDNS resolver
// they do not require the framework mDNS header, so this file is safe to include
// from headers that are used by libraries as well (e.g. kfc_fw_config.h)

#include <Arduino_compat.h>
#include <functional>

namespace MDNSResolver {

    enum class ResponseType {
        NONE = 0,
        TIMEOUT,
        RESOLVED,
    };

    using ResolvedCallback = std::function<void(const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, ResponseType type)>;

}
