/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // Clock

#if IOT_CLOCK
        #include "../src/plugins/clock/clock_def.h"
#endif

        class ClockConfig {
        public:
            union __attribute__packed__ ClockColor_t {
                uint32_t value: 24;
                uint8_t bgr[3];
                struct __attribute__packed__ {
                    uint8_t blue;
                    uint8_t green;
                    uint8_t red;
                };
                ClockColor_t(uint32_t val = 0) : value(val) {}
                operator uint32_t() const {
                    return value;
                }
            };

            struct __attribute__packed__ RainbowMultiplier_t {
                float value;
                float min;
                float max;
                float incr;
                RainbowMultiplier_t();
                RainbowMultiplier_t(float value, float min, float max, float incr);
            };

            struct __attribute__packed__ RainbowColor_t {
                ClockColor_t min;
                ClockColor_t factor;
                float red_incr;
                float green_incr;
                float blue_incr;
                RainbowColor_t();
            };

            struct __attribute__packed__ FireAnimation_t {

                enum class Orientation : uint8_t {
                    MIN = 0,
                    VERTICAL = MIN,
                    HORIZONTAL,
                    MAX
                };

                using Type = FireAnimation_t;
                uint8_t cooling;
                uint8_t sparking;
                uint8_t speed;
                CREATE_ENUM_BITFIELD(orientation, Orientation);
                CREATE_BOOL_BITFIELD(invert_direction);
                ClockColor_t factor;

                Orientation getOrientation() const {
                    return get_enum_orientation(*this);
                }

                FireAnimation_t();

            };

            struct __attribute__packed__ VisualizerAnimation_t {
                using Type = VisualizerAnimation_t;
                enum class VisualizerType : uint8_t {
                    SINGLE_COLOR,
                    SINGLE_COLOR_DOUBLE_SIDED,
                    RAINBOW,
                    RAINBOW_DOUBLE_SIDED,
                    MAX,
                };

                CREATE_UINT32_BITFIELD_MIN_MAX(port, 16, 0, 65535, 4210);
                CREATE_ENUM_D_BITFIELD(type, VisualizerType, VisualizerType::RAINBOW);
                ClockColor_t color;

                VisualizerAnimation_t() :
                    port(kDefaultValueFor_port),
                    type(kDefaultValueFor_type),
                    color(0xff00ff)
                {
                }
            };

            struct __attribute__packed__ ClockConfig_t {
                using Type = ClockConfig_t;

                enum class AnimationType : uint8_t {
                    MIN = 0,
                    SOLID = 0,
                    RAINBOW,
                    FLASHING,
                    FADING,
                    FIRE,
#if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
                    VISUALIZER,
#endif
                    INTERLEAVED,
#if !IOT_LED_MATRIX
                    COLON_SOLID,
                    COLON_BLINK_FAST,
                    COLON_BLINK_SLOWLY,
#endif
                    MAX,
                };

                static const __FlashStringHelper *getAnimationNames();
                static const __FlashStringHelper *getAnimationNamesJsonArray();
                static const __FlashStringHelper *getAnimationName(AnimationType type);

                enum class InitialStateType : uint8_t  {
                    MIN = 0,
                    OFF = 0,
                    ON,
                    RESTORE,
                    MAX
                };

                ClockColor_t solid_color;
                CREATE_ENUM_BITFIELD(animation, AnimationType);
                CREATE_ENUM_BITFIELD(initial_state, InitialStateType);
                CREATE_UINT8_BITFIELD(time_format_24h, 1);
                CREATE_UINT8_BITFIELD(dithering, 1);
                CREATE_UINT8_BITFIELD(standby_led, 1);
                CREATE_UINT8_BITFIELD(enabled, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(fading_time, 6, 0, 60, 10, 1);
#if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
                CREATE_UINT32_BITFIELD_MIN_MAX(power_limit, 8, 0, 255, 0, 1);
#endif
                CREATE_UINT32_BITFIELD_MIN_MAX(brightness, 8, 0, 255, 255 / 4, 1);
                CREATE_INT32_BITFIELD_MIN_MAX(auto_brightness, 11, -1, 1023, -1, 1);
#if !IOT_LED_MATRIX
                CREATE_UINT32_BITFIELD_MIN_MAX(blink_colon_speed, 13, 50, 8000, 1000, 100);
#endif
                CREATE_UINT32_BITFIELD_MIN_MAX(flashing_speed, 13, 50, 8000, 150, 100);
#if IOT_CLOCK_HAVE_MOTION_SENSOR
                CREATE_UINT32_BITFIELD_MIN_MAX(motion_auto_off, 10, 0, 1000, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(motion_trigger_timeout, 8, 1, 240, 15, 1);
#endif
#if IOT_LED_MATRIX_FAN_CONTROL
                CREATE_UINT32_BITFIELD_MIN_MAX(fan_speed, 8, 0, 255, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(min_fan_speed, 8, 0, 255, 32, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(max_fan_speed, 8, 0, 255, 255, 1);
#endif

                static const uint16_t kPowerNumLeds = 256;

#if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
                struct __attribute__packed__ {
                    uint16_t red;
                    uint16_t green;
                    uint16_t blue;
                    uint16_t idle;
                } power;
#endif

                struct __attribute__packed__ {
                    struct __attribute__packed__ {
                        uint8_t min;
                        uint8_t max;
                    } temperature_reduce_range;
                    uint8_t max_temperature;
                    int8_t regulator_margin;
                } protection;

                struct __attribute__packed__ {
                    RainbowMultiplier_t multiplier;
                    RainbowColor_t color;
                    uint16_t speed;
                } rainbow;

                struct __attribute__packed__ {
                    ClockColor_t color;
                    uint16_t speed;
                } alarm;

                typedef struct __attribute__packed__ fading_t {
                    using Type = fading_t;
                    CREATE_UINT32_BITFIELD_MIN_MAX(speed, 17, 100, 100000, 1000, 100)
                    CREATE_UINT32_BITFIELD_MIN_MAX(delay, 12, 0, 3600, 3, 1)
                    ClockColor_t factor;
                } fading_t;
                fading_t fading;

                FireAnimation_t fire;

#if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
                VisualizerAnimation_t visualizer;
#endif

                struct __attribute__packed__ {
                    uint8_t rows;
                    uint8_t cols;
                    uint32_t time;
                } interleaved;

                ClockConfig_t();

                uint16_t getBrightness() const {
                    return brightness;
                }
                void setBrightness(uint8_t pBrightness) {
                    brightness = pBrightness;
                }

                uint32_t getFadingTimeMillis() const {
                    return fading_time * 1000;
                }
                void setFadingTimeMillis(uint32_t time) {
                    fading_time = time / 1000;
                }

                AnimationType getAnimation() const {
                    return get_enum_animation(*this);
                }
                void setAnimation(AnimationType animation) {
                    set_enum_animation(*this, animation);
                }
                bool hasColorSupport() const {
                    switch(getAnimation()) {
                        case AnimationType::FADING:
                        case AnimationType::SOLID:
                        case AnimationType::FLASHING:
                        case AnimationType::INTERLEAVED:
#if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
                        case AnimationType::VISUALIZER:
#endif
                            return true;
                        default:
                            break;
                    }
                    return false;
                }

                InitialStateType getInitialState() const {
                    return get_enum_initial_state(*this);
                }
                void setInitialState(InitialStateType state) {
                    set_enum_initial_state(*this, state);
                }

            };
        };

        class Clock : public ClockConfig, public KFCConfigurationClasses::ConfigGetterSetter<ClockConfig::ClockConfig_t, _H(MainConfig().plugins.clock.cfg) CIF_DEBUG(, &handleNameClockConfig_t)>
        {
        public:
            static void defaults();
        };

