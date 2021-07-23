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
                using Type = RainbowMultiplier_t;
                CREATE_FLOAT_FIELD(value, 0.1, 100.0, 0);
                CREATE_FLOAT_FIELD(min, 0.1, 100.0, 0);
                CREATE_FLOAT_FIELD(max, 0.1, 100.0, 0);
                CREATE_FLOAT_FIELD(incr, 0, 0.1, 0);
                RainbowMultiplier_t(float _value = kDefaultValueFor_value, float _min = kDefaultValueFor_min, float _max = kDefaultValueFor_max, float _incr = kDefaultValueFor_incr) :
                    value(_value), min(_min), max(_max), incr(_incr)
                {
                }
            };

            struct __attribute__packed__ RainbowColor_t {
                using Type = RainbowColor_t;
                ClockColor_t min;
                ClockColor_t factor;
                CREATE_FLOAT_FIELD(red_incr, 0, 0.1, 0);
                CREATE_FLOAT_FIELD(green_incr, 0, 0.1, 0);
                CREATE_FLOAT_FIELD(blue_incr, 0, 0.1, 0);
                RainbowColor_t(uint32_t _min = 0, uint32_t _factor = 0xffffff, float _red_incr = kDefaultValueFor_red_incr, float _green_incr = kDefaultValueFor_green_incr, float _blue_incr = kDefaultValueFor_blue_incr) :
                    min(_min), factor(_factor), red_incr(_red_incr), green_incr(_green_incr), blue_incr(_blue_incr)
                {
                }
            };

            struct __attribute__packed__ FireAnimation_t {

                enum class Orientation : uint8_t {
                    MIN = 0,
                    VERTICAL = MIN,
                    HORIZONTAL,
                    MAX
                };

                using Type = FireAnimation_t;
                CREATE_UINT8_BITFIELD_MIN_MAX(cooling, 8, 0, 255, 60, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(sparking, 8, 0, 255, 95, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(speed, 8, 1, 100, 50, 1);
                CREATE_ENUM_BITFIELD_SIZE_DEFAULT(orientation, 1, Orientation, std::underlying_type<Orientation>::type, uint8, Orientation::VERTICAL);
                CREATE_BOOL_BITFIELD(invert_direction);
                ClockColor_t factor;

                Orientation getOrientation() const {
                    return get_enum_orientation(*this);
                }

                FireAnimation_t(uint8_t _cooling = kDefaultValueFor_cooling, uint8_t _sparking = kDefaultValueFor_sparking, uint8_t _speed = kDefaultValueFor_speed, Orientation _orientation = Orientation::VERTICAL, bool _invert_direction = false) :
                    cooling(_cooling),
                    sparking(_sparking),
                    speed(_speed),
                    orientation(cast(_orientation)),
                    invert_direction(_invert_direction),
                    factor(0xffff00)
                {
                }
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
                    RAINBOW_FASTLED,
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
                static AnimationType getAnimationType(const __FlashStringHelper *name);
                static const __FlashStringHelper *getAnimationName(AnimationType type);
                static const __FlashStringHelper *getAnimationNameSlug(AnimationType type);
                static const __FlashStringHelper *getAnimationTitle(AnimationType type);

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
                CREATE_UINT8_BITFIELD_MIN_MAX(time_format_24h, 1, false, true, true, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(dithering, 1, false, true, false, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(standby_led, 1, false, true, true, 1);
                CREATE_UINT8_BITFIELD(enabled, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(fading_time, 6, 0, 60, 10, 1);
                #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
                    CREATE_UINT32_BITFIELD_MIN_MAX(power_limit, 8, 0, 255, 0, 1);
                #endif
                CREATE_UINT32_BITFIELD_MIN_MAX(brightness, 8, 0, 255, 255 / 4, 1);
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

                static constexpr uint16_t kPowerNumLeds = 256;
                #define kMultiplyNumLeds(value) static_cast<uint16_t>(value * 256)

                #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
                    struct __attribute__packed__ PowerType {
                        using Type = PowerType;
                        CREATE_UINT16_BITFIELD_MIN_MAX(red, 16, 0, 0xffff, kMultiplyNumLeds(79.7617), 1);
                        CREATE_UINT16_BITFIELD_MIN_MAX(green, 16, 0, 0xffff, kMultiplyNumLeds(79.9648), 1);
                        CREATE_UINT16_BITFIELD_MIN_MAX(blue, 16, 0, 0xffff, kMultiplyNumLeds(79.6055), 1);
                        CREATE_UINT16_BITFIELD_MIN_MAX(idle, 16, 0, 0xffff, kMultiplyNumLeds(4.0586), 1);
                        PowerType() : red(kDefaultValueFor_red), green(kDefaultValueFor_green), blue(kDefaultValueFor_blue), idle(kDefaultValueFor_idle) {}
                    } power;
                #endif

                struct __attribute__packed__ ProtectionType {
                    using Type = ProtectionType;
                    struct __attribute__packed__ TemperatureReduceRangeType {
                        using Type = TemperatureReduceRangeType;
                        CREATE_UINT8_BITFIELD_MIN_MAX(min, 8, 6, 85, 45, 1);
                        CREATE_UINT8_BITFIELD_MIN_MAX(max, 8, 6, 85, 60, 1);
                        TemperatureReduceRangeType() : min(kDefaultValueFor_min), max(kDefaultValueFor_max) {}
                    } temperature_reduce_range;
                    CREATE_UINT8_BITFIELD_MIN_MAX(max_temperature, 8, 6, 105, 70, 1);
                    CREATE_INT8_BITFIELD_MIN_MAX(regulator_margin, 8, -50, 50, 20, 1);
                    ProtectionType() : max_temperature(kDefaultValueFor_max_temperature), regulator_margin(kDefaultValueFor_regulator_margin) {}
                } protection;

                struct __attribute__packed__ RainbowAnimationType {
                    using Type = RainbowAnimationType;
                    RainbowMultiplier_t multiplier;
                    RainbowColor_t color;
                    CREATE_UINT32_BITFIELD_MIN_MAX(speed, 14, 0, 16383, 60, 1);
                    CREATE_UINT32_BITFIELD_MIN_MAX(bpm, 8, 0, 255, 10, 1);
                    CREATE_UINT32_BITFIELD_MIN_MAX(hue, 8, 0, 255, 10, 1);
                    RainbowAnimationType() :
                        speed(kDefaultValueFor_speed),
                        bpm(kDefaultValueFor_bpm),
                        hue(kDefaultValueFor_hue)
                    {
                    }
                } rainbow;

                struct __attribute__packed__ AlarmType {
                    using Type = AlarmType;
                    ClockColor_t color;
                    CREATE_UINT16_BITFIELD_MIN_MAX(speed, 16, 50, 0xffff, 0, 1);
                    AlarmType();
                } alarm;

                typedef struct __attribute__packed__ FadingType {
                    using Type = FadingType;
                    CREATE_UINT32_BITFIELD_MIN_MAX(speed, 17, 100, 100000, 1000, 100);
                    CREATE_UINT32_BITFIELD_MIN_MAX(delay, 12, 0, 3600, 3, 1);
                    ClockColor_t factor;
                    static constexpr uint32_t kDefaultValueFor_factor = 0xffffff;
                    FadingType() : speed(kDefaultValueFor_speed), delay(kDefaultValueFor_delay), factor(kDefaultValueFor_factor) {}
                } fading_t;
                fading_t fading;

                FireAnimation_t fire;

                #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
                    VisualizerAnimation_t visualizer;
                #endif

                struct __attribute_packed__ InterleavedType {
                    using Type = InterleavedType;
                    CREATE_UINT32_BITFIELD_MIN_MAX(time, 32, 0, 0xffffffffU, 60000, 1);
                    CREATE_UINT8_BITFIELD_MIN_MAX(rows, 8, 0, 0xff, 2, 1);
                    CREATE_UINT8_BITFIELD_MIN_MAX(cols, 8, 0, 0xff, 0, 1);
                    InterleavedType() : time(kDefaultValueFor_time), rows(kDefaultValueFor_rows), cols(kDefaultValueFor_cols) {}
                } interleaved;

                #if IOT_LED_MATRIX_CONFIGURABLE
                struct __attribute_packed__ MatrixType {
                    using Type = MatrixType;
                    CREATE_UINT16_BITFIELD_MIN_MAX(rows, 16, 1, 0xffff, IOT_LED_MATRIX_ROWS, 1);
                    CREATE_UINT16_BITFIELD_MIN_MAX(cols, 16, 1, 0xffff, IOT_LED_MATRIX_COLS, 1);
                    CREATE_UINT16_BITFIELD_MIN_MAX(pixels, 16, 1, 0xffff, IOT_CLOCK_NUM_PIXELS, 1);
                    CREATE_UINT16_BITFIELD_MIN_MAX(offset, 16, 1, 0xffff, IOT_LED_MATRIX_PIXEL_OFFSET, 1);
                    CREATE_UINT8_BITFIELD_MIN_MAX(reverse_rows, 1, false, true, IOT_LED_MATRIX_OPTS_REVERSE_ROWS, 1);
                    CREATE_UINT8_BITFIELD_MIN_MAX(reverse_cols, 1, false, true, IOT_LED_MATRIX_OPTS_REVERSE_COLS, 1);
                    CREATE_UINT8_BITFIELD_MIN_MAX(rotate, 1, false, true, IOT_LED_MATRIX_OPTS_ROTATE, 1);
                    CREATE_UINT8_BITFIELD_MIN_MAX(interleaved, 1, false, true, IOT_LED_MATRIX_OPTS_INTERLEAVED, 1);
                    MatrixType(
                        uint16_t _rows = kDefaultValueFor_rows,
                        uint16_t _cols = kDefaultValueFor_cols,
                        uint16_t _pixels = kDefaultValueFor_pixels,
                        uint16_t _offset = kDefaultValueFor_offset,
                        bool _reverse_rows = kDefaultValueFor_reverse_rows,
                        bool _reverse_cols = kDefaultValueFor_reverse_cols,
                        bool _rotate = kDefaultValueFor_rotate,
                        bool _interleaved = kDefaultValueFor_interleaved
                    ) :
                        rows(_rows),
                        cols(_cols),
                        pixels(_pixels),
                        offset(_offset),
                        reverse_rows(_reverse_rows),
                        reverse_cols(_reverse_cols),
                        rotate(_rotate),
                        interleaved(_interleaved)
                    {
                    }
                };

                MatrixType matrix;
                #endif

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

