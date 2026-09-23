/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

#if IOT_CLOCK
    #include "../src/plugins/clock/clock_def.h"
#endif

#ifndef IOT_CLOCK_SHOW_METHOD_MAX
    #define IOT_CLOCK_SHOW_METHOD_MAX 0
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
                SOLID = MIN,
                GRADIENT,
                RAINBOW,
                RAINBOW_FASTLED,
                FLASHING,
                FADING,
                FIRE,
                PLASMA,
                #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                    VISUALIZER,
                #endif
                XMAS,
                LAST,   // this can be used to loop through all animations: for(int i = 0; i <static_cast<int>(AnimationType::LAST); i++) {}
                #if !IOT_LED_MATRIX
                    COLON_SOLID = LAST,
                    COLON_BLINK_FAST,
                    COLON_BLINK_SLOWLY,
                    MAX,
                #else
                    MAX = LAST
                #endif
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
                static constexpr float kLEDVoltage = 5.0;
                static constexpr float kLEDCurrentIdle = 0.812;
                #define kMultiplyNumLeds(value) static_cast<uint16_t>((value) * kPowerNumLeds)
                #define kmA_perType(value) kMultiplyNumLeds((value) * kLEDVoltage)
                CREATE_UINT16_BITFIELD_MIN_MAX(red, 16, 0, 0xffff, kmA_perType(16.3 - kLEDCurrentIdle), 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(green, 16, 0, 0xffff, kmA_perType(16.4 - kLEDCurrentIdle), 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(blue, 16, 0, 0xffff, kmA_perType(16.2 - kLEDCurrentIdle), 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(idle, 16, 0, 0xffff, kmA_perType(kLEDCurrentIdle), 1);
                PowerConfigType() : red(kDefaultValueFor_red), green(kDefaultValueFor_green), blue(kDefaultValueFor_blue), idle(kDefaultValueFor_idle) {}
                #undef kMultiplyNumLeds
                #undef kmA_perType
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
                CREATE_COLOR_FIELD(factor, 0x000000);

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

            struct __attribute__packed__ PlasmaAnimationType {
                using Type = PlasmaAnimationType;
                CREATE_UINT32_BITFIELD_MIN_MAX(angle1, 9, 0, 360, 30, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(angle2, 9, 0, 360, 50, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(angle3, 9, 0, 360, 80, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(angle4, 9, 0, 360, 150, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(hue_shift, 8, 0, 255, 20, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(speed, 8, 1, 255, 32, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(x_size, 8, 1, 255, 8, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(y_size, 8, 1, 255, 16, 1);
                PlasmaAnimationType() :
                    angle1(kDefaultValueFor_angle1),
                    angle2(kDefaultValueFor_angle2),
                    angle3(kDefaultValueFor_angle3),
                    angle4(kDefaultValueFor_angle4),
                    hue_shift(kDefaultValueFor_hue_shift),
                    speed(kDefaultValueFor_speed),
                    x_size(kDefaultValueFor_x_size),
                    y_size(kDefaultValueFor_y_size)
                {
                }
            };

            struct __attribute__packed__ XmasAnimationType {
                using Type = XmasAnimationType;
                CREATE_UINT32_BITFIELD_MIN_MAX(speed, 16, 0, 60000, 5000, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(fade, 13, 0, 5000, 1000, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(palette, 3, 0, 4, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(sparkling, 7, 0, 100, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(pixels, 4, 1, 8, 3, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(gap, 4, 0, 8, 2, 1);
                CREATE_BOOL_BITFIELD(invert_direction);
                XmasAnimationType() :
                    speed(kDefaultValueFor_speed),
                    fade(kDefaultValueFor_fade),
                    palette(kDefaultValueFor_palette),
                    sparkling(kDefaultValueFor_sparkling),
                    pixels(kDefaultValueFor_pixels),
                    gap(kDefaultValueFor_gap),
                    invert_direction(false)
                {
                }
            };

            #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                struct __attribute__packed__ VisualizerType {
                    using Type = VisualizerType;
                    enum class OrientationType : uint8_t {
                        MIN = 0,
                        VERTICAL = MIN,
                        HORIZONTAL,
                        MAX
                    };
                    enum class VisualizerAnimationType : uint8_t {
                        SPECTRUM_RAINBOW_BARS_2D,
                        SPECTRUM_GRADIENT_BARS_2D,
                        SPECTRUM_COLOR_BARS_2D,
                        RGB565_VIDEO,
                        RGB24_VIDEO,
                        PLASMA_AUDIO,
                        FIRE_AUDIO,
                        MAX,
                    };
                    enum class VisualizerPeakType : uint8_t {
                        MIN = 0,
                        DISABLED,
                        ENABLED,
                        FADING,
                        FALLING_DOWN,
                        MAX,
                    };
                    enum class AudioInputType : uint8_t {
                        MIN = 0,
                        #if IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT
                            UDP,
                        #endif
                        #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
                            MICROPHONE,
                        #endif
                        MAX,
                    };

                    // audio reaction, shared by the audio reactive plasma and fire modes
                    struct __attribute__packed__ AudioReactionType {
                        using Type = AudioReactionType;

                        CREATE_UINT32_BITFIELD_MIN_MAX(sensitivity, 10, 1, 1000, 100, 1); // gain for the level in percent (100 = 1x)
                        CREATE_UINT32_BITFIELD_MIN_MAX(attack, 16, 1, 5000, 40, 5); // milliseconds to follow a rising level
                        CREATE_UINT32_BITFIELD_MIN_MAX(release, 16, 1, 5000, 400, 10); // milliseconds to follow a falling level

                        AudioReactionType() :
                            sensitivity(kDefaultValueFor_sensitivity),
                            attack(kDefaultValueFor_attack),
                            release(kDefaultValueFor_release)
                        {
                        }
                    };

                    // audio reactive plasma animation, all parameters are independent from the standalone plasma animation
                    struct __attribute__packed__ PlasmaAudioType {
                        using Type = PlasmaAudioType;

                        // plasma field
                        CREATE_UINT32_BITFIELD_MIN_MAX(angle1, 9, 0, 360, 30, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(angle2, 9, 0, 360, 50, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(angle3, 9, 0, 360, 80, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(angle4, 9, 0, 360, 150, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(hue_shift, 8, 0, 255, 20, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(speed, 8, 1, 255, 32, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(x_size, 8, 1, 255, 8, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(y_size, 8, 1, 255, 16, 1);

                        // reactions
                        CREATE_BOOL_BITFIELD(enable_speed);
                        CREATE_UINT32_BITFIELD_MIN_MAX(speed_gain, 8, 0, 255, 128, 1);
                        CREATE_BOOL_BITFIELD(enable_hue);
                        CREATE_UINT32_BITFIELD_MIN_MAX(hue_gain, 8, 0, 255, 96, 1);
                        CREATE_BOOL_BITFIELD(enable_zoom);
                        CREATE_UINT32_BITFIELD_MIN_MAX(zoom_gain, 8, 0, 255, 128, 1);
                        CREATE_BOOL_BITFIELD(enable_bands);
                        CREATE_UINT32_BITFIELD_MIN_MAX(band_gain, 8, 0, 255, 128, 1);

                        PlasmaAudioType() :
                            angle1(kDefaultValueFor_angle1),
                            angle2(kDefaultValueFor_angle2),
                            angle3(kDefaultValueFor_angle3),
                            angle4(kDefaultValueFor_angle4),
                            hue_shift(kDefaultValueFor_hue_shift),
                            speed(kDefaultValueFor_speed),
                            x_size(kDefaultValueFor_x_size),
                            y_size(kDefaultValueFor_y_size),

                            enable_speed(true),
                            speed_gain(kDefaultValueFor_speed_gain),
                            enable_hue(true),
                            hue_gain(kDefaultValueFor_hue_gain),
                            enable_zoom(false),
                            zoom_gain(kDefaultValueFor_zoom_gain),
                            enable_bands(true),
                            band_gain(kDefaultValueFor_band_gain)
                        {
                        }
                    };

                    // audio reactive fire animation, all parameters are independent from the standalone fire animation
                    struct __attribute__packed__ FireAudioType {
                        using Type = FireAudioType;
                        enum class DirectionType : uint8_t {
                            MIN = 0,
                            HORIZONTAL = MIN,       // fire lines are the rows, the flames grow along the columns
                            HORIZONTAL_FLIPPED,     // ... into the opposite direction
                            VERTICAL,               // fire lines are the columns, the flames grow along the rows
                            VERTICAL_FLIPPED,       // ... into the opposite direction
                            MAX
                        };

                        // fire
                        CREATE_UINT32_BITFIELD_MIN_MAX(cooling, 8, 0, 255, 60, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(sparking, 8, 0, 255, 95, 1);
                        CREATE_UINT32_BITFIELD_MIN_MAX(speed, 8, 1, 100, 50, 1); // milliseconds between two updates
                        CREATE_COLOR_FIELD(factor, 0x000000);
                        // the enum values match the pixel mapping of FireField::copyTo()
                        CREATE_ENUM_D_BITFIELD(direction, DirectionType, DirectionType::VERTICAL);

                        // reactions
                        CREATE_BOOL_BITFIELD(enable_speed);
                        CREATE_UINT32_BITFIELD_MIN_MAX(speed_gain, 8, 0, 255, 128, 1);
                        CREATE_BOOL_BITFIELD(enable_heat);
                        CREATE_UINT32_BITFIELD_MIN_MAX(heat_gain, 8, 0, 255, 96, 1);
                        CREATE_BOOL_BITFIELD(enable_sparks);
                        CREATE_UINT32_BITFIELD_MIN_MAX(spark_gain, 8, 0, 255, 128, 1);
                        CREATE_BOOL_BITFIELD(enable_bands);
                        CREATE_UINT32_BITFIELD_MIN_MAX(band_gain, 8, 0, 255, 160, 1);

                        DirectionType getDirection() const {
                            return get_enum_direction(*this);
                        }

                        // vertical directions use the columns as fire lines
                        bool isVertical() const {
                            return getDirection() >= DirectionType::VERTICAL;
                        }

                        FireAudioType() :
                            cooling(kDefaultValueFor_cooling),
                            sparking(kDefaultValueFor_sparking),
                            speed(kDefaultValueFor_speed),
                            factor(kDefaultValueFor_factor),
                            direction(kDefaultValueFor_direction),
                            enable_speed(true),
                            speed_gain(kDefaultValueFor_speed_gain),
                            enable_heat(true),
                            heat_gain(kDefaultValueFor_heat_gain),
                            enable_sparks(true),
                            spark_gain(kDefaultValueFor_spark_gain),
                            enable_bands(true),
                            band_gain(kDefaultValueFor_band_gain)
                        {
                        }
                    };

                    CREATE_UINT32_BITFIELD_MIN_MAX(port, 16, 0, 65535, IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT);
                    CREATE_UINT32_BITFIELD_MIN_MAX(peak_falling_speed, 15, 250, 15000, 2500, 100);
                    CREATE_UINT32_BITFIELD_MIN_MAX(peak_extra_color, 1, 0, 1, 1);
                    CREATE_UINT32_BITFIELD_MIN_MAX(vumeter_rows, 4, 0, 8, 1);
                    CREATE_UINT32_BITFIELD_MIN_MAX(vumeter_peaks, 1, 0, 1, 1);
                    CREATE_COLOR_FIELD(color, 0x00ff00);
                    CREATE_COLOR_FIELD(peak_color, 0xff0000);
                    CREATE_BOOL_BITFIELD(multicast);
                    CREATE_ENUM_D_BITFIELD(peak_show, VisualizerPeakType, VisualizerPeakType::FALLING_DOWN);
                    CREATE_ENUM_D_BITFIELD(type, VisualizerAnimationType, VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D);
                    CREATE_ENUM_BITFIELD_SIZE_DEFAULT(orientation, 1, OrientationType, std::underlying_type<OrientationType>::type, uint8, OrientationType::HORIZONTAL);
                    CREATE_ENUM_BITFIELD_SIZE_DEFAULT(input, 2, AudioInputType, std::underlying_type<AudioInputType>::type, uint8, AudioInputType(int(AudioInputType::MIN) + 1));
                    CREATE_UINT32_BITFIELD_MIN_MAX(mic_loudness_gain, 10, 1, 1000, 275, 1);
                    CREATE_UINT32_BITFIELD_MIN_MAX(mic_band_gain, 10, 1, 1000, 175, 1);

                    AudioReactionType audio_reaction;
                    PlasmaAudioType plasma_audio;
                    FireAudioType fire_audio;

                    VisualizerType() :
                        port(kDefaultValueFor_port),
                        peak_falling_speed(kDefaultValueFor_peak_falling_speed),
                        peak_extra_color(kDefaultValueFor_peak_extra_color),
                        vumeter_rows(kDefaultValueFor_vumeter_rows),
                        vumeter_peaks(kDefaultValueFor_vumeter_peaks),
                        color(kDefaultValueFor_color),
                        peak_color(kDefaultValueFor_peak_color),
                        multicast(false),
                        peak_show(kDefaultValueFor_peak_show),
                        type(kDefaultValueFor_type),
                        orientation(kDefaultValueFor_orientation),
                        input(kDefaultValueFor_input),
                        mic_loudness_gain(kDefaultValueFor_mic_loudness_gain),
                        mic_band_gain(kDefaultValueFor_mic_band_gain),
                        audio_reaction(),
                        plasma_audio(),
                        fire_audio()
                    {
                    }
                };
            #endif

            #if IOT_LED_MATRIX_CONFIGURABLE
            struct __attribute__packed__ MatrixConfigType {
                using Type = MatrixConfigType;
                CREATE_UINT32_BITFIELD_MIN_MAX(rows, 16, 1, 0xffff, IOT_LED_MATRIX_ROWS, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(cols, 16, 1, 0xffff, IOT_LED_MATRIX_COLS, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(rowOfs, 16, 0, 0xffff, IOT_LED_MATRIX_ROW_OFS, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(colOfs, 16, 0, 0xffff, IOT_LED_MATRIX_COL_OFS, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(pixels0, 13, 0, 4096, std::min(1024, IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS), 1); // segment 1
                CREATE_UINT32_BITFIELD_MIN_MAX(offset0, 13, 0, 4096, IOT_LED_MATRIX_PIXEL_OFFSET, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(pixels1, 13, 0, 4096, 0, 1); // segment 2
                CREATE_UINT32_BITFIELD_MIN_MAX(offset1, 13, 0, 4096, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(pixels2, 13, 0, 4096, 0, 1); // segment 3
                CREATE_UINT32_BITFIELD_MIN_MAX(offset2, 13, 0, 4096, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(pixels3, 13, 0, 4096, 0, 1); // segment 4
                CREATE_UINT32_BITFIELD_MIN_MAX(offset3, 13, 0, 4096, 0, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(reverse_rows, 1, false, true, IOT_LED_MATRIX_OPTS_REVERSE_ROWS, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(reverse_cols, 1, false, true, IOT_LED_MATRIX_OPTS_REVERSE_COLS, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(rotate, 1, false, true, IOT_LED_MATRIX_OPTS_ROTATE, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(interleaved, 1, false, true, IOT_LED_MATRIX_OPTS_INTERLEAVED, 1);
                MatrixConfigType(
                    uint16_t _rows = kDefaultValueFor_rows,
                    uint16_t _cols = kDefaultValueFor_cols,
                    uint16_t _rowOfs = kDefaultValueFor_rowOfs,
                    uint16_t _colOfs = kDefaultValueFor_colOfs,
                    uint16_t _pixels0 = kDefaultValueFor_pixels0,
                    uint16_t _offset0 = kDefaultValueFor_offset0,
                    uint16_t _pixels1 = kDefaultValueFor_pixels1,
                    uint16_t _offset1 = kDefaultValueFor_offset1,
                    uint16_t _pixels2 = kDefaultValueFor_pixels2,
                    uint16_t _offset2 = kDefaultValueFor_offset2,
                    uint16_t _pixels3 = kDefaultValueFor_pixels3,
                    uint16_t _offset3 = kDefaultValueFor_offset3,
                    bool _reverse_rows = kDefaultValueFor_reverse_rows,
                    bool _reverse_cols = kDefaultValueFor_reverse_cols,
                    bool _rotate = kDefaultValueFor_rotate,
                    bool _interleaved = kDefaultValueFor_interleaved
                ) :
                    rows(_rows),
                    cols(_cols),
                    rowOfs(_rowOfs),
                    colOfs(_colOfs),
                    pixels0(_pixels0),
                    offset0(_offset0),
                    pixels1(_pixels1),
                    offset1(_offset1),
                    pixels2(_pixels2),
                    offset2(_offset2),
                    pixels3(_pixels3),
                    offset3(_offset3),
                    reverse_rows(_reverse_rows),
                    reverse_cols(_reverse_cols),
                    rotate(_rotate),
                    interleaved(_interleaved)
                {
                }
            };

            #endif

            #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1

                // IR remote control
                //
                // Nothing is hardcoded, every button has to be configured by the user and an
                // unassigned button does nothing (kNoCode). The receiver decodes the NEC protocol,
                // which is used by most cheap remotes, and logs the 32 bit code of every data frame
                // (`IR xxxxxxxx`, visible in the serial console and on status.html). To assign a
                // button, press it and copy the value into the form.
                struct __attribute__packed__ IRRemoteConfigType {
                    using Type = IRRemoteConfigType;

                    static constexpr uint32_t kNoCode = 0;              // button is not assigned
                    static constexpr uint8_t kNumColors = 20;           // 5 rows with 4 color buttons
                    static constexpr uint8_t kNumChannels = 3;          // red, green, blue
                    static constexpr uint8_t kMaxStep = 127;
                    static constexpr uint8_t kConfigVersion = 1;

                    enum class ActionType : uint8_t {
                        POWER = 0,
                        NEXT_ANIMATION,
                        BRIGHTNESS_UP,
                        BRIGHTNESS_DOWN,
                        RED_UP,
                        GREEN_UP,
                        BLUE_UP,
                        RED_DOWN,
                        GREEN_DOWN,
                        BLUE_DOWN,
                        COLOR_1,
                        COLOR_LAST = COLOR_1 + kNumColors - 1,
                        MAX = COLOR_LAST + 1,
                    };

                    // The config is stored as one binary blob and resized/zero filled when the layout
                    // changes (see WriteableData::resize). A stored config without this version has
                    // never been written by this firmware and gets the defaults applied once (see
                    // ClockPlugin::readConfig())
                    CREATE_UINT8_BITFIELD(version, 4);
                    // turning this off disables the receiver (no codes are received/logged)
                    CREATE_UINT8_BITFIELD_MIN_MAX(enabled, 1, false, true, true, 1);
                    // A bit-field that crosses the boundary of its storage unit (1 byte for uint8_t)
                    // was laid out differently before GCC 4.4, which is reported by
                    // -Wpacked-bitfield-compat. Complete the byte to keep the field on the boundary,
                    // this is a no-op for the layout: version/enabled/step keep their bit positions
                    // and the size of the structure does not change.
                    uint8_t : 3;
                    // the color channels are changed by this value per button press
                    CREATE_UINT8_BITFIELD_MIN_MAX(step, 7, 1, kMaxStep, 8, 1);

                    // code of every button, index is ActionType
                    uint32_t codes[static_cast<uint8_t>(ActionType::MAX)]{};
                    // color of the COLOR_x buttons
                    ColorType color_value[kNumColors]{};

                    IRRemoteConfigType() {
                        applyDefaults();
                    }

                    bool isInitialized() const {
                        return version == kConfigVersion;
                    }

                    // Defaults of the 44 key remote control (docs/IR_Remote_44_Keys.md). Only the NEC
                    // protocol is implemented in the firmware, the buttons of any other remote can be
                    // assigned in the IR Remote form. Unassigned buttons keep kNoCode
                    void applyDefaults() {
                        version = kConfigVersion;
                        enabled = kDefaultValueFor_enabled;
                        step = kDefaultValueFor_step;
                        memset(codes, 0, sizeof(codes));
                        memset(color_value, 0, sizeof(color_value));

                        setCode(ActionType::POWER, 0xbf40ff00);
                        setCode(ActionType::NEXT_ANIMATION, 0xbe41ff00);
                        setCode(ActionType::BRIGHTNESS_UP, 0xa35cff00);
                        setCode(ActionType::BRIGHTNESS_DOWN, 0xa25dff00);
                        // up/down arrows of the red, green and blue block
                        setCode(ActionType::RED_UP, 0xeb14ff00);
                        setCode(ActionType::GREEN_UP, 0xea15ff00);
                        setCode(ActionType::BLUE_UP, 0xe916ff00);
                        setCode(ActionType::RED_DOWN, 0xef10ff00);
                        setCode(ActionType::GREEN_DOWN, 0xee11ff00);
                        setCode(ActionType::BLUE_DOWN, 0xed12ff00);
                        // R, G, B and W of the color buttons 1-4
                        setCode(colorAction(0), 0xa758ff00);
                        setColor(colorAction(0), 0xff0000);
                        setCode(colorAction(1), 0xa659ff00);
                        setColor(colorAction(1), 0x00ff00);
                        setCode(colorAction(2), 0xba45ff00);
                        setColor(colorAction(2), 0x0000ff);
                        setCode(colorAction(3), 0xbb44ff00);
                        setColor(colorAction(3), 0xffffff);
                    }

                    // the struct is packed, the members must be accessed by value
                    uint32_t getCode(ActionType action) const {
                        return codes[static_cast<uint8_t>(action)];
                    }

                    void setCode(ActionType action, uint32_t code) {
                        codes[static_cast<uint8_t>(action)] = code;
                    }

                    static bool isColor(ActionType action) {
                        return action >= ActionType::COLOR_1;
                    }

                    // color button 0-19 as ActionType, the enum only names the first and the last one
                    static constexpr ActionType colorAction(uint8_t index) {
                        return static_cast<ActionType>(static_cast<uint8_t>(ActionType::COLOR_1) + index);
                    }

                    uint32_t getColor(ActionType action) const {
                        return color_value[static_cast<uint8_t>(action) - static_cast<uint8_t>(ActionType::COLOR_1)].value;
                    }

                    void setColor(ActionType action, uint32_t color) {
                        color_value[static_cast<uint8_t>(action) - static_cast<uint8_t>(ActionType::COLOR_1)].value = color;
                    }

                    static bool hasCode(uint32_t code) {
                        return code != kNoCode;
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
                CREATE_UINT8_BITFIELD_MIN_MAX(method, 3, 1/*MIN + 1*/, IOT_CLOCK_SHOW_METHOD_MAX, 1/*FASTLED*/, 1); // Clock::ShowMethodType
                CREATE_UINT32_BITFIELD_MIN_MAX(fading_time, 6, 0, 60, 10, 1);
                #if IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
                    CREATE_UINT32_BITFIELD_MIN_MAX(power_limit, 12, 0, IOT_LED_MATRIX_MAX_POWER, 0, 1);
                #endif
                CREATE_UINT32_BITFIELD_MIN_MAX(brightness, 8, 0, 255, 255 / 4, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(group_pixels, 2, 0, 2, 0, 1);
                #if !IOT_LED_MATRIX
                    CREATE_UINT32_BITFIELD_MIN_MAX(blink_colon_speed, 13, 50, 8000, 1000, 100);
                #endif
                CREATE_UINT32_BITFIELD_MIN_MAX(flashing_speed, 13, 50, 8000, 150, 100);
                CREATE_COLOR_FIELD(flashing_color, 0x00ff00);
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
                PlasmaAnimationType plasma;
                AlarmType alarm;
                PowerConfigType power;
                #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                    VisualizerType visualizer;
                #endif
                #if IOT_LED_MATRIX_CONFIGURABLE
                    MatrixConfigType matrix;
                #endif
                XmasAnimationType xmas;
                #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
                    IRRemoteConfigType ir;
                #endif

                uint16_t getBrightness() const
                {
                    return brightness;
                }

                void setBrightness(uint8_t pBrightness)
                {
                    brightness = pBrightness;
                }

                uint32_t getFadingTimeMillis() const
                {
                    return fading_time * 1000;
                }
                void setFadingTimeMillis(uint32_t time)
                {
                    fading_time = time / 1000;
                }

                AnimationType getAnimation() const
                {
                    return get_enum_animation(*this);
                }

                void setAnimation(AnimationType animation)
                {
                    set_enum_animation(*this, animation);
                }

                bool hasColorSupport() const
                {
                    switch(getAnimation()) {
                        case AnimationType::FADING:
                        case AnimationType::SOLID:
                        case AnimationType::FLASHING:
                        case AnimationType::PLASMA:
                        #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                            case AnimationType::VISUALIZER:
                        #endif
                            return true;
                        default:
                            break;
                    }
                    return false;
                }

                InitialStateType getInitialState() const
                {
                    return get_enum_initial_state(*this);
                }

                void setInitialState(InitialStateType state)
                {
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
