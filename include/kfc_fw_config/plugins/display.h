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

                    // backlight level in percent
                    CREATE_UINT32_BITFIELD_MIN_MAX(backlight_level, 7, 0, 100, 100);
                    // backlight level in percent while the power saving mode is active (idle dimming).
                    // 0 is not a valid value - the stored blob of an existing device is zero filled
                    // when it grows to the new length, so 0 means "not initialized" and the default
                    // is used (a standby level that turns the display off completely is not useful)
                    CREATE_UINT32_BITFIELD_MIN_MAX(power_saving_level, 7, 1, 100, 30);
                    // seconds without user activity before the backlight is dimmed
                    CREATE_UINT32_BITFIELD_MIN_MAX(power_saving_timeout, 17, 10, 86400, 300, 10);
                    // seconds without user activity before the display is turned off (standby)
                    CREATE_UINT32_BITFIELD_MIN_MAX(standby_timeout, 17, 60, 86400, 3600, 10);

                    Config_t() :
                        backlight_level(kDefaultValueFor_backlight_level),
                        power_saving_level(kDefaultValueFor_power_saving_level),
                        power_saving_timeout(kDefaultValueFor_power_saving_timeout),
                        standby_timeout(kDefaultValueFor_standby_timeout)
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
