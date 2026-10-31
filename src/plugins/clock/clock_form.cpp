/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <KFCForms.h>
#include "../src/plugins/sensor/sensor.h"
#include "Utility/ProgMemHelper.h"
#include "animation.h"
#include "matrix_validation.h"
#include <HeapSelector.h>
#include "templates.h"
#include "../src/plugins/mqtt/mqtt_json.h"

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

void ClockPlugin::_saveStateDeferred()
{
    _enqueue([this] { _saveState(); });
}

void ClockPlugin::_saveState()
{
    #if ESP8266
        constexpr uint32_t kSaveDelay = 10000;
    #else
        constexpr uint32_t kSaveDelay = 5000;
    #endif
    // the storage has to match _config immediately, form callbacks (web server task) read it and
    // the callback below only writes it to flash
    Plugins::Clock::setConfig(_config);
    // delay writing config
    _Timer(_saveTimer).add(Event::milliseconds(kSaveDelay), false, [this](Event::CallbackTimerPtr) {
        __LDBG_printf("delay save dirty=%u", config.isDirty());
        if (config.isDirty()) {
            config.write();
        }
    });
}

void ClockPlugin::_setStateDeferred(bool state, bool autoOff)
{
    _enqueue([this, state, autoOff] { _setState(state, autoOff); });
}

void ClockPlugin::_setState(bool state, bool autoOff)
{
    #if IOT_SENSOR_HAVE_MOTION_SENSOR
        _motionAutoOff = autoOff;
    #endif
    if (state) {
        if (_targetBrightness == 0) {
            if (_savedBrightness) {
                _setBrightness(_savedBrightness);
            }
            else {
                _setBrightness(_config.getBrightness());
            }
        }
    }
    else {
        if (_targetBrightness != 0) {
            _savedBrightness = _targetBrightness;
        }
        _setBrightness(0);
    }
    _saveState();
}

void ClockPlugin::_createConfigureFormAnimation(AnimationType animation, FormUI::Form::BaseForm &form, ClockConfigType &cfg, TitleType titleType)
{
    FormUI::Group *group = nullptr;
    switch(titleType) {
        case TitleType::SET_TITLE_AND_ADD_GROUP: {
                // standalone page: the plugin name is the form title, the animation is the card
                form.getWebUIConfig().setTitle(F(FORM_TITLE));
                auto title = _getAnimationTitle(animation);
                #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                    // every visualizer mode is configured in the same card
                    if (ClockConfigType::getVisualizerType(animation) != VisualizerAnimationType::MAX) {
                        title = F("Visualizer");
                    }
                #endif
                group = &form.addCardGroup(_getAnimationNameSlug(animation), title, true);
            }
            break;
        case TitleType::SET_TITLE:
            // inline form for the WebUI: the animation is the form title, no card group
            form.getWebUIConfig().setTitle(_getAnimationTitle(animation));
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
                form.addFormUI(F("Color Correction"));
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
            // every visualizer mode shares the same form, the animation selects the sub type
            case AnimationType::VISUALIZER_SPECTRUM_RAINBOW_BARS:
            case AnimationType::VISUALIZER_SPECTRUM_GRADIENT_BARS:
            case AnimationType::VISUALIZER_SPECTRUM_SINGLE_COLOR_BARS:
            case AnimationType::VISUALIZER_PLASMA_REACTIVE:
            case AnimationType::VISUALIZER_FIRE_REACTIVE: {
                    SELECT_IRAM();

                    using OrientationType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::OrientationType;
                    using AudioInputType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::AudioInputType;
                    using VisualizerAnimationType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::VisualizerAnimationType;
                    using VisualizerPeakType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::VisualizerPeakType;
                    auto orientationItems = FormUI::List(
                        OrientationType::HORIZONTAL, F("Horizontal (spectrum from left to right)"),
                        OrientationType::VERTICAL, F("Vertical (spectrum from bottom to top)")
                    );

                    auto VisualizerAnimationTypeItems = FormUI::Container::List(
                        VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D, F("Spectrum Rainbow Bars 2D"),
                        VisualizerAnimationType::SPECTRUM_GRADIENT_BARS_2D, F("Spectrum Gradient 2D"),
                        VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D, F("Spectrum Single Color Bars 2D"),
                        VisualizerAnimationType::PLASMA_AUDIO, F("Plasma Audio Reactive"),
                        VisualizerAnimationType::FIRE_AUDIO, F("Fire Audio Reactive")
                    );
                    // changing the mode also updates the animation (they are two controls for one value)
                    form.addObjectGetterSetter(F("v_ln"), FormGetterSetter(cfg, visualizer_type));
                    form.addFormUI(F("Visualization Type"), VisualizerAnimationTypeItems);

                    // the common audio options are shared by every visualization type
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

                    // the fields of the visualization types are added inside groups that are shown and hidden by forms.js
                    // (form-dependency-group, see Form::BaseForm::addDivGroup). fields outside of a group are always visible

                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT
                        auto &visualizerUdpGroup = form.addDivGroup(F("v_grp_udp"), PrintString(
                            F("{'i':'#v_ait','s':{'%d':'$T.show()'},'m':'$T.hide()'}"),
                            static_cast<int>(AudioInputType::UDP)
                        ));

                        auto &multicast = form.addObjectGetterSetter(F("v_muca"), FormGetterSetter(cfg.visualizer, multicast));
                        form.addFormUI(FormUI::Type::HIDDEN);

                        form.addObjectGetterSetter(F("v_port"), FormGetterSetter(cfg.visualizer, port));
                        form.addFormUI(F("UDP Port"), FormUI::CheckboxButtonSuffix(multicast, F("Multicast")));
                        cfg.visualizer.addRangeValidatorFor_port(form);

                        visualizerUdpGroup.end();
                    #endif

                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
                        auto &visualizerMicGroup = form.addDivGroup(F("v_grp_mic"), PrintString(
                            F("{'i':'#v_ait','s':{'%d':'$T.show()'},'m':'$T.hide()'}"),
                            static_cast<int>(AudioInputType::MICROPHONE)
                        ));

                        form.addObjectGetterSetter(F("v_mag"), FormGetterSetter(cfg.visualizer, mic_agc));
                        form.addFormUI(F("Automatic Gain"), FormUI::BoolItems());

                        // the automatic gain shows the range below the loudest band, the gains are not used
                        auto &visualizerMicAgcGroup = form.addDivGroup(F("v_grp_agc"), F("{'i':'#v_mag','s':{'1':'$T.show()'},'m':'$T.hide()'}"));

                        form.addObjectGetterSetter(F("v_msl"), FormGetterSetter(cfg.visualizer, mic_silence));
                        form.addFormUI(F("Silence Level (-dBFS)"));
                        cfg.visualizer.addRangeValidatorFor_mic_silence(form);

                        form.addObjectGetterSetter(F("v_mrg"), FormGetterSetter(cfg.visualizer, mic_range));
                        form.addFormUI(F("Dynamic Range (dB)"));
                        cfg.visualizer.addRangeValidatorFor_mic_range(form);

                        visualizerMicAgcGroup.end();

                        auto &visualizerMicGainGroup = form.addDivGroup(F("v_grp_mgn"), F("{'i':'#v_mag','s':{'0':'$T.show()'},'m':'$T.hide()'}"));

                        form.addObjectGetterSetter(F("v_mlg"), cfg.visualizer, cfg.visualizer.get_bits_mic_loudness_gain, cfg.visualizer.set_bits_mic_loudness_gain);
                        form.addFormUI(F("Microphone Loudness Gain"));
                        cfg.visualizer.addRangeValidatorFor_mic_loudness_gain(form);

                        form.addObjectGetterSetter(F("v_mbg"), cfg.visualizer, cfg.visualizer.get_bits_mic_band_gain, cfg.visualizer.set_bits_mic_band_gain);
                        form.addFormUI(F("Microphone Band Gain"));
                        cfg.visualizer.addRangeValidatorFor_mic_band_gain(form);

                        visualizerMicGainGroup.end();

                        visualizerMicGroup.end();
                    #endif

                    // the audio reaction is shared by the audio reactive plasma and fire
                    auto &visualizerReactionGroup = form.addDivGroup(F("v_grp_rct"), PrintString(
                        F("{'i':'#v_ln','s':{'%d':'$T.show()','%d':'$T.show()'},'m':'$T.hide()'}"),
                        static_cast<int>(VisualizerAnimationType::PLASMA_AUDIO),
                        static_cast<int>(VisualizerAnimationType::FIRE_AUDIO)
                    ));

                    form.addObjectGetterSetter(F("v_ars"), FormGetterSetter(cfg.visualizer.audio_reaction, sensitivity));
                    form.addFormUI(F("Audio Sensitivity (Gain x100)"));
                    cfg.visualizer.audio_reaction.addRangeValidatorFor_sensitivity(form);

                    form.addObjectGetterSetter(F("v_ara"), FormGetterSetter(cfg.visualizer.audio_reaction, attack));
                    form.addFormUI(F("Attack Time (ms)"));
                    cfg.visualizer.audio_reaction.addRangeValidatorFor_attack(form);

                    form.addObjectGetterSetter(F("v_are"), FormGetterSetter(cfg.visualizer.audio_reaction, release));
                    form.addFormUI(F("Release Time (ms)"));
                    cfg.visualizer.audio_reaction.addRangeValidatorFor_release(form);

                    visualizerReactionGroup.end();

                    // spectrum bars
                    auto &visualizerSpectrumGroup = form.addDivGroup(F("v_grp_spec"), PrintString(
                        F("{'i':'#v_ln','s':{'%d':'$T.show()','%d':'$T.show()','%d':'$T.show()'},'m':'$T.hide()'}"),
                        static_cast<int>(VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D),
                        static_cast<int>(VisualizerAnimationType::SPECTRUM_GRADIENT_BARS_2D),
                        static_cast<int>(VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D)
                    ));

                    // the bars and the VU meter can be drawn horizontally or vertically (transposed)
                    form.addObjectGetterSetter(F("v_or"), cfg.visualizer, cfg.visualizer.get_bits_orientation, cfg.visualizer.set_bits_orientation);
                    form.addFormUI(F("Orientation"), orientationItems);

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

                    visualizerSpectrumGroup.end();

                    // single color spectrum bars
                    auto &visualizerColorGroup = form.addDivGroup(F("v_grp_col"), PrintString(
                        F("{'i':'#v_ln','s':{'%d':'$T.show()'},'m':'$T.hide()'}"),
                        static_cast<int>(VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D)
                    ));

                    form.add(F("v_sc"), Color(cfg.visualizer.color).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                        if (store) {
                            cfg.visualizer.color = Color::fromString(value);
                        }
                        return false;
                    });
                    form.addFormUI(F("Color For Single Color Mode"));

                    visualizerColorGroup.end();

                    // audio reactive plasma, all parameters are independent from the plasma animation
                    auto &visualizerPlasmaGroup = form.addDivGroup(F("v_grp_plm"), PrintString(
                        F("{'i':'#v_ln','s':{'%d':'$T.show()'},'m':'$T.hide()'}"),
                        static_cast<int>(VisualizerAnimationType::PLASMA_AUDIO)
                    ));

                    auto &plasmaAudioSpeedEnable = form.addObjectGetterSetter(F("v_pase"), FormGetterSetter(cfg.visualizer.plasma_audio, enable_speed));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_pasg"), FormGetterSetter(cfg.visualizer.plasma_audio, speed_gain));
                    form.addFormUI(F("Plasma Speed Reaction"), FormUI::CheckboxButtonSuffix(plasmaAudioSpeedEnable, F("Enabled")));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_speed_gain(form);

                    auto &plasmaAudioHueEnable = form.addObjectGetterSetter(F("v_pahe"), FormGetterSetter(cfg.visualizer.plasma_audio, enable_hue));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_pahg"), FormGetterSetter(cfg.visualizer.plasma_audio, hue_gain));
                    form.addFormUI(F("Plasma Hue Rotation"), FormUI::CheckboxButtonSuffix(plasmaAudioHueEnable, F("Enabled")));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_hue_gain(form);

                    auto &plasmaAudioZoomEnable = form.addObjectGetterSetter(F("v_paze"), FormGetterSetter(cfg.visualizer.plasma_audio, enable_zoom));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_pazg"), FormGetterSetter(cfg.visualizer.plasma_audio, zoom_gain));
                    form.addFormUI(F("Plasma Zoom Reaction"), FormUI::CheckboxButtonSuffix(plasmaAudioZoomEnable, F("Enabled")));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_zoom_gain(form);

                    auto &plasmaAudioBandsEnable = form.addObjectGetterSetter(F("v_pabe"), FormGetterSetter(cfg.visualizer.plasma_audio, enable_bands));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_pabg"), FormGetterSetter(cfg.visualizer.plasma_audio, band_gain));
                    form.addFormUI(F("Plasma Spectrum Ripples (Bass, Low Mid, High Mid, Treble)"), FormUI::CheckboxButtonSuffix(plasmaAudioBandsEnable, F("Enabled")));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_band_gain(form);

                    form.addObjectGetterSetter(F("v_pasp"), FormGetterSetter(cfg.visualizer.plasma_audio, speed));
                    form.addFormUI(F("Plasma Speed"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_speed(form);

                    form.addObjectGetterSetter(F("v_paa1"), FormGetterSetter(cfg.visualizer.plasma_audio, angle1));
                    form.addFormUI(F("Plasma Angle 1"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_angle1(form);

                    form.addObjectGetterSetter(F("v_paa2"), FormGetterSetter(cfg.visualizer.plasma_audio, angle2));
                    form.addFormUI(F("Plasma Angle 2"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_angle2(form);

                    form.addObjectGetterSetter(F("v_paa3"), FormGetterSetter(cfg.visualizer.plasma_audio, angle3));
                    form.addFormUI(F("Plasma Angle 3"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_angle3(form);

                    form.addObjectGetterSetter(F("v_paa4"), FormGetterSetter(cfg.visualizer.plasma_audio, angle4));
                    form.addFormUI(F("Plasma Angle 4"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_angle4(form);

                    form.addObjectGetterSetter(F("v_pahs"), FormGetterSetter(cfg.visualizer.plasma_audio, hue_shift));
                    form.addFormUI(F("Plasma Hue Shift"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_hue_shift(form);

                    form.addObjectGetterSetter(F("v_paxs"), FormGetterSetter(cfg.visualizer.plasma_audio, x_size));
                    form.addFormUI(F("Plasma X Size"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_x_size(form);

                    form.addObjectGetterSetter(F("v_pays"), FormGetterSetter(cfg.visualizer.plasma_audio, y_size));
                    form.addFormUI(F("Plasma Y Size"));
                    cfg.visualizer.plasma_audio.addRangeValidatorFor_y_size(form);

                    visualizerPlasmaGroup.end();

                    // audio reactive fire, all parameters are independent from the fire animation
                    auto &visualizerFireGroup = form.addDivGroup(F("v_grp_fir"), PrintString(
                        F("{'i':'#v_ln','s':{'%d':'$T.show()'},'m':'$T.hide()'}"),
                        static_cast<int>(VisualizerAnimationType::FIRE_AUDIO)
                    ));

                    auto &fireSpeedEnable = form.addObjectGetterSetter(F("v_fase"), FormGetterSetter(cfg.visualizer.fire_audio, enable_speed));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_fasg"), FormGetterSetter(cfg.visualizer.fire_audio, speed_gain));
                    form.addFormUI(F("Fire Speed Reaction"), FormUI::CheckboxButtonSuffix(fireSpeedEnable, F("Enabled")));
                    cfg.visualizer.fire_audio.addRangeValidatorFor_speed_gain(form);

                    auto &fireHeatEnable = form.addObjectGetterSetter(F("v_fahe"), FormGetterSetter(cfg.visualizer.fire_audio, enable_heat));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_fahg"), FormGetterSetter(cfg.visualizer.fire_audio, heat_gain));
                    form.addFormUI(F("Fire Heat Reaction"), FormUI::CheckboxButtonSuffix(fireHeatEnable, F("Enabled")));
                    cfg.visualizer.fire_audio.addRangeValidatorFor_heat_gain(form);

                    auto &fireSparksEnable = form.addObjectGetterSetter(F("v_fape"), FormGetterSetter(cfg.visualizer.fire_audio, enable_sparks));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_fapg"), FormGetterSetter(cfg.visualizer.fire_audio, spark_gain));
                    form.addFormUI(F("Fire Spark Reaction"), FormUI::CheckboxButtonSuffix(fireSparksEnable, F("Enabled")));
                    cfg.visualizer.fire_audio.addRangeValidatorFor_spark_gain(form);

                    auto &fireBandsEnable = form.addObjectGetterSetter(F("v_fabe"), FormGetterSetter(cfg.visualizer.fire_audio, enable_bands));
                    form.addFormUI(FormUI::Type::HIDDEN);
                    form.addObjectGetterSetter(F("v_fabg"), FormGetterSetter(cfg.visualizer.fire_audio, band_gain));
                    form.addFormUI(F("Fire Spectrum Ripples"), FormUI::CheckboxButtonSuffix(fireBandsEnable, F("Enabled")));
                    cfg.visualizer.fire_audio.addRangeValidatorFor_band_gain(form);

                    using FireDirectionType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::FireAudioType::DirectionType;
                    auto fireDirectionItems = FormUI::List(
                        FireDirectionType::HORIZONTAL, F("Horizontal (flames move sideways)"),
                        FireDirectionType::HORIZONTAL_FLIPPED, F("Horizontal, flipped (opposite direction)"),
                        FireDirectionType::VERTICAL, F("Vertical (flames rise)"),
                        FireDirectionType::VERTICAL_FLIPPED, F("Vertical, flipped (flames move down)")
                    );

                    form.addObjectGetterSetter(F("v_faor"), FormGetterSetter(cfg.visualizer.fire_audio, direction));
                    form.addFormUI(F("Fire Direction"), fireDirectionItems);

                    form.addObjectGetterSetter(F("v_fasd"), FormGetterSetter(cfg.visualizer.fire_audio, speed));
                    form.addFormUI(F("Fire Update Rate (ms)"));
                    cfg.visualizer.fire_audio.addRangeValidatorFor_speed(form);

                    form.addObjectGetterSetter(F("v_fac"), FormGetterSetter(cfg.visualizer.fire_audio, cooling));
                    form.addFormUI(F("Fire Cooling"));
                    cfg.visualizer.fire_audio.addRangeValidatorFor_cooling(form);

                    form.addObjectGetterSetter(F("v_fasp"), FormGetterSetter(cfg.visualizer.fire_audio, sparking));
                    form.addFormUI(F("Fire Sparking"));
                    cfg.visualizer.fire_audio.addRangeValidatorFor_sparking(form);

                    form.add(F("v_faf"), Color(cfg.visualizer.fire_audio.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                        if (store) {
                            cfg.visualizer.fire_audio.factor.value = Color::fromString(value);
                        }
                        return false;
                    });
                    form.addFormUI(F("Fire Color Correction"));

                    visualizerFireGroup.end();
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
                            if (StrWrapper(value).trim().length() == 0) {
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

#if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1

void ClockPlugin::_createConfigureFormIRRemote(FormUI::Form::BaseForm &form, ClockConfigType &cfg)
{
    using IRRemoteConfigType = KFCConfigurationClasses::Plugins::ClockConfigNS::IRRemoteConfigType;
    using ActionType = IRRemoteConfigType::ActionType;

    // codes are hex values, an empty field means that the button is not assigned.
    // The Learn button captures the code of a button press (see Resources/js/forms/led-matrix.js).
    // addFormUI() replaces the UI of the last field, label/suffix must be added in one call
    auto addCodeField = [&form, &cfg](const __FlashStringHelper *name, ActionType action, const FormUI::Label &label) {
        form.addCallbackGetterSetter<String>(name, [&cfg, action](String &value, FormUI::Field::BaseField &field, bool store) {
            if (store) {
                StrWrapper(value).trim();
                cfg.ir.setCode(action, value.length() ? static_cast<uint32_t>(strtoul(value.c_str(), nullptr, 16)) : IRRemoteConfigType::kNoCode);
            }
            else {
                auto code = cfg.ir.getCode(action);
                value = code ? PrintString(F("%08x"), code) : String();
            }
            return true;
        }, InputFieldType::TEXT);
        form.addFormUI(label, FormUI::PlaceHolder(F("not assigned")), FormUI::SuffixHtml(F("<button type=\"button\" class=\"btn btn-default ir_remote_learn\" title=\"Capture the code of a button press\"><span class=\"oi oi-target\"></span> Learn</button>")));
    };

    auto &group = form.addCardGroup(F("ircfg"), F("IR Remote"), true);

    form.addObjectGetterSetter(F("ir_en"), FormGetterSetter(cfg.ir, enabled));
    form.addFormUI(F("Enable IR Remote"), FormUI::BoolItems(F("Enable"), F("Disable")));

    addCodeField(F("ir_pwr"), ActionType::POWER, F("Power On/Off"));
    addCodeField(F("ir_nxt"), ActionType::NEXT_ANIMATION, F("Next Animation"));
    addCodeField(F("ir_bup"), ActionType::BRIGHTNESS_UP, F("Brightness Up (hold to ramp)"));
    addCodeField(F("ir_bdn"), ActionType::BRIGHTNESS_DOWN, F("Brightness Down (hold to ramp)"));

    form.addObjectGetterSetter(F("ir_stp"), FormGetterSetter(cfg.ir, step));
    form.addFormUI(F("Color Step Per Press"));
    cfg.ir.addRangeValidatorFor_step(form);

    // icd is indexed by the ActionType, icv by the color button and irr by the row of color buttons
    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, static_cast<uint8_t>(ActionType::MAX), icd);
    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IRRemoteConfigType::kNumColors, icv);
    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IRRemoteConfigType::kNumColors / 4, irr);

    static const char *const kChannelNames[IRRemoteConfigType::kNumChannels] = { "Red", "Green", "Blue" };

    // the color channels, a button held down ramps the level
    for(uint8_t channel = 0; channel < IRRemoteConfigType::kNumChannels; channel++) {
        const auto up = static_cast<ActionType>(static_cast<uint8_t>(ActionType::RED_UP) + channel);
        const auto down = static_cast<ActionType>(static_cast<uint8_t>(ActionType::RED_DOWN) + channel);

        addCodeField(F_VAR(icd, static_cast<uint8_t>(up)), up, FormUI::Label(PrintString(F("%s Up"), kChannelNames[channel])));
        addCodeField(F_VAR(icd, static_cast<uint8_t>(down)), down, FormUI::Label(PrintString(F("%s Down"), kChannelNames[channel])));
    }

    // color buttons, 5 rows with 4 buttons like the remote
    for(uint8_t row = 0; row < IRRemoteConfigType::kNumColors / 4; row++) {
        auto &rowGroup = form.addCardGroup(F_VAR(irr, row), PrintString(F("Color Buttons %u"), row + 1), false);
        for(uint8_t col = 0; col < 4; col++) {
            const uint8_t index = row * 4 + col;
            const auto action = static_cast<ActionType>(static_cast<uint8_t>(ActionType::COLOR_1) + index);

            addCodeField(F_VAR(icd, static_cast<uint8_t>(action)), action, FormUI::Label(PrintString(F("Button %u Code"), col + 1)));

            form.addCallbackGetterSetter<String>(F_VAR(icv, index), [&cfg, action](String &value, FormUI::Field::BaseField &field, bool store) {
                if (store) {
                    cfg.ir.setColor(action, Color::fromString(value).get());
                }
                else {
                    value = Color(cfg.ir.getColor(action)).toString();
                }
                return true;
            }, InputFieldType::TEXT);
            form.addFormUI(FormUI::Label(PrintString(F("Button %u Color"), col + 1)));
        }
        rowGroup.end();
    }

    group.end();
}

#endif

// The display/transport selection. It is rendered by the settings form and by the matrix form,
// which validates the segment layout against the selected transport (see MatrixLayoutValidator and
// Resources/js/forms/led-matrix.js).
template<typename _ConfigType>
static void _createConfigureFormDisplayMethod(FormUI::Form::BaseForm &form, _ConfigType &cfg)
{
    #if HAVE_NEOPIXELBUS

        auto displayMethodItems = FormUI::Container::List(
            Clock::ShowMethodType::NEOBUS_RMT, F("NeoPixelBus RMT"),
            Clock::ShowMethodType::NEOBUS_I2S, F("NeoPixelBus I2S")
        );

        form.addObjectGetterSetter(F("dm"), FormGetterSetter(cfg, method));
        form.addFormUI(F("Display Method"), displayMethodItems);

    #elif IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT

        auto displayMethodItems = FormUI::Container::List(
            Clock::ShowMethodType::FASTLED, F("FastLED"),
            Clock::ShowMethodType::NEOPIXEL_EX, F("NeoPixelEx")
        );

        form.addObjectGetterSetter(F("dm"), FormGetterSetter(cfg, method));
        form.addFormUI(F("Display Method"), displayMethodItems);

    #endif
}

#if IOT_LED_MATRIX_CONFIGURABLE

namespace {

    // The form validates one field at a time and only writes the fields it has reached, so the
    // matrix values are read from the raw request to keep the result independent of the field order.
    Clock::MatrixValidation::Result _readMatrixForm(FormUI::Field::BaseField &field)
    {
        auto data = field.getForm().getFormData();
        if (!data) {
            return Clock::MatrixValidation::Result();
        }
        Clock::MatrixValidation::Segment segments[Clock::MatrixValidation::kMaxSegments] = {};
        #if IOT_LED_MATRIX_HAVE_PANELS
            for(uint8_t i = 0; i < Clock::MatrixValidation::kMaxSegments; i++) {
                // the pins that are not compiled in have no fields, arg() returns an empty string = disabled
                segments[i].enabled = data->arg(PrintString(F("mx_en%u"), i)).toInt() != 0;
                segments[i].rows = data->arg(PrintString(F("mx_pr%u"), i)).toInt();
                segments[i].cols = data->arg(PrintString(F("mx_pc%u"), i)).toInt();
                segments[i].posRow = data->arg(PrintString(F("mx_prp%u"), i)).toInt();
                segments[i].posCol = data->arg(PrintString(F("mx_pcp%u"), i)).toInt();
            }
        #endif
        return Clock::MatrixValidation::validate(
            data->arg(F("mx_rows")).toInt(),
            data->arg(F("mx_cols")).toInt(),
            segments,
            IOT_LED_MATRIX_CHANNELS,
            Clock::MatrixValidation::kMaxPixels
        );
    }

    // Checks the whole matrix layout. The form runs only the first validator of a field
    // (Form::BaseForm::_validatorFindNext() returns nullptr), therefore all checks are combined into
    // a single validator. It is attached to the read only "Maximum Number Of Pixels" field, which
    // has no other validator, and names the concrete cause of every problem.
    class MatrixLayoutValidator : public FormUI::Validator::BaseValidator {
    public:
        virtual bool validate() override
        {
            const auto result = _readMatrixForm(getField());
            if (!result.hasErrors()) {
                return true;
            }
            PrintString message;
            bool first = true;
            auto addLine = [&message, &first](const String &line) {
                if (!first) {
                    message.print(F("<br>"));
                }
                first = false;
                message.print(line);
            };

            if (!result.rows || !result.cols) {
                addLine(F("Rows and columns must be at least 1"));
            }
            else if (result.sizeExceeded) {
                addLine(PrintString(F("Rows x columns = %u x %u = %u exceeds the maximum number of pixels (%u)"),
                    static_cast<unsigned>(result.rows),
                    static_cast<unsigned>(result.cols),
                    static_cast<unsigned>(result.matrixPixels),
                    static_cast<unsigned>(result.maxPixels)
                ));
            }
            if (result.panelInvalid) {
                addLine(PrintString(F("Segment %u: the panel height and width must be at least 1"), static_cast<unsigned>(result.panelInvalidIndex)));
            }
            if (result.panelTooLarge) {
                const auto &segment = result.segments[result.panelTooLargeIndex - 1];
                addLine(PrintString(F("Segment %u: panel %u x %u = %u LEDs exceeds the maximum number of pixels (%u)"),
                    static_cast<unsigned>(result.panelTooLargeIndex),
                    static_cast<unsigned>(segment.rows),
                    static_cast<unsigned>(segment.cols),
                    static_cast<unsigned>(segment.pixels()),
                    static_cast<unsigned>(result.maxPixels)
                ));
            }
            if (result.panelOutside) {
                const auto &segment = result.segments[result.panelOutsideIndex - 1];
                addLine(PrintString(F("Segment %u: the panel %u x %u at row %u, column %u reaches outside the matrix (%u x %u)"),
                    static_cast<unsigned>(result.panelOutsideIndex),
                    static_cast<unsigned>(segment.rows),
                    static_cast<unsigned>(segment.cols),
                    static_cast<unsigned>(segment.posRow),
                    static_cast<unsigned>(segment.posCol),
                    static_cast<unsigned>(result.rows),
                    static_cast<unsigned>(result.cols)
                ));
            }
            if (result.transportExceeded) {
                addLine(PrintString(F("%u segments are enabled, this build has %u output pins"),
                    static_cast<unsigned>(result.activeSegments),
                    static_cast<unsigned>(Clock::MatrixValidation::kMaxStrips)
                ));
            }

            setMessage(message);
            return false;
        }
    };

}

#endif

void ClockPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    __LDBG_printf("callback_type=%u name=%s", type, formName.c_str());

    // handle delayed saves
    if (type == FormCallbackType::SAVE) {
        // a form callback runs in the web server task, the local config copy and the save timer
        // belong to the loop task
        #if IOT_LED_MATRIX_HAVE_PANELS
            // the test pattern buttons of the matrix form save the form like "Save", the pattern replaces
            // the animation after the new settings have been applied (see _reconfigureQueued() and _loop()).
            // mx_tp is "all" (+LMC=dir) or the segment 1-4 to show alone (+LMC=id)
            bool showTestPattern = false;
            int8_t testSegment = -1;
            if (F("matrix") == formName) {
                auto data = form.getFormData();
                const String testPattern = data ? data->arg(F("mx_tp")) : String();
                showTestPattern = testPattern.length() != 0;
                const int segment = testPattern.toInt();
                if (segment >= 1 && segment <= Clock::MatrixValidation::kMaxSegments) {
                    testSegment = segment - 1;
                }
            }
            _enqueue([this, showTestPattern, testSegment] {
                _syncConfigFromStorageQueued();
                if (showTestPattern) {
                    _testPattern = TestPatternType::AFTER_RECONFIGURE;
                    _testPatternSegment = testSegment;
                }
            });
        #else
            _enqueue([this] { _syncConfigFromStorageQueued(); });
        #endif
        return;
    }

    if (!isCreateFormCallbackType(type)) {
        return;
    }

    // the storage is kept in sync by the loop task (see _saveStateDeferred()) and is the only config the
    // form may use - _config and the save timer belong to the loop task
    auto &cfg = Plugins::Clock::getWriteableConfig();

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

    if (F("protection") == formName) {

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
        else if (F("matrix") == formName) {

            if (type == FormCallbackType::CREATE_GET) {
                // a device without a stored matrix parameter would show zeros, see _sanitizeMatrixConfig()
                _sanitizeMatrixConfig(cfg.matrix);
            }

            auto &mainGroup = form.addCardGroup(F("matrix"));

            // the transport the segments are driven with, the layout is validated against it
            _createConfigureFormDisplayMethod(form, cfg);

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

            #if HAVE_NEOPIXELBUS
                constexpr int kMethodRmt = static_cast<int>(Clock::ShowMethodType::NEOBUS_RMT);
                constexpr int kMethodI2s = static_cast<int>(Clock::ShowMethodType::NEOBUS_I2S);
            #else
                constexpr int kMethodRmt = -1;
                constexpr int kMethodI2s = -1;
            #endif
            form.add(F("mx_px"), String(IOT_CLOCK_NUM_PIXELS));
            form.addFormUI(F("Maximum Number Of Pixels"), FormUI::ReadOnlyAttribute(),
                FormUI::IntAttribute(F("data-max-strips"), static_cast<int>(Clock::MatrixValidation::kMaxStrips)),
                FormUI::IntAttribute(F("data-i2s-strips"), static_cast<int>(Clock::MatrixValidation::kAvailableI2sStrips)),
                FormUI::IntAttribute(F("data-i2s-port"), static_cast<int>(Clock::MatrixValidation::kMicI2sPort)),
                FormUI::IntAttribute(F("data-method-rmt"), kMethodRmt),
                FormUI::IntAttribute(F("data-method-i2s"), kMethodI2s)
            );
            // the matrix layout is validated as a whole, see MatrixLayoutValidator
            form.addValidator(MatrixLayoutValidator());

            #if IOT_LED_MATRIX_HAVE_PANELS

                // saves the form like "Save" and shows the test pattern with the new settings: "Show All" draws
                // the border of every panel (+LMC=dir), "Segment n" only the panel of that pin (+LMC=id,n)
                static const char testPatternButtons[] PROGMEM =
                    "<button type=\"submit\" name=\"mx_tp\" value=\"all\" class=\"btn btn-primary\">Show All</button>"
                    "<button type=\"submit\" name=\"mx_tp\" value=\"1\" class=\"btn btn-secondary\" title=\"Only the panel of segment 1, all other LEDs off\">Segment 1</button>"
                    #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
                        "<button type=\"submit\" name=\"mx_tp\" value=\"2\" class=\"btn btn-secondary\" title=\"Only the panel of segment 2, all other LEDs off\">Segment 2</button>"
                    #endif
                    #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
                        "<button type=\"submit\" name=\"mx_tp\" value=\"3\" class=\"btn btn-secondary\" title=\"Only the panel of segment 3, all other LEDs off\">Segment 3</button>"
                    #endif
                    #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
                        "<button type=\"submit\" name=\"mx_tp\" value=\"4\" class=\"btn btn-secondary\" title=\"Only the panel of segment 4, all other LEDs off\">Segment 4</button>"
                    #endif
                    ;
                form.add(F("mx_tpi"), String(F("Saves the settings and draws the border of every panel, or of one segment only")));
                form.addFormUI(F("Test Pattern"), FormUI::ReadOnlyAttribute(), FormUI::SuffixHtml(FPSTR(testPatternButtons)));

                // clockwise, the panel height/width are entered as the panel is mounted
                auto rotationItems = FormUI::List(
                    0, F("0\xc2\xb0"),
                    1, F("90\xc2\xb0 clockwise"),
                    2, F("180\xc2\xb0"),
                    3, F("270\xc2\xb0 clockwise")
                );

                // the fields of the panel of segment n (0 based, shown as segment s), see led-matrix.html for the
                // help texts and led-matrix.js for the live validation. The size of a disabled segment is not
                // validated (0 = unset is accepted, MatrixLayoutValidator rejects 0 for an enabled segment)
                #define ADD_PANEL_FIELDS(n, s, pin) \
                    { \
                        auto &enabled = form.addObjectGetterSetter(F("mx_en" _STRINGIFY(n)), FormGetterSetter(cfg.matrix, enabled##n)); \
                        form.addFormUI(FormUI::Type::HIDDEN); \
                        form.add(F("mx_seg" _STRINGIFY(n)), String()); \
                        form.addFormUI(F("Segment " _STRINGIFY(s) " Pin #" _STRINGIFY(pin)), FormUI::ReadOnlyAttribute(), FormUI::CheckboxButtonSuffix(enabled, F("Enabled"))); \
                        form.addObjectGetterSetter(F("mx_pr" _STRINGIFY(n)), FormGetterSetter(cfg.matrix, panel_rows##n)); \
                        form.addFormUI(F("Segment " _STRINGIFY(s) " Panel Height (Rows)")); \
                        cfg.matrix.addRangeValidatorFor_panel_rows##n(form, true); \
                        form.addObjectGetterSetter(F("mx_pc" _STRINGIFY(n)), FormGetterSetter(cfg.matrix, panel_cols##n)); \
                        form.addFormUI(F("Segment " _STRINGIFY(s) " Panel Width (Columns)")); \
                        cfg.matrix.addRangeValidatorFor_panel_cols##n(form, true); \
                        form.addObjectGetterSetter(F("mx_prp" _STRINGIFY(n)), FormGetterSetter(cfg.matrix, pos_row##n)); \
                        form.addFormUI(F("Segment " _STRINGIFY(s) " Panel Placement: Top Row")); \
                        cfg.matrix.addRangeValidatorFor_pos_row##n(form); \
                        form.addObjectGetterSetter(F("mx_pcp" _STRINGIFY(n)), FormGetterSetter(cfg.matrix, pos_col##n)); \
                        form.addFormUI(F("Segment " _STRINGIFY(s) " Panel Placement: Left Column")); \
                        cfg.matrix.addRangeValidatorFor_pos_col##n(form); \
                        auto &mirror = form.addObjectGetterSetter(F("mx_mi" _STRINGIFY(n)), FormGetterSetter(cfg.matrix, mirror##n)); \
                        form.addFormUI(FormUI::Type::HIDDEN); \
                        form.addObjectGetterSetter(F("mx_rot" _STRINGIFY(n)), FormGetterSetter(cfg.matrix, rotation##n)); \
                        form.addFormUI(F("Segment " _STRINGIFY(s) " Panel Rotation"), FormUI::Type::SELECT, rotationItems, FormUI::CheckboxButtonSuffix(mirror, F("Mirror"))); \
                    }

                // the read only field shows the LED count and linear offset of the segment, filled in by led-matrix.js
                ADD_PANEL_FIELDS(0, 1, IOT_LED_MATRIX_OUTPUT_PIN);
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
                    ADD_PANEL_FIELDS(1, 2, IOT_LED_MATRIX_OUTPUT_PIN1);
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
                    ADD_PANEL_FIELDS(2, 3, IOT_LED_MATRIX_OUTPUT_PIN2);
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
                    ADD_PANEL_FIELDS(3, 4, IOT_LED_MATRIX_OUTPUT_PIN3);
                #endif
                #undef ADD_PANEL_FIELDS

            #endif

            mainGroup.end();

        }
    #endif
    #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
        else if (F("irremote") == formName) {

            _createConfigureFormIRRemote(form, cfg);

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
        form.addFormUI(FormUI::Type::RANGE_SLIDER, F("Brightness"), FormUI::MinMax(cfg.kMinValueFor_brightness, cfg.kMaxValueFor_brightness));

        form.addObjectGetterSetter(F("ft"), FormGetterSetter(cfg, fading_time));
        form.addFormUI(F("Fading Time From 0 To 100%"), FormUI::Suffix(F("seconds")));
        cfg.addRangeValidatorFor_fading_time(form, true);

        #if IOT_LED_MATRIX == 0
            form.addObjectGetterSetter(F("tfmt"), FormGetterSetter(cfg, time_format_24h));
            form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

            form.addObjectGetterSetter(F("csp"), FormGetterSetter(cfg, blink_colon_speed));
            form.addFormUI(F("Colon Blink Speed"), FormUI::Suffix(F("milliseconds")), FormUI::IntAttribute(F("disabled-value"), 0), FormUI::FPStringAttribute(F("disabled-value-placeholder"), F("Solid")));
            cfg.addRangeValidatorFor_blink_colon_speed(form, true);
        #endif

        _createConfigureFormDisplayMethod(form, cfg);

        // NeoPixelBus has no temporal dithering, the stored value is kept but not shown
        #if !HAVE_NEOPIXELBUS
            form.addObjectGetterSetter(F("dt"), FormGetterSetter(cfg, dithering));
            form.addFormUI(F("FastLED Temporal Dithering"), FormUI::BoolItems(F("Enable"), F("Disable")));
        #endif

        #if IOT_LED_MATRIX_STANDBY_PIN != -1
            form.addObjectGetterSetter(F("sbl"), cfg, cfg.get_bits_standby_led, cfg.set_bits_standby_led);
            form.addFormUI(F("Standby LED/Relay or MOSFET"), FormUI::BoolItems(F("Enable"), F("Disable")));
        #endif

        mainGroup.end();

        #if IOT_ALARM_PLUGIN_ENABLED

            // color and speed of the alarm animation
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

    form.finalize();
}

#if IOT_LED_MATRIX_HAVE_PANELS

void ClockPlugin::_registerMatrixTestWebHandler()
{
    WebServer::Plugin::addHandler(F("/led-matrix-test.json"), [](AsyncWebServerRequest *request) {
        ClockPlugin::_matrixTestWebHandler(request);
    });
}

// test pattern buttons of the matrix form (led-matrix.js): the posted form is validated and saved like
// the Save button and applied right away (no "applying" page), the pattern replaces the animation
// once the new settings are active (see _reconfigureQueued()). Answers {"ok":true} or the validation
// errors {"ok":false,"errors":[{"target":"#field","error":"message"}]} for $.formValidator.addErrors().
// Runs in the web server task like the regular form POST
void ClockPlugin::_matrixTestWebHandler(AsyncWebServerRequest *request)
{
    if (!WebServer::Plugin::isAuthenticated(request)) {
        request->send(403);
        return;
    }
    if (request->method() != HTTP_POST) {
        request->send(405);
        return;
    }

    using MQTT::Json::UnnamedObject;
    using MQTT::Json::NamedArray;
    using MQTT::Json::NamedBool;
    using MQTT::Json::NamedString;

    auto &plugin = getInstance();
    const String formName = F("matrix");
    std::unique_ptr<FormUI::Form::BaseForm> form(new SettingsForm(request));
    plugin.createConfigureForm(FormCallbackType::CREATE_POST, formName, *form, request);

    UnnamedObject json;
    if (form->validate()) {
        plugin.createConfigureForm(FormCallbackType::SAVE, formName, *form, request);
        KFCConfigurationClasses::System::Flags::getWriteableConfig().is_factory_settings = false;
        config.write();
        plugin.reconfigure(formName);
        json.append(NamedBool(F("ok"), true));
    }
    else {
        plugin.createConfigureForm(FormCallbackType::DISCARD, formName, *form, request);
        config.discard();
        NamedArray errors(F("errors"));
        for(const auto &error: form->getErrors()) {
            errors.append(UnnamedObject(
                NamedString(F("target"), PrintString(F("#%s"), error.getName())),
                NamedString(F("error"), error.getMessage())
            ));
        }
        json.append(NamedBool(F("ok"), false));
        json.append(errors);
    }

    HttpHeaders headers;
    headers.addNoCache();
    auto response = request->beginResponse(200, FSPGM(mime_application_json), json.toString());
    headers.setResponseHeaders(response);
    request->send(response);
}

#endif
