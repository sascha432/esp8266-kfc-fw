/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

#if IOT_CLOCK
    #include "../src/plugins/clock/clock_def.h"
#endif

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Clock

        namespace ClockConfigNS {

            union __attribute__packed__ ColorType {
                uint32_t value: 24;
                uint8_t bgr[3];
                struct __attribute__packed__ {
                    uint8_t blue;
                    uint8_t green;
                    uint8_t red;
                };
                ColorType(uint32_t val = 0) : value(val) {}
                operator uint32_t() const {
                    return value;
                }
            };

            #define CREATE_COLOR_FIELD(name, default_value) \
                static constexpr uint32_t kDefaultValueFor_##name = default_value; \
                ColorType name{kDefaultValueFor_##name}; \
                inline static void set_bits_##name(Type &obj, uint32_t color) { \
                    obj.name = color; \
                } \
                inline static uint32_t get_bits_##name(const Type &obj) { \
                    return obj.name; \
                }

            enum class AnimationType : uint8_t {
                MIN = 0,
                SOLID = 0,
                GRADIENT,
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

            enum class InitialStateType : uint8_t  {
                MIN = 0,
                OFF = 0,
                ON,
                RESTORE,
                MAX
            };

            struct __attribute__packed__ PowerConfigType {
                using Type = PowerConfigType;
                static constexpr uint16_t kPowerNumLeds = 256;
                #define kMultiplyNumLeds(value) static_cast<uint16_t>(value * 256)
                CREATE_UINT16_BITFIELD_MIN_MAX(red, 16, 0, 0xffff, kMultiplyNumLeds(79.7617), 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(green, 16, 0, 0xffff, kMultiplyNumLeds(79.9648), 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(blue, 16, 0, 0xffff, kMultiplyNumLeds(79.6055), 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(idle, 16, 0, 0xffff, kMultiplyNumLeds(4.0586), 1);
                PowerConfigType() : red(kDefaultValueFor_red), green(kDefaultValueFor_green), blue(kDefaultValueFor_blue), idle(kDefaultValueFor_idle) {}
            };

            struct __attribute__packed__ ProtectionConfigType {
                using Type = ProtectionConfigType;
                struct __attribute__packed__ TemperatureReduceRangeType {
                    using Type = TemperatureReduceRangeType;
                    CREATE_UINT8_BITFIELD_MIN_MAX(min, 8, 6, 85, 45, 1);
                    CREATE_UINT8_BITFIELD_MIN_MAX(max, 8, 6, 85, 60, 1);
                    TemperatureReduceRangeType() : min(kDefaultValueFor_min), max(kDefaultValueFor_max) {}
                } temperature_reduce_range;
                CREATE_UINT8_BITFIELD_MIN_MAX(max_temperature, 8, 6, 105, 70, 1);
                CREATE_INT8_BITFIELD_MIN_MAX(regulator_margin, 8, -50, 50, 20, 1);
                ProtectionConfigType() : max_temperature(kDefaultValueFor_max_temperature), regulator_margin(kDefaultValueFor_regulator_margin) {}
            } ;

            struct __attribute__packed__ RainbowAnimationType {
                using Type = RainbowAnimationType;
                struct __attribute__packed__ MultiplierType {
                    using Type = MultiplierType;
                    CREATE_FLOAT_FIELD(value, 0.1, 100.0, 0);
                    CREATE_FLOAT_FIELD(min, 0.1, 100.0, 0);
                    CREATE_FLOAT_FIELD(max, 0.1, 100.0, 0);
                    CREATE_FLOAT_FIELD(incr, 0, 0.1, 0);
                    MultiplierType(float _value = kDefaultValueFor_value, float _min = kDefaultValueFor_min, float _max = kDefaultValueFor_max, float _incr = kDefaultValueFor_incr) :
                        value(_value), min(_min), max(_max), incr(_incr)
                    {
                    }
                } multiplier;
                struct __attribute__packed__ ColorAnimationType {
                    using Type = ColorAnimationType;
                    CREATE_COLOR_FIELD(min, 0);
                    CREATE_COLOR_FIELD(factor, 0xffffff);
                    CREATE_FLOAT_FIELD(red_incr, 0, 0.1, 0);
                    CREATE_FLOAT_FIELD(green_incr, 0, 0.1, 0);
                    CREATE_FLOAT_FIELD(blue_incr, 0, 0.1, 0);
                    ColorAnimationType(uint32_t _min = kDefaultValueFor_min, uint32_t _factor = kDefaultValueFor_factor, float _red_incr = kDefaultValueFor_red_incr, float _green_incr = kDefaultValueFor_green_incr, float _blue_incr = kDefaultValueFor_blue_incr) :
                        min(_min), factor(_factor), red_incr(_red_incr), green_incr(_green_incr), blue_incr(_blue_incr)
                    {
                    }
                } color;
                CREATE_UINT32_BITFIELD_MIN_MAX(speed, 14, 0, 16383, 60, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(bpm, 8, 0, 255, 12, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(hue, 8, 0, 254, 14, 1);
                CREATE_BOOL_BITFIELD(invert_direction);
                RainbowAnimationType() :
                    speed(kDefaultValueFor_speed),
                    bpm(kDefaultValueFor_bpm),
                    hue(kDefaultValueFor_hue),
                    invert_direction(false)
                {
                }
            };

            struct __attribute__packed__ AlarmType {
                using Type = AlarmType;
                CREATE_COLOR_FIELD(color, 0xff0000);
                CREATE_UINT16_BITFIELD_MIN_MAX(speed, 16, 50, 0xffff, 0, 1);
                AlarmType();
            };

            struct __attribute__packed__ FadingAnimationType {
                using Type = FadingAnimationType;
                CREATE_UINT32_BITFIELD_MIN_MAX(speed, 17, 100, 100000, 1000, 100);
                CREATE_UINT32_BITFIELD_MIN_MAX(delay, 12, 0, 3600, 3, 1);
                CREATE_COLOR_FIELD(factor, 0xffffff);
                FadingAnimationType() : speed(kDefaultValueFor_speed), delay(kDefaultValueFor_delay), factor(kDefaultValueFor_factor) {}
            };

            #ifndef IOT_CLOCK_GRADIENT_ENTRIES
            #define IOT_CLOCK_GRADIENT_ENTRIES 8
            #endif

            struct __attribute__packed__ GradientAnimationType {
                using Type = GradientAnimationType;
                static constexpr size_t kMaxEntries = IOT_CLOCK_GRADIENT_ENTRIES;
                static constexpr uint16_t kDisabled = ~0;

                struct __attribute__packed__ Entry {
                    using Type = Entry;
                    CREATE_COLOR_FIELD(color, 0);
                    CREATE_UINT16_BITFIELD_MIN_MAX(pixel, 16, 0, 0xffff, kDisabled, 1);
                    Entry() : color(0), pixel(kDefaultValueFor_pixel) {}
                    Entry(uint32_t _color, uint16_t _pixel) : color(_color), pixel(_pixel) {}
                };
                CREATE_UINT16_BITFIELD_MIN_MAX(speed, 16, 0, 0xffff - 256, 0, 1);
                Entry entries[kMaxEntries];
                void sort() {
                    std::sort(std::begin(entries), std::end(entries), [](const Entry &a, const Entry &b) {
                        return b.pixel > a.pixel;
                    });
                }
                GradientAnimationType() :
                    speed(kDefaultValueFor_speed),
                    entries{
                        #ifdef IOT_CLOCK_NUM_PIXELS
                            Entry(0x0000ff, 0),
                            Entry(0x00ff00, IOT_CLOCK_NUM_PIXELS / 2),
                            Entry(0xff0000, IOT_CLOCK_NUM_PIXELS - 1)
                        #endif
                    }
                {}
            };

            struct __attribute__packed__ FireAnimationType {
                enum class OrientationType : uint8_t {
                    MIN = 0,
                    VERTICAL = MIN,
                    HORIZONTAL,
                    MAX
                };
                using Type = FireAnimationType;
                CREATE_UINT8_BITFIELD_MIN_MAX(cooling, 8, 0, 255, 60, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(sparking, 8, 0, 255, 95, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(speed, 8, 1, 100, 50, 1);
                CREATE_ENUM_BITFIELD_SIZE_DEFAULT(orientation, 1, OrientationType, std::underlying_type<OrientationType>::type, uint8, OrientationType::VERTICAL);
                CREATE_BOOL_BITFIELD(invert_direction);
                CREATE_COLOR_FIELD(factor, 0xffff00);
                OrientationType getOrientation() const {
                    return get_enum_orientation(*this);
                }
                FireAnimationType(uint8_t _cooling = kDefaultValueFor_cooling, uint8_t _sparking = kDefaultValueFor_sparking, uint8_t _speed = kDefaultValueFor_speed, OrientationType _orientation = OrientationType::VERTICAL, bool _invert_direction = false) :
                    cooling(_cooling),
                    sparking(_sparking),
                    speed(_speed),
                    orientation(cast(_orientation)),
                    invert_direction(_invert_direction),
                    factor(kDefaultValueFor_factor)
                {
                }
            };

            #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
                struct __attribute__packed__ VisualizerType {
                    using Type = VisualizerType;
                    enum class OrientationType : uint8_t {
                        MIN = 0,
                        VERTICAL = MIN,
                        HORIZONTAL,
                        MAX
                    };
                    enum class VisualizerAnimationType : uint8_t {
                        LOUDNESS_1D,
                        LOUDNESS_STEREO_1D,
                        SPECTRUM_RAINBOW_1D,
                        SPECTRUM_RAINBOW_STEREO_1D,
                        SPECTRUM_RAINBOW_BARS_2D,
                        SPECTRUM_COLOR_BARS_2D,
                        RGB565_VIDEO,
                        RGB24_VIDEO,
                        MAX,
                    };

                    CREATE_UINT32_BITFIELD_MIN_MAX(port, 16, 0, 65535, 4210);
                    CREATE_ENUM_D_BITFIELD(type, VisualizerAnimationType, VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D);
                    CREATE_COLOR_FIELD(color, 0xff00ff);
                    CREATE_COLOR_FIELD(peak_color, 0x0000ff);
                    CREATE_BOOL_BITFIELD(multicast);
                    CREATE_ENUM_BITFIELD_SIZE_DEFAULT(orientation, 1, OrientationType, std::underlying_type<OrientationType>::type, uint8, OrientationType::HORIZONTAL);

                    VisualizerType() :
                        port(kDefaultValueFor_port),
                        type(kDefaultValueFor_type),
                        color(kDefaultValueFor_color),
                        peak_color(kDefaultValueFor_peak_color),
                        multicast(false),
                        orientation(kDefaultValueFor_orientation)
                    {
                    }
                };
            #endif

            struct __attribute__packed__ InterleavedAnimationType {
                using Type = InterleavedAnimationType;
                CREATE_UINT32_BITFIELD_MIN_MAX(time, 32, 0, 0xffffffffU, 60000, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(rows, 8, 0, 0xff, 2, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(cols, 8, 0, 0xff, 0, 1);
                InterleavedAnimationType() : time(kDefaultValueFor_time), rows(kDefaultValueFor_rows), cols(kDefaultValueFor_cols) {}
            };

            #if IOT_LED_MATRIX_CONFIGURABLE
            struct __attribute__packed__ MatrixConfigType {
                using Type = MatrixConfigType;
                CREATE_UINT16_BITFIELD_MIN_MAX(rows, 16, 1, 0xffff, IOT_LED_MATRIX_ROWS, 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(cols, 16, 1, 0xffff, IOT_LED_MATRIX_COLS, 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(pixels, 16, 1, 0xffff, IOT_CLOCK_NUM_PIXELS, 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(offset, 16, 1, 0xffff, IOT_LED_MATRIX_PIXEL_OFFSET, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(reverse_rows, 1, false, true, IOT_LED_MATRIX_OPTS_REVERSE_ROWS, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(reverse_cols, 1, false, true, IOT_LED_MATRIX_OPTS_REVERSE_COLS, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(rotate, 1, false, true, IOT_LED_MATRIX_OPTS_ROTATE, 1);
                CREATE_UINT8_BITFIELD_MIN_MAX(interleaved, 1, false, true, IOT_LED_MATRIX_OPTS_INTERLEAVED, 1);
                MatrixConfigType(
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

            #endif

            struct __attribute__packed__ ClockConfigType {
                using Type = ClockConfigType;
                CREATE_COLOR_FIELD(solid_color, 0xff00ff);
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

                ProtectionConfigType protection;
                GradientAnimationType gradient;
                RainbowAnimationType rainbow;
                FadingAnimationType fading;
                FireAnimationType fire;
                AlarmType alarm;
                InterleavedAnimationType interleaved;
                PowerConfigType power;
                #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
                    VisualizerType visualizer;
                #endif
                #if IOT_LED_MATRIX_CONFIGURABLE
                    MatrixConfigType matrix;
                #endif

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

                static const __FlashStringHelper *getAnimationNames();
                static const __FlashStringHelper *getAnimationNamesJsonArray();
                static AnimationType getAnimationType(const __FlashStringHelper *name);
                static const __FlashStringHelper *getAnimationName(AnimationType type);
                static const __FlashStringHelper *getAnimationNameSlug(AnimationType type);
                static const __FlashStringHelper *getAnimationTitle(AnimationType type);
                static String &normalizeSlug(String &slug);

                ClockConfigType();
            };

            class Clock : public ClockConfigType, public KFCConfigurationClasses::ConfigGetterSetter<ClockConfigType, _H(MainConfig().plugins.clock.cfg) CIF_DEBUG(, &handleNameClockConfig_t)>
            {
            public:
                static void defaults();
            };

        }
    }
}
