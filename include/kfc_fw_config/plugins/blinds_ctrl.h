/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // BlindsController

        #ifndef BLINDS_CONFIG_MAX_OPERATIONS
        #define BLINDS_CONFIG_MAX_OPERATIONS                        6
        #endif

        class BlindsConfig {
        public:
            static constexpr size_t kChannel0_OpenPinArrayIndex = 0;
            static constexpr size_t kChannel0_ClosePinArrayIndex = 1;
            static constexpr size_t kChannel1_OpenPinArrayIndex = 2;
            static constexpr size_t kChannel1_ClosePinArrayIndex = 3;
            static constexpr size_t kMultiplexerPinArrayIndex = 4;
            static constexpr size_t kDACPinArrayIndex = 5;

            static constexpr size_t kMaxOperations = BLINDS_CONFIG_MAX_OPERATIONS;

            enum class OperationType : uint32_t {
                NONE = 0,
                OPEN_CHANNEL0,                    // _FOR_CHANNEL0_AND_ALL
                OPEN_CHANNEL0_FOR_CHANNEL1,       // _ONLY
                OPEN_CHANNEL1,
                OPEN_CHANNEL1_FOR_CHANNEL0,
                CLOSE_CHANNEL0,
                CLOSE_CHANNEL0_FOR_CHANNEL1,
                CLOSE_CHANNEL1,
                CLOSE_CHANNEL1_FOR_CHANNEL0,
                DO_NOTHING,
                DO_NOTHING_CHANNEL0,
                DO_NOTHING_CHANNEL1,
                MAX
            };

            enum class PlayToneType : uint32_t {
                NONE = 0,
                INTERVAL,
                IMPERIAL_MARCH,
                MAX
            };

            typedef struct __attribute__packed__ BlindsConfigOperation_t {
                using Type = BlindsConfigOperation_t;

                CREATE_UINT32_BITFIELD_MIN_MAX(delay, 20, 0, 900000, 0, 500);
                CREATE_UINT32_BITFIELD_MIN_MAX(relative_delay, 1, 0, 1, 0, 1);
                CREATE_ENUM_BITFIELD(play_tone, PlayToneType);
                CREATE_ENUM_BITFIELD(action, OperationType);

                // uint16_t delay;                                     // delay before execution in seconds
                // OperationType type;                                 // action

                BlindsConfigOperation_t() :
                    delay(kDefaultValueFor_delay),
                    relative_delay(0),
                    play_tone(BlindsConfigOperation_t::cast_int_play_tone(PlayToneType::NONE)),
                    action(BlindsConfigOperation_t::cast_int_action(OperationType::NONE))
                {}

             } BlindsConfigOperation_t;

            typedef struct __attribute__packed__ BlindsConfigChannel_t {
                using Type = BlindsConfigChannel_t;
                CREATE_UINT32_BITFIELD_MIN_MAX(current_limit_mA, 12, 1, 4095, 100, 10);                     // bits 00:11 ofs:len 000:12 0-0x0fff (4095)
                CREATE_UINT32_BITFIELD_MIN_MAX(dac_pwm_value, 10, 0, 1023, 512);                            // bits 12:21 ofs:len 012:10 0-0x03ff (1023)
                CREATE_UINT32_BITFIELD_MIN_MAX(pwm_value, 10, 0, 1023, 256);                                // bits 22:31 ofs:len 022:10 0-0x03ff (1023)
                CREATE_UINT32_BITFIELD_MIN_MAX(current_avg_period_us, 16, 100, 50000, 2500, 100);           // bits 00:15 ofs:len 032:16 0-0xffff (65535)
                CREATE_UINT32_BITFIELD_MIN_MAX(open_time_ms, 16, 0, 60000, 5000, 250);                      // bits 16:31 ofs:len 048:16 0-0xffff (65535)
                CREATE_UINT16_BITFIELD_MIN_MAX(close_time_ms, 16, 0, 60000, 5000, 250);                     // bits 00:15 ofs:len 064:16 0-0xffff (65535)
                BlindsConfigChannel_t();

            } BlindsConfigChannel_t;

            typedef struct __attribute__packed__ BlindsConfig_t {
                using Type = BlindsConfig_t;

                BlindsConfigChannel_t channels[2];
                BlindsConfigOperation_t open[kMaxOperations];
                BlindsConfigOperation_t close[kMaxOperations];
                uint8_t pins[6];
                CREATE_UINT32_BITFIELD_MIN_MAX(pwm_frequency, 16, 1000, 40000, 30000, 1000);
                CREATE_UINT32_BITFIELD_MIN_MAX(adc_recovery_time, 16, 1000, 65000, 12500, 500);
                CREATE_UINT32_BITFIELD_MIN_MAX(adc_read_interval, 12, 250, 4000, 750, 100);
                CREATE_UINT32_BITFIELD_MIN_MAX(adc_recoveries_per_second, 3, 1, 7, 4);
                CREATE_UINT32_BITFIELD_MIN_MAX(adc_multiplexer, 1, 0, 1, 0);
                CREATE_INT32_BITFIELD_MIN_MAX(adc_offset, 11, -1000, 1000, 0);
                CREATE_UINT32_BITFIELD_MIN_MAX(pwm_softstart_time, 10, 0, 1000, 300, 10);
                CREATE_UINT32_BITFIELD_MIN_MAX(play_tone_channel, 3, 0, 2, 0, 0);
                CREATE_UINT32_BITFIELD_MIN_MAX(tone_frequency, 11, 150, 2000, 800, 50);
                CREATE_UINT32_BITFIELD_MIN_MAX(tone_pwm_value, 10, 0, 1023, 150, 1);

                BlindsConfig_t();

            } SensorConfig_t;

            static constexpr size_t SensorConfig_t_Size = sizeof(SensorConfig_t);
            static constexpr size_t BlindsConfig_t_Size = sizeof(BlindsConfig_t);
            static constexpr size_t BlindsConfig_t_open_Size = sizeof(BlindsConfig_t().open);
            static constexpr size_t BlindsConfigChannel_t_Size = sizeof(BlindsConfigChannel_t);
            static constexpr size_t BlindsConfigOperation_t_Size = sizeof(BlindsConfigOperation_t);
        };

        class Blinds : public BlindsConfig, public KFCConfigurationClasses::ConfigGetterSetter<BlindsConfig::BlindsConfig_t, _H(MainConfig().plugins.blinds.cfg) CIF_DEBUG(, &handleNameBlindsConfig_t)> {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.blinds, Channel0Name, 2, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.blinds, Channel1Name, 2, 64);

            static const char *getChannelName(size_t num) {
                switch(num) {
                    case 0:
                        return getChannel0Name();
                    case 1:
                    default:
                        return getChannel1Name();
                }
            }
        };
