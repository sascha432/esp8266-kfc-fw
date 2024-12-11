/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <KFCForms.h>
#include "../src/plugins/sensor/sensor.h"
#include "Utility/ProgMemHelper.h"
#include "animation.h"
#include <HeapSelector.h>

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#if IOT_LED_STRIP
#    define FORM_TITLE "LED Strip Configuration"
#elif IOT_LED_MATRIX
#    define FORM_TITLE "LED Matrix Configuration"
#else
#    define FORM_TITLE "Clock Configuration"
#endif

void ClockPlugin::_saveState()
{
    #if ESP8266
        constexpr uint32_t kSaveDelay = 10000;
    #else
        constexpr uint32_t kSaveDelay = 5000;
    #endif
    // delay writing config
    _Timer(_saveTimer).add(Event::milliseconds(kSaveDelay), false, [this](Event::CallbackTimerPtr) {
        Plugins::Clock::setConfig(_config);
        __LDBG_printf("delay save dirty=%u", config.isDirty());
        if (config.isDirty()) {
            config.write();
        }
    });
}

void ClockPlugin::_setState(bool state, bool autoOff)
{
    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        _motionAutoOff = autoOff;
    #endif
    if (state) {
        if (_targetBrightness == 0) {
            if (_savedBrightness) {
                setBrightness(_savedBrightness);
            }
            else {
                setBrightness(_config.getBrightness());
            }
        }
    }
    else {
        if (_targetBrightness != 0) {
            _savedBrightness = _targetBrightness;
        }
        setBrightness(0);
    }
    _saveState();
}

void ClockPlugin::_createConfigureFormAnimation(AnimationType animation, FormUI::Form::BaseForm &form, ClockConfigType &cfg, TitleType titleType)
{
    FormUI::Group *group;
    switch(titleType) {
        case TitleType::SET_TITLE_AND_ADD_GROUP:
            form.getWebUIConfig().setTitle(F(FORM_TITLE));
            // fall through
        case TitleType::ADD_GROUP:
            group = &form.addCardGroup(_getAnimationNameSlug(animation), _getAnimationTitle(animation), true);
            break;
        case TitleType::SET_TITLE:
            form.getWebUIConfig().setTitle(_getAnimationTitle(animation));
            // fall through
        default:
            group = nullptr;
            break;
    }

    switch(animation) {
        case AnimationType::FADING: {
                form.addObjectGetterSetter(F("fse"), cfg.fading, cfg.fading.get_bits_speed, cfg.fading.set_bits_speed);
                form.addFormUI(F("Time Between Fading Colors"), FormUI::Suffix(FSPGM(milliseconds)));
                cfg.fading.addRangeValidatorFor_speed(form);

                form.addObjectGetterSetter(F("fdy"), cfg.fading, cfg.fading.get_bits_delay, cfg.fading.set_bits_delay);
                form.addFormUI(F("Delay Before Start Fading To Next Random Color"), FormUI::Suffix(FSPGM(seconds)));
                cfg.fading.addRangeValidatorFor_delay(form);

                form.add(F("fcf"), Color(cfg.fading.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                    if (store) {
                        cfg.fading.factor.value = Color::fromString(value);
                    }
                    return false;
                });
                form.addFormUI(F("Random Color Factor"));
            }
            break;
        case AnimationType::INTERLEAVED: {
                form.addObjectGetterSetter(F("ilr"), FormGetterSetter(cfg.interleaved, rows));
                form.addFormUI(F("Display every nth row"));
                // form.addValidator(FormUI::Validator::Range(0, IOT_LED_MATRIX_ROWS));

                form.addObjectGetterSetter(F("ilc"), FormGetterSetter(cfg.interleaved, cols));
                form.addFormUI(F("Display every nth column"));
                // form.addValidator(FormUI::Validator::Range(0, IOT_LED_MATRIX_COLS));

                form.addObjectGetterSetter(F("ilt"), FormGetterSetter(cfg.interleaved, time));
                form.addFormUI(F("Rotate Through Rows And Columns"), FormUI::Suffix(F("milliseconds")), FormUI::IntAttribute(F("disabled-value"), 0));
            }
            break;
        case AnimationType::FIRE: {
                form.addObjectGetterSetter(F("fic"), FormGetterSetter(cfg.fire, cooling));
                form.addFormUI(F("Cooling Value:"));
                cfg.fire.addRangeValidatorFor_cooling(form);

                form.addObjectGetterSetter(F("fis"), FormGetterSetter(cfg.fire, sparking));
                form.addFormUI(F("Sparking Value"));
                cfg.fire.addRangeValidatorFor_sparking(form);

                form.addObjectGetterSetter(F("fip"), FormGetterSetter(cfg.fire, speed));
                form.addFormUI(F("Speed"), FormUI::Suffix("milliseconds"));
                cfg.fire.addRangeValidatorFor_speed(form);

                auto &invertHidden = form.addObjectGetterSetter(F("fiinv"), FormGetterSetter(cfg.fire, invert_direction));
                form.addFormUI(FormUI::Type::HIDDEN);

                auto orientationItems = FormUI::List(
                    FireAnimationType::OrientationType::VERTICAL, F("Vertical"),
                    FireAnimationType::OrientationType::HORIZONTAL, F("Horizontal")
                );

                form.addObjectGetterSetter(F("fiori"), FormGetterSetter(cfg.fire, orientation));
                form.addFormUI(F("Orientation"), orientationItems, FormUI::CheckboxButtonSuffix(invertHidden, F("Invert Direction")));

                form.add(F("ficf"), Color(cfg.fire.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                    if (store) {
                        cfg.fire.factor.value = Color::fromString(value);
                    }
                    return false;
                });
                form.addFormUI(F("Color Factor"));
            }
            break;
        case AnimationType::FLASHING: {
                form.addObjectGetterSetter(F("fsp"), FormGetterSetter(cfg, flashing_speed));
                form.addFormUI(F("Flashing Speed"), FormUI::Suffix(FSPGM(milliseconds)));
                cfg.addRangeValidatorFor_flashing_speed(form);
            }
            break;
        case AnimationType::SOLID: {
                form.add(F("col"), Color(cfg.solid_color.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                    if (store) {
                        cfg.solid_color.value = Color::fromString(value);
                    }
                    return false;
                }, FormUI::Field::Type::TEXT);
                form.addFormUI(FSPGM(Solid_Color));
            }
            break;
        #if IOT_LED_MATRIX_ENABLE_VISUALIZER
            case AnimationType::VISUALIZER: {
                    SELECT_IRAM();

                    using OrientationType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::OrientationType;
                    using AudioInputType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::AudioInputType;
                    using VisualizerAnimationType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::VisualizerAnimationType;
                    using VisualizerPeakType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::VisualizerPeakType;
                    auto orientationItems = FormUI::List(
                        OrientationType::VERTICAL, F("Vertical"),
                        OrientationType::HORIZONTAL, F("Horizontal")
                    );
                    auto &orientation = form.addObjectGetterSetter(F("v_or"), cfg.visualizer, cfg.visualizer.get_bits_orientation, cfg.visualizer.set_bits_orientation);
                    form.addFormUI(FormUI::Type::HIDDEN_SELECT, orientationItems);

                    auto VisualizerAnimationTypeItems = FormUI::Container::List(
                        VisualizerAnimationType::VUMETER_1D, F("VU Meter Mono Gradient"),
                        VisualizerAnimationType::VUMETER_COLOR_1D, F("VU Meter Mono Color"),
                        VisualizerAnimationType::VUMETER_STEREO_1D, F("VU Meter Stereo Gradient"),
                        VisualizerAnimationType::VUMETER_COLOR_STEREO_1D, F("VU Meter Stereo Color"),
                        VisualizerAnimationType::SPECTRUM_RAINBOW_1D, F("Spectrum Rainbow 1D"),
                        VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D, F("Spectrum Rainbow Bars 2D"),
                        VisualizerAnimationType::SPECTRUM_GRADIENT_BARS_2D, F("Spectrum Gradient 2D"),
                        VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D, F("Spectrum Single Color Bars 2D"),
                        VisualizerAnimationType::RGB565_VIDEO, F("RGB565 Video"),
                        VisualizerAnimationType::RGB24_VIDEO, F("RGB24 Video")
                    );
                    form.addObjectGetterSetter(F("v_ln"), FormGetterSetter(cfg.visualizer, type));
                    form.addFormUI(F("Visualization Type"), VisualizerAnimationTypeItems, FormUI::SelectSuffix(orientation));

                    form.add(F("v_sc"), Color(cfg.visualizer.color).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                        if (store) {
                            cfg.visualizer.color = Color::fromString(value);
                        }
                        return false;
                    });
                    form.addFormUI(F("Color For Single Color Mode"));

                    auto showPeaksItems = FormUI::List(
                        VisualizerPeakType::DISABLED, F("Disabled"),
                        VisualizerPeakType::ENABLED, F("Enabled"),
                        VisualizerPeakType::FADING, F("Fading (time in ms) - recommended time is 1000-1500ms"),
                        VisualizerPeakType::FALLING_DOWN, F("Falling (time in ms) - recommended time is 2500-3000ms")
                    );
                    form.addObjectGetterSetter(F("v_psh"), cfg.visualizer, cfg.visualizer.get_bits_peak_show, cfg.visualizer.set_bits_peak_show);
                    form.addFormUI(F("Show Peaks"), FormUI::Type::SELECT, showPeaksItems);

                    auto &peakExtraColor = form.addObjectGetterSetter(F("v_pxc"), cfg.visualizer, cfg.visualizer.get_bits_peak_extra_color, cfg.visualizer.set_bits_peak_extra_color);
                    form.addFormUI(FormUI::Type::HIDDEN);

                    auto &peakFallingSpeed = form.addObjectGetterSetter(F("v_pfs"), cfg.visualizer, cfg.visualizer.get_bits_peak_falling_speed, cfg.visualizer.set_bits_peak_falling_speed);
                    // cfg.visualizer.addRangeValidatorFor_peak_falling_speed(form, true);
                    form.addFormUI(FormUI::Type::HIDDEN);

                    form.add(F("v_pc"), Color(cfg.visualizer.peak_color).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                        if (store) {
                            cfg.visualizer.peak_color = std::max<uint32_t>(1, Color::fromString(value));
                        }
                        return false;
                    });
                    form.addFormUI(F("Peak Color Indicator"), FormUI::CheckboxButtonSuffix(peakExtraColor, F("Use Color")), FormUI::TextInputSuffix(peakFallingSpeed));

                    auto &showLoudnessPeaks = form.addObjectGetterSetter(F("v_lnp"), cfg.visualizer, cfg.visualizer.get_bits_vumeter_peaks, cfg.visualizer.set_bits_vumeter_peaks);
                    form.addFormUI(FormUI::Type::HIDDEN, FormUI::BoolItems(F("Yes"), F("No")));

                    auto showVuMeterRowsItems = FormUI::List(
                        0, F("Disable"),
                        1, F("1 Row"),
                        2, F("2 Rows"),
                        3, F("3 Rows"),
                        4, F("4 Rows"),
                        5, F("5 Rows"),
                        6, F("6 Rows"),
                        7, F("7 Rows"),
                        8, F("8 Rows")
                    );
                    form.addObjectGetterSetter(F("v_sln"), cfg.visualizer, cfg.visualizer.get_bits_vumeter_rows, cfg.visualizer.set_bits_vumeter_rows);
                    form.addFormUI(F("Show VUMeter at the top"), FormUI::Type::SELECT, showVuMeterRowsItems, FormUI::CheckboxButtonSuffix(showLoudnessPeaks, F("Show Peaks")));

                    #if !IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT && !IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
                    #    error No audio input type defined
                    #endif

                    auto inputTypeItems = FormUI::List(
                        #if IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT
                            AudioInputType::UDP, F("UDP")
                        #endif
                        #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
                            #if IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT
                                ,
                            #endif
                            AudioInputType::MICROPHONE, F("Microphone")
                        #endif
                    );
                    form.addObjectGetterSetter(F("v_ait"), FormGetterSetter(cfg.visualizer, input));
                    form.addFormUI(F("Audio Input Source"), FormUI::Type::SELECT, inputTypeItems);

                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
                        form.addObjectGetterSetter(F("v_mlg"), cfg.visualizer, cfg.visualizer.get_bits_mic_loudness_gain, cfg.visualizer.set_bits_mic_loudness_gain);
                        form.addFormUI(F("Microphone Loudness Gain"));
                        cfg.visualizer.addRangeValidatorFor_mic_loudness_gain(form);

                        form.addObjectGetterSetter(F("v_mbg"), cfg.visualizer, cfg.visualizer.get_bits_mic_band_gain, cfg.visualizer.set_bits_mic_band_gain);
                        form.addFormUI(F("Microphone Band Gain"));
                        cfg.visualizer.addRangeValidatorFor_mic_band_gain(form);
                    #endif

                    auto &multicast = form.addObjectGetterSetter(F("v_muca"), FormGetterSetter(cfg.visualizer, multicast));
                    form.addFormUI(FormUI::Type::HIDDEN);

                    form.addObjectGetterSetter(F("v_port"), FormGetterSetter(cfg.visualizer, port));
                    form.addFormUI(F("UDP Port"), FormUI::CheckboxButtonSuffix(multicast, F("Multicast")));
                    cfg.visualizer.addRangeValidatorFor_port(form);
                }
                break;
        #endif
        case AnimationType::RAINBOW: {
                SELECT_IRAM();

                form.addObjectGetterSetter(F("rb_mul"), FormGetterSetter(cfg.rainbow.multiplier, value));
                form.addFormUI(FSPGM(Multiplier));
                cfg.rainbow.multiplier.addRangeValidatorFor_value(form);

                form.addObjectGetterSetter(F("rb_incr"), FormGetterSetter(cfg.rainbow.multiplier, incr));
                form.addFormUI(F("Multiplier Increment Per Frame"));
                cfg.rainbow.multiplier.addRangeValidatorFor_incr(form);

                form.addObjectGetterSetter(F("rb_min"), FormGetterSetter(cfg.rainbow.multiplier, min));
                form.addFormUI(F("Minimum Multiplier"));
                cfg.rainbow.multiplier.addRangeValidatorFor_min(form);

                form.addObjectGetterSetter(F("rb_max"), FormGetterSetter(cfg.rainbow.multiplier, max));
                form.addFormUI(F("Maximum Multiplier"));
                cfg.rainbow.multiplier.addRangeValidatorFor_max(form);

                form.addObjectGetterSetter(F("rb_sp"), FormGetterSetter(cfg.rainbow, speed));
                form.addFormUI(F("Animation Speed"));
                cfg.rainbow.addRangeValidatorFor_speed(form);

                form.add(F("rb_cf"), Color(cfg.rainbow.color.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                    if (store) {
                        cfg.rainbow.color.factor.value = Color::fromString(value);
                    }
                    return false;
                });
                form.addFormUI(F("Color Multiplier"));

                form.add(F("rb_mv"), Color(cfg.rainbow.color.min.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                    if (store) {
                        cfg.rainbow.color.min.value = Color::fromString(value);
                    }
                    return false;
                });
                form.addFormUI(F("Minimum Color Value"));

                form.addObjectGetterSetter(F("rb_cre"), FormGetterSetter(cfg.rainbow.color, red_incr));
                form.addFormUI(F("Color Increment Per Frame (Red)"));
                cfg.rainbow.color.addRangeValidatorFor_red_incr(form);

                form.addObjectGetterSetter(F("rb_cgr"), FormGetterSetter(cfg.rainbow.color, green_incr));
                form.addFormUI(F("Color Increment Per Frame (Green)"));
                cfg.rainbow.color.addRangeValidatorFor_green_incr(form);

                form.addObjectGetterSetter(F("rb_cbl"), FormGetterSetter(cfg.rainbow.color, blue_incr));
                form.addFormUI(F("Color Increment Per Frame (Blue)"));
                cfg.rainbow.color.addRangeValidatorFor_blue_incr(form);
            }
            break;
        case AnimationType::RAINBOW_FASTLED: {
                auto &invertHidden = form.addObjectGetterSetter(F("rb_dir"), FormGetterSetter(cfg.rainbow, invert_direction));
                form.addFormUI(FormUI::Type::HIDDEN);

                form.addObjectGetterSetter(F("rb_bpm"), FormGetterSetter(cfg.rainbow, bpm));
                form.addFormUI(F("BPM"), FormUI::CheckboxButtonSuffix(invertHidden, F("Invert Direction")));
                cfg.rainbow.addRangeValidatorFor_bpm(form);

                form.addObjectGetterSetter(F("rb_hue"), FormGetterSetter(cfg.rainbow, hue));
                form.addFormUI(F("Hue"));
                cfg.rainbow.addRangeValidatorFor_hue(form);
            }
            break;
        case AnimationType::GRADIENT: {
                form.addObjectGetterSetter(F("gd_speed"), FormGetterSetter(cfg.gradient, speed));
                form.addFormUI(F("Blending Speed"));
                cfg.gradient.addRangeValidatorFor_speed(form);

                PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_CLOCK_GRADIENT_ENTRIES, gd_c, gd_px);
                cfg.gradient.sort();
                for(uint8_t i = 0; i < cfg.gradient.kMaxEntries; i++) {

                    auto &pos = form.addCallbackGetterSetter<String>(F_VAR(gd_px, i), [&cfg, this, i](String &value, Field::BaseField &field, bool store) {
                        if (store) {
                            if (value.trim().length() == 0) {
                                cfg.gradient.entries[i].pixel = cfg.gradient.kDisabled;
                            }
                            else {
                                cfg.gradient.entries[i].pixel = static_cast<uint16_t>(value.toInt());
                                if (cfg.gradient.entries[i].pixel > _display.size() - 1) {
                                    cfg.gradient.entries[i].pixel = cfg.gradient.kDisabled;
                                }
                            }
                        }
                        else {
                            if (cfg.gradient.entries[i].pixel > _display.size() - 1) {
                                value = String();
                            }
                            else {
                                value = String(cfg.gradient.entries[i].pixel);
                            }
                        }
                        return true;
                    }, InputFieldType::TEXT);
                    form.addFormUI(FormUI::PlaceHolder(F("Disabled")), FormUI::Type::HIDDEN);
                    // form.addValidator(FormUI::Validator::Range(0, _display.size() - 1));

                    form.addCallbackGetterSetter<String>(F_VAR(gd_c, i), [&cfg, i](String &value, FormUI::Field::BaseField &field, bool store) {
                        if (store) {
                            cfg.gradient.entries[i].color = Color::fromString(value);
                        }
                        else {
                            value = Color(cfg.gradient.entries[i].color).toString();
                        }
                        return true;
                    });
                    form.addFormUI(F("Color / Pixel Position"), FormUI::TextInputSuffix(pos));
                }
            }
            break;
        case AnimationType::PLASMA: {
                form.addObjectGetterSetter(F("plmsp"), FormGetterSetter(cfg.plasma, speed));
                form.addFormUI(F("Speed"));
                cfg.plasma.addRangeValidatorFor_speed(form);

                form.addObjectGetterSetter(F("plma1"), FormGetterSetter(cfg.plasma, angle1));
                form.addFormUI(F("Angle 1"));
                cfg.plasma.addRangeValidatorFor_angle1(form);

                form.addObjectGetterSetter(F("plma2"), FormGetterSetter(cfg.plasma, angle2));
                form.addFormUI(F("Angle 2"));
                cfg.plasma.addRangeValidatorFor_angle2(form);

                form.addObjectGetterSetter(F("plma3"), FormGetterSetter(cfg.plasma, angle3));
                form.addFormUI(F("Angle 3"));
                cfg.plasma.addRangeValidatorFor_angle3(form);

                form.addObjectGetterSetter(F("plma4"), FormGetterSetter(cfg.plasma, angle4));
                form.addFormUI(F("Angle 4"));
                cfg.plasma.addRangeValidatorFor_angle4(form);

                form.addObjectGetterSetter(F("plmhs"), FormGetterSetter(cfg.plasma, hue_shift));
                form.addFormUI(F("Hue Shift"));
                cfg.plasma.addRangeValidatorFor_hue_shift(form);

                form.addObjectGetterSetter(F("plmxs"), FormGetterSetter(cfg.plasma, x_size));
                form.addFormUI(F("X Size"));
                cfg.plasma.addRangeValidatorFor_x_size(form);

                form.addObjectGetterSetter(F("plmys"), FormGetterSetter(cfg.plasma, y_size));
                form.addFormUI(F("Y Size"));
                cfg.plasma.addRangeValidatorFor_y_size(form);
            }
            break;
        case AnimationType::XMAS:
            {
                auto &invertHidden = form.addObjectGetterSetter(F("xdi"), FormGetterSetter(cfg.xmas, invert_direction));
                form.addFormUI(FormUI::Type::HIDDEN);

                form.addObjectGetterSetter(F("xsa"), FormGetterSetter(cfg.xmas, speed));
                form.addFormUI(F("Animation Speed"), FormUI::CheckboxButtonSuffix(invertHidden, F("Invert Direction")));
                cfg.xmas.addRangeValidatorFor_speed(form);

                form.addObjectGetterSetter(F("xfs"), FormGetterSetter(cfg.xmas, fade));
                form.addFormUI(F("Fade Speed"));
                cfg.xmas.addRangeValidatorFor_fade(form);

                form.addObjectGetterSetter(F("xsl"), FormGetterSetter(cfg.xmas, sparkling));
                form.addFormUI(F("Sparkle Intensity"));
                cfg.xmas.addRangeValidatorFor_sparkling(form);

                form.addObjectGetterSetter(F("xpx"), FormGetterSetter(cfg.xmas, pixels));
                form.addFormUI(F("LEDs per Pixel"));
                cfg.xmas.addRangeValidatorFor_pixels(form);

                form.addObjectGetterSetter(F("xgp"), FormGetterSetter(cfg.xmas, gap));
                form.addFormUI(F("Gap between Pixels"));
                cfg.xmas.addRangeValidatorFor_gap(form);

                auto paletteItems = FormUI::List(
                    0, F("Simple"),
                    1, F("Christmas Noel"),
                    2, F("Christmas Day Theme"),
                    3, F("Christmas Decorations"),
                    4, F("Christmas Snow")
                );
                form.addObjectGetterSetter(F("xcp"), FormGetterSetter(cfg.xmas, palette));
                form.addFormUI(F("Color Palette"), FormUI::Type::SELECT, paletteItems);
            }
            break;
        case AnimationType::MAX:
        #if !IOT_LED_MATRIX
            case AnimationType::COLON_SOLID:
            case AnimationType::COLON_BLINK_SLOWLY:
            case AnimationType::COLON_BLINK_FAST:
        #endif
            break;
    }

    if (group) {
        group->end();
    }
}

void ClockPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    __LDBG_printf("callback_type=%u name=%s", type, formName.c_str());

    // handle delayed saves
    if (type == FormCallbackType::SAVE) {
        // on save copy changes to local memory
        auto &cfg = Plugins::Clock::getWriteableConfig();
        _config = cfg;
        // remove timer, everything has been written already
        _Timer(_saveTimer).remove();
        return;
    }

    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Clock::getWriteableConfig();
    // handle delayed saves
    if (_saveTimer && memcmp(&cfg, &_config, sizeof(cfg)) != 0) {
        cfg = _config; // copy updates
    }

    // sub forms for the WebUI or split forms
    if (formName.startsWith(F("ani-"))) {
        auto subForm = formName.c_str() + 4;
        const auto animation = _getAnimationType(FPSTR(subForm)); // translate name to number or just return number
        const auto isInline = isdigit(subForm[0]); // inline form for webui requested
        __LDBG_printf("form=%s animation=%u valid=%u inline=%u", subForm, animation, animation < AnimationType::LAST, isInline);
        if (animation < AnimationType::LAST) {
            auto &ui = form.createWebUI();
            ui.setContainerId(F("led-matrix-settings"));
            ui.setStyle(isInline ? FormUI::WebUI::StyleType::WEBUI : FormUI::WebUI::StyleType::ACCORDION);
            _createConfigureFormAnimation(animation, form, cfg, isInline ? TitleType::SET_TITLE : TitleType::SET_TITLE_AND_ADD_GROUP);
            form.finalize();
        }
        return;
    }

    auto &ui = form.createWebUI();
    ui.setTitle(F(FORM_TITLE));
    ui.setContainerId(F("led-matrix-settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    #if ESP32
        if (formName == F("animations")) {

            // --------------------------------------------------------------------
            auto &animationGroup = form.addCardGroup(F("anicfg"), FSPGM(Animation), true);

            FormUI::Container::List animationTypeItems;
            for(int i = 0; i < static_cast<int>(AnimationType::MAX); i++) {
                animationTypeItems.emplace_back(static_cast<AnimationType>(i), _getAnimationName(static_cast<AnimationType>(i)));
            }

            form.addObjectGetterSetter(F("ani"), FormGetterSetter(cfg, animation));
            form.addFormUI(FSPGM(Type), animationTypeItems);

            _createConfigureFormAnimation(AnimationType::SOLID, form, cfg, TitleType::NONE);
            _createConfigureFormAnimation(AnimationType::FLASHING, form, cfg, TitleType::NONE);

            animationGroup.end();

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::RAINBOW, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::RAINBOW_FASTLED, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                _createConfigureFormAnimation(AnimationType::VISUALIZER, form, cfg, TitleType::ADD_GROUP);
            #endif

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::FIRE, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::PLASMA, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::INTERLEAVED, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::FADING, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::GRADIENT, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            _createConfigureFormAnimation(AnimationType::XMAS, form, cfg, TitleType::ADD_GROUP);

            // --------------------------------------------------------------------
            #if IOT_ALARM_PLUGIN_ENABLED

                auto &alarmGroup = form.addCardGroup(FSPGM(alarm), FSPGM(Alarm), true);

                form.add(F("acl"), Color(cfg.alarm.color.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                    if (store) {
                        cfg.alarm.color.value = Color::fromString(value);
                    }
                    return false;
                });
                form.addFormUI(FSPGM(Color));

                form.addObjectGetterSetter(F("asp"), FormGetterSetter(cfg.alarm, speed));
                form.addFormUI(F("Flashing Speed"), FormUI::Suffix(FSPGM(milliseconds)));
                form.addValidator(FormUI::Validator::Range(50, 0xffff));

                alarmGroup.end();

            #endif

        }
        else
    #endif
    if (formName == F("protection")) {

        // --------------------------------------------------------------------
        #if IOT_CLOCK_TEMPERATURE_PROTECTION
            auto &protectionGroup = form.addCardGroup(F("prot"), FSPGM(Protection), true);

            form.addObjectGetterSetter(F("tmi"), FormGetterSetter(cfg.protection.temperature_reduce_range, min));
            form.addFormUI(F("Minimum Temperature To Reduce Brightness"), FormUI::Suffix(FSPGM(UTF8_degreeC)));
            form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 85));

            form.addObjectGetterSetter(F("tma"), FormGetterSetter(cfg.protection.temperature_reduce_range, max));
            form.addFormUI(F("Maximum. Temperature To Reduce Brightness To 25%"), FormUI::Suffix(FSPGM(UTF8_degreeC)));
            form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 85));

            //TODO
            form.addObjectGetterSetter(F("tpm"), FormGetterSetter(cfg.protection, max_temperature));
            form.addFormUI(F("Over Temperature Protection"), FormUI::Suffix(FSPGM(UTF8_degreeC)));
            form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 105));

            #if IOT_LED_MATRIX

                form.addObjectGetterSetter(F("tpv"), FormGetterSetter(cfg.protection, regulator_margin));
                form.addFormUI(F("Extra Margin For Voltage Regulator"), FormUI::Suffix(FSPGM(UTF8_degreeC)));
                form.addValidator(FormUI::Validator::Range(-50, 50));

            #endif

            protectionGroup.end();
        #endif

        #if IOT_LED_MATRIX_FAN_CONTROL
            auto &fanGroup = form.addCardGroup(F("fan"), F("Fan"), true);

            form.addObjectGetterSetter(F("fs"), FormGetterSetter(cfg, fan_speed));
            form.addFormUI(F("Fan Speed"));
            form.addValidator(FormUI::Validator::Range(0, 255));
            //TODO validator

            form.addObjectGetterSetter(F("mif"), cfg, cfg.get_bits_min_fan_speed, cfg.set_bits_min_fan_speed);
            form.addFormUI(F("Minimum Fan Speed"));
            form.addValidator(FormUI::Validator::Range(0, 255));

            form.addObjectGetterSetter(F("maf"), cfg, cfg.get_bits_max_fan_speed, cfg.set_bits_max_fan_speed);
            form.addFormUI(F("Maximum Fan Speed"));
            form.addValidator(FormUI::Validator::Range(0, 255));

            fanGroup.end();
        #endif

        auto &powerGroup = form.addCardGroup(F("pow"), F("Power"), true);

        #if IOT_CLOCK_HAVE_POWER_LIMIT
            form.addObjectGetterSetter(F("pl"), FormGetterSetter(cfg, power_limit));
            form.addFormUI(F("Limit Maximum Power"), FormUI::Suffix(F("Watt")), FormUI::IntAttribute(F("disabled-value"), 0));
            cfg.addRangeValidatorFor_power_limit(form, true);
        #endif

        #if IOT_LED_MATRIX_GROUP_PIXELS

            auto groupPixelsItems = FormUI::List(
                0, F("None"),
                1, F("Group 3 LEDs, 33.3% brightness"),
                2, F("Group 9 LEDs, 11.1% brightness")
            );
            form.addObjectGetterSetter(F("pes"), FormGetterSetter(cfg, group_pixels));
            form.addFormUI(F("Group Pixels"), FormUI::Type::SELECT, groupPixelsItems);

        #endif

        form.addObjectGetterSetter(F("plr"), FormGetterSetter(cfg.power, red));
        form.addFormUI(F("Power Consumption For 256 LEDs at 100% Red"), FormUI::Suffix(F("mW")));
        cfg.power.addRangeValidatorFor_red(form);

        form.addObjectGetterSetter(F("plg"), FormGetterSetter(cfg.power, green));
        form.addFormUI(F("Power Consumption 100% Green"), FormUI::Suffix(F("mW")));
        cfg.power.addRangeValidatorFor_green(form);

        form.addObjectGetterSetter(F("plb"), FormGetterSetter(cfg.power, blue));
        form.addFormUI(F("Power Consumption 100% Blue"), FormUI::Suffix(F("mW")));
        cfg.power.addRangeValidatorFor_blue(form);

        form.addObjectGetterSetter(F("pli"), FormGetterSetter(cfg.power, idle));
        form.addFormUI(F("Power Consumption While Idle"), FormUI::Suffix(F("mW")));
        cfg.power.addRangeValidatorFor_idle(form);

        powerGroup.end();

    }
    #if IOT_LED_MATRIX_CONFIGURABLE
        else if (formName == F("matrix")) {

            auto &mainGroup = form.addCardGroup(F("matrix"));

            auto &reverseRows = form.addObjectGetterSetter(F("mx_rr"), FormGetterSetter(cfg.matrix, reverse_rows));
            form.addFormUI(FormUI::Type::HIDDEN);

            form.addObjectGetterSetter(F("mx_rows"), FormGetterSetter(cfg.matrix, rows));
            form.addFormUI(F("Rows"), FormUI::CheckboxButtonSuffix(reverseRows, F("Reverse Rows")));
            cfg.matrix.addRangeValidatorFor_rows(form);

            auto &reverseCols = form.addObjectGetterSetter(F("mx_rc"), FormGetterSetter(cfg.matrix, reverse_cols));
            form.addFormUI(FormUI::Type::HIDDEN);

            form.addObjectGetterSetter(F("mx_cols"), FormGetterSetter(cfg.matrix, cols));
            form.addFormUI(F("Columns"), FormUI::CheckboxButtonSuffix(reverseCols, F("Reverse Columns")));
            cfg.matrix.addRangeValidatorFor_cols(form);

            FormUI::Validator::CallbackTemplate<uint16_t> *validator;
            validator = &form.addValidator(FormUI::Validator::CallbackTemplate<uint16_t>([&cfg, &validator, this](uint16_t cols, Field::BaseField &field) {
                auto rows = field.getForm().getField(F("mx_rows"))->getValue().toInt();
                if ((rows * cols) != 0 && (rows * cols) <= static_cast<long>(_display.getMaxNumPixels())) {
                    return true;
                }
                validator->setMessage(PrintString(F("rows * cols (%u * %u = %u) exceeds maximum number of pixels (%u)"),
                    static_cast<unsigned>(rows), static_cast<unsigned>(cols), static_cast<unsigned>(rows * cols), static_cast<unsigned>(_display.getMaxNumPixels()))
                );
                return false;
            }));

            form.addObjectGetterSetter(F("mx_rf"), FormGetterSetter(cfg.matrix, rowOfs));
            form.addFormUI(F("Row Offset"));
            cfg.matrix.addRangeValidatorFor_rowOfs(form);

            form.addObjectGetterSetter(F("mx_cf"), FormGetterSetter(cfg.matrix, colOfs));
            form.addFormUI(F("Column Offset"));
            cfg.matrix.addRangeValidatorFor_colOfs(form);

            form.addObjectGetterSetter(F("mx_rt"), FormGetterSetter(cfg.matrix, rotate));
            form.addFormUI(F("90\xc2\xb0 Rotation"), FormUI::BoolItems());

            form.addObjectGetterSetter(F("mx_il"), FormGetterSetter(cfg.matrix, interleaved));
            form.addFormUI(F("Interleaved"), FormUI::BoolItems());

            form.add(F("mx_px"), String(IOT_CLOCK_NUM_PIXELS));
            form.addFormUI(F("Maximum Number Of Pixels"), FormUI::ReadOnlyAttribute());

            form.addObjectGetterSetter(F("mx_px0"), FormGetterSetter(cfg.matrix, pixels0));
            form.addFormUI(F("Segment 1 Pixels Pin #" _STRINGIFY(IOT_LED_MATRIX_OUTPUT_PIN)));
            form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_pixels0, cfg.matrix.kMaxValueFor_pixels0));

            form.addObjectGetterSetter(F("mx_ofs0"), FormGetterSetter(cfg.matrix, offset0));
            form.addFormUI(F("Segment 1 Offset"));
            form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_offset0, cfg.matrix.kMaxValueFor_offset0));

            #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1

                form.addObjectGetterSetter(F("mx_px1"), FormGetterSetter(cfg.matrix, pixels1));
                form.addFormUI(F("Segment 2 Pixels Pin #" _STRINGIFY(IOT_LED_MATRIX_OUTPUT_PIN1)));
                form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_pixels1, cfg.matrix.kMaxValueFor_pixels1));

                form.addObjectGetterSetter(F("mx_ofs1"), FormGetterSetter(cfg.matrix, offset1));
                form.addFormUI(F("Segment 2 Offset"));
                form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_offset1, cfg.matrix.kMaxValueFor_offset1));

            #endif

            #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1

                form.addObjectGetterSetter(F("mx_px2"), FormGetterSetter(cfg.matrix, pixels2));
                form.addFormUI(F("Segment 3 Pixels Pin #" _STRINGIFY(IOT_LED_MATRIX_OUTPUT_PIN2)));
                form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_pixels2, cfg.matrix.kMaxValueFor_pixels2));

                form.addObjectGetterSetter(F("mx_ofs2"), FormGetterSetter(cfg.matrix, offset2));
                form.addFormUI(F("Segment 3 Offset"));
                form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_offset2, cfg.matrix.kMaxValueFor_offset2));

            #endif

            #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1

                form.addObjectGetterSetter(F("mx_px3"), FormGetterSetter(cfg.matrix, pixels3));
                form.addFormUI(F("Segment 3 Pixels Pin #" _STRINGIFY(IOT_LED_MATRIX_OUTPUT_PIN3)));
                form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_pixels3, cfg.matrix.kMaxValueFor_pixels3));

                form.addObjectGetterSetter(F("mx_ofs3"), FormGetterSetter(cfg.matrix, offset3));
                form.addFormUI(F("Segment 3 Offset"));
                form.addValidator(FormUI::Validator::Range(cfg.matrix.kMinValueFor_offset3, cfg.matrix.kMaxValueFor_offset3));

            #endif

            // validator = &form.addValidator(FormUI::Validator::CallbackTemplate<uint16_t>([&cfg, &validator, this](uint16_t offset, Field::BaseField &field) {
            //     auto rows = field.getForm().getField(F("mx_rows"))->getValue().toInt();
            //     auto cols = field.getForm().getField(F("mx_cols"))->getValue().toInt();
            //     if ((rows * cols) + offset <= static_cast<long>(_display.getMaxNumPixels())) {
            //         return true;
            //     }
            //     validator->setMessage(PrintString(F("offset + (rows * cols) = %u + (%u * %u) = %u exceeds exceeds maximum number of pixels (%u)"),
            //         static_cast<unsigned>(offset),
            //         static_cast<unsigned>(rows),
            //         static_cast<unsigned>(cols),
            //         static_cast<unsigned>((rows * cols) + offset),
            //         static_cast<unsigned>(_display.getMaxNumPixels()))
            //     );
            //     return false;
            // }));

            mainGroup.end();

        }
    #endif
    else {

        auto &mainGroup = form.addCardGroup(FSPGM(config));

        // --------------------------------------------------------------------
        auto initialStateItems = FormUI::Container::List(
            Clock::InitialStateType::OFF, F("Turn Off"),
            Clock::InitialStateType::ON, F("Turn On"),
            Clock::InitialStateType::RESTORE, F("Restore Last State")
        );

        form.addObjectGetterSetter(F("is"), cfg, cfg.get_int_initial_state, cfg.set_int_initial_state);
        form.addFormUI(F("After Reset"), initialStateItems);

        form.addObjectGetterSetter(F("br"), cfg, cfg.get_bits_brightness, cfg.set_bits_brightness);
        form.addFormUI(FormUI::Type::RANGE_SLIDER, FSPGM(Brightness), FormUI::MinMax(cfg.kMinValueFor_brightness, cfg.kMaxValueFor_brightness));

        form.addObjectGetterSetter(F("ft"), FormGetterSetter(cfg, fading_time));
        form.addFormUI(F("Fading Time From 0 To 100%"), FormUI::Suffix(FSPGM(seconds)));
        cfg.addRangeValidatorFor_fading_time(form, true);

        #if IOT_LED_MATRIX == 0
            form.addObjectGetterSetter(F("tfmt"), FormGetterSetter(cfg, time_format_24h));
            form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

            form.addObjectGetterSetter(F("csp"), FormGetterSetter(cfg, blink_colon_speed));
            form.addFormUI(F("Colon Blink Speed"), FormUI::Suffix(F("milliseconds")), FormUI::IntAttribute(F("disabled-value"), 0), FormUI::FPStringAttribute(F("disabled-value-placeholder"), F("Solid")));
            cfg.addRangeValidatorFor_blink_colon_speed(form, true);
        #endif

        #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT || IOT_LED_MATRIX_NEOPIXEL_SUPPORT

            auto displayMethodItems = FormUI::Container::List(
                Clock::ShowMethodType::FASTLED, F("FastLED")
                #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
                    , Clock::ShowMethodType::NEOPIXEL_EX, F("NeoPixelEx")
                #endif
                #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                    , Clock::ShowMethodType::AF_NEOPIXEL, F("Adafruit NeoPixel")
                #endif
            );

            form.addObjectGetterSetter(F("dm"), FormGetterSetter(cfg, method));
            form.addFormUI(F("Display Method"), displayMethodItems);

        #endif

        form.addObjectGetterSetter(F("dt"), FormGetterSetter(cfg, dithering));
        form.addFormUI(F("FastLED Temporal Dithering"), FormUI::BoolItems(F("Enable"), F("Disable")));

        #if IOT_LED_MATRIX_STANDBY_PIN != -1
            form.addObjectGetterSetter(F("sbl"), cfg, cfg.get_bits_standby_led, cfg.set_bits_standby_led);
            form.addFormUI(F("Standby LED/Relay or MOSFET"), FormUI::BoolItems(F("Enable"), F("Disable")));
        #endif

        mainGroup.end();

    }

    form.finalize();
}
