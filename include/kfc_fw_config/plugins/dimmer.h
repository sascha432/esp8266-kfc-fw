/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Dimmer

        namespace DimmerConfigNS {

            class DimmerConfig {
            public:
                struct __attribute__packed__ DimmerConfig_t {
                    using Type = DimmerConfig_t;
                    #if IOT_ATOMIC_SUN_V2
                        int8_t channel_mapping[IOT_DIMMER_MODULE_CHANNELS];
                    #endif
                    #if IOT_DIMMER_MODULE_CHANNELS
                        struct __attribute__packed__ {
                            uint16_t from[IOT_DIMMER_MODULE_CHANNELS];
                            uint16_t to[IOT_DIMMER_MODULE_CHANNELS];
                        } level;
                    #endif
                    #if IOT_DIMMER_MODULE_HAS_BUTTONS
                        CREATE_UINT16_BITFIELD_MIN_MAX(off_delay, 9, 0, 480, 0);
                        CREATE_UINT16_BITFIELD_MIN_MAX(off_delay_signal, 1, 0, 1, false);
                        CREATE_UINT16_BITFIELD_MIN_MAX(pin_ch0_down, 5, 0, 16, 13);
                        CREATE_UINT16_BITFIELD_MIN_MAX(pin_ch0_down_inverted, 1, 0, 1, false);
                        CREATE_UINT16_BITFIELD_MIN_MAX(pin_ch0_up, 5, 0, 16, 4);
                        CREATE_UINT16_BITFIELD_MIN_MAX(pin_ch0_up_inverted, 1, 0, 1, false);
                        CREATE_UINT16_BITFIELD_MIN_MAX(single_click_time, 14, 250, 16000, 750);
                        CREATE_UINT16_BITFIELD_MIN_MAX(longpress_time, 12, 0, 4000, 1000);
                        CREATE_UINT16_BITFIELD_MIN_MAX(min_brightness, 7, 0, 95, 15);
                        CREATE_UINT16_BITFIELD_MIN_MAX(shortpress_steps, 7, 4, 100, 15);
                        CREATE_UINT16_BITFIELD_MIN_MAX(longpress_max_brightness, 7, 0, 100, 80);
                        CREATE_UINT16_BITFIELD_MIN_MAX(longpress_min_brightness, 7, 0, 99, 33);
                        CREATE_UINT16_BITFIELD_MIN_MAX(max_brightness, 7, 5, 100, 100);
                        CREATE_UINT16_BITFIELD_MIN_MAX(shortpress_time, 10, 100, 1000, 275);
                        CREATE_FLOAT_FIELD(lp_fadetime, 1.0, 300.0, 5.0);

                        uint8_t pin(uint8_t idx) const {
                            return pin(idx / 2, idx % 2);
                        }
                        uint8_t pin(uint8_t channel, uint8_t button) const {
                            switch((channel << 4) | button) {
                                case 0x00: return pin_ch0_up;
                                case 0x01: return pin_ch0_down;
                            }
                            return 0xff;
                        }
                        uint8_t pin_inverted(uint8_t idx) const {
                            return pin_inverted(idx / 2, idx % 2);
                        }
                        bool pin_inverted(uint8_t channel, uint8_t button) const {
                            switch((channel << 4) | button) {
                                case 0x00: return pin_ch0_up_inverted;
                                case 0x01: return pin_ch0_down_inverted;
                            }
                            return 0xff;
                        }
                    #else
                        CREATE_UINT16_BITFIELD_MIN_MAX(min_brightness, 7, 0, 95, 1);
                        CREATE_UINT16_BITFIELD_MIN_MAX(max_brightness, 7, 5, 100, 100);
                    #endif
                    #if IOT_ATOMIC_SUN_V2
                        CREATE_FLOAT_FIELD(on_fadetime, 1.0, 300.0, 12.5);
                        CREATE_FLOAT_FIELD(off_fadetime, 1.0, 300.0, 12.5);
                        float _fadetime() const {
                            return on_fadetime;
                        }
                    #else
                        CREATE_FLOAT_FIELD(on_fadetime, 1.0, 300.0, 7.5);
                        CREATE_FLOAT_FIELD(off_fadetime, 1.0, 300.0, 7.5);
                        float _fadetime() const {
                            return lp_fadetime;
                        }
                    #endif

                    DimmerConfig_t();

                };
                static constexpr size_t kDimmerConfigSize = sizeof(DimmerConfig_t);
            };

            class Dimmer : public DimmerConfig, public KFCConfigurationClasses::ConfigGetterSetter<DimmerConfig::DimmerConfig_t, _H(MainConfig().plugins.dimmer.cfg) CIF_DEBUG(, &handleNameDimmerConfig_t)>
            {
            public:
                static void defaults();

                #if IOT_DIMMER_MODULE_CHANNELS >= 1
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel1Name, 0, 64);
                #endif
                #if IOT_DIMMER_MODULE_CHANNELS >= 2
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel2Name, 0, 64);
                #endif
                #if IOT_DIMMER_MODULE_CHANNELS >= 3
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel3Name, 0, 64);
                #endif
                #if IOT_DIMMER_MODULE_CHANNELS >= 4
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel4Name, 0, 64);
                #endif
                #if IOT_DIMMER_MODULE_CHANNELS >= 5
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel5Name, 0, 64);
                #endif
                #if IOT_DIMMER_MODULE_CHANNELS >= 6
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel6Name, 0, 64);
                #endif
                #if IOT_DIMMER_MODULE_CHANNELS >= 7
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel7Name, 0, 64);
                #endif
                #if IOT_DIMMER_MODULE_CHANNELS >= 8
                    CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.dimmer, Channel8Name, 0, 64);
                #endif

                static const char *getChannelName(uint8_t channel) {
                    switch(channel) {
                        #if IOT_DIMMER_MODULE_CHANNELS >= 1
                            case 0:
                                return getChannel1Name();
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 2
                            case 1:
                                return getChannel2Name();
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 3
                            case 2:
                                return getChannel3Name();
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 4
                            case 3:
                                return getChannel4Name();
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 5
                            case 4:
                                return getChannel5Name();
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 6
                            case 5:
                                return getChannel6Name();
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 7
                            case 6:
                                return getChannel7Name();
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 8
                            case 7:
                                return getChannel8Name();
                        #endif
                    }
                    return nullptr;
                }

                static void setChannelName(uint8_t channel, const String &name) {
                    switch(channel) {
                        #if IOT_DIMMER_MODULE_CHANNELS >= 1
                            case 0:
                                setChannel1Name(name);
                                break;
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 2
                            case 1:
                                setChannel2Name(name);
                                break;
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 3
                            case 2:
                                setChannel3Name(name);
                                break;
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 4
                            case 3:
                                setChannel4Name(name);
                                break;
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 5
                            case 4:
                                setChannel5Name(name);
                                break;
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 6
                            case 5:
                                setChannel6Name(name);
                                break;
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 7
                            case 6:
                                setChannel7Name(name);
                                break;
                        #endif
                        #if IOT_DIMMER_MODULE_CHANNELS >= 8
                            case 7:
                                setChannel8Name(name);
                                break;
                        #endif
                    }
                }
            };

        }
    }
}
