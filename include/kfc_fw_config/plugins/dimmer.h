/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"



        // --------------------------------------------------------------------
        // Dimmer

#if !IOT_DIMMER_MODULE && !IOT_ATOMIC_SUN_V2
        typedef struct {
        } register_mem_cfg_t;
        typedef struct  {
        } dimmer_version_t;
#endif

        class DimmerConfig {
        public:
            typedef struct __attribute__packed__ DimmerConfig_t {
                using Type = DimmerConfig_t;
                dimmer_version_t version;
                register_mem_cfg_t cfg;
            #if IOT_ATOMIC_SUN_V2
                int8_t channel_mapping[IOT_DIMMER_MODULE_CHANNELS];
            #endif
            #if IOT_DIMMER_MODULE_CHANNELS
                struct __attribute__packed__ {
                    uint16_t from[IOT_DIMMER_MODULE_CHANNELS];
                    uint16_t to[IOT_DIMMER_MODULE_CHANNELS];
                } level;
            #endif
                float on_fadetime;
                float off_fadetime;
                float lp_fadetime;
            #if IOT_DIMMER_MODULE_HAS_BUTTONS
                /*CREATE_UINT32_BITFIELD_MIN_MAX(config_valid, 1, 0, 1, false);*/                           // bits 00:00 ofs:len 000:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(off_delay, 9, 0, 480, 0);                                    // bits 01:09 ofs:len 001:09 0-0x01ff (511)
                CREATE_UINT32_BITFIELD_MIN_MAX(off_delay_signal, 1, 0, 1, false);                           // bits 10:10 ofs:len 010:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_down_inverted, 1, 0, 1, false);                      // bits 11:11 ofs:len 011:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_up, 5, 0, 16, 4);                                    // bits 12:16 ofs:len 012:05 0-0x001f (31)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_up_inverted, 1, 0, 1, false);                        // bits 17:17 ofs:len 017:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(single_click_time, 14, 250, 16000, 750);                     // bits 18:31 ofs:len 018:14 0-0x3fff (16383)
                CREATE_UINT32_BITFIELD_MIN_MAX(longpress_time, 12, 0, 4000, 1000);                        // bits 00:11 ofs:len 032:12 0-0x0fff (4095)
                CREATE_UINT32_BITFIELD_MIN_MAX(min_brightness, 7, 0, 95, 15);                               // bits 12:18 ofs:len 044:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_down, 5, 0, 16, 13);                                 // bits 19:23 ofs:len 051:05 0-0x001f (31)
                CREATE_UINT32_BITFIELD_MIN_MAX(shortpress_steps, 7, 4, 100, 15);                             // bits 24:30 ofs:len 056:07 0-0x007f (127)
                uint32_t __free2: 1;                                                                        // bits 31:31 ofs:len 063:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(longpress_max_brightness, 7, 0, 100, 80);                   // bits 00:06 ofs:len 064:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(longpress_min_brightness, 7, 0, 99, 33);                     // bits 07:13 ofs:len 071:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(max_brightness, 7, 5, 100, 100);                            // bits 14:20 ofs:len 078:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(shortpress_time, 10, 100, 1000, 275);                        // bits 21:30 ofs:len 085:10 0-0x03ff (1023)
                uint32_t __free3: 1;                                                                        // bits 31:31 ofs:len 095:01 0-0x0001 (1)

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
                CREATE_BOOL_BITFIELD(config_valid);
            #endif

                DimmerConfig_t();

            } DimmerConfig_t;
        };

        class Dimmer : public DimmerConfig, public KFCConfigurationClasses::ConfigGetterSetter<DimmerConfig::DimmerConfig_t, _H(MainConfig().plugins.dimmer.cfg) CIF_DEBUG(, &handleNameDimmerConfig_t)>
        {
        public:
            static void defaults();
        };
