/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Display

        namespace DisplayConfigNS {

            class DisplayConfig {
            public:
                struct __attribute__packed__ Config_t {
                    using Type = Config_t;

                    CREATE_UINT32_BITFIELD_MIN_MAX(backlight_level, 8, 0, 255, 255);

                    Config_t() :
                        backlight_level(kDefaultValueFor_backlight_level)
                    {
                    }
                };
            };

            class Display : public DisplayConfig, public KFCConfigurationClasses::ConfigGetterSetter<DisplayConfig::Config_t, _H(MainConfig().plugins.display.cfg) CIF_DEBUG(, &handleNameDisplayConfig_t)> {
            public:
                static void defaults();
            };

        }

    }

}
