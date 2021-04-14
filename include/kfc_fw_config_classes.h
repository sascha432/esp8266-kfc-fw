/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "kfc_fw_config/base.h"

// --------------------------------------------------------------------
// System
#include "kfc_fw_config/system.h"

// --------------------------------------------------------------------
// Network
#include "kfc_fw_config/network.h"

// --------------------------------------------------------------------
// Plugins
#include "kfc_fw_config/plugins.h"

namespace KFCConfigurationClasses {

    // --------------------------------------------------------------------
    // Main Config Structure
    struct MainConfig {
        System system;
        Network network;
        Plugins plugins;
    };

};

static constexpr size_t ConfigFlagsSize = sizeof(KFCConfigurationClasses::System::Flags::ConfigFlags_t);
static_assert(ConfigFlagsSize == sizeof(uint32_t), "size exceeded");

#include <pop_pack.h>

// #ifdef _H_DEFINED_KFCCONFIGURATIONCLASSES
// #undef CONFIG_GET_HANDLE_STR
// #undef _H
// #undef _H_DEFINED_KFCCONFIGURATIONCLASSES
// #endif
