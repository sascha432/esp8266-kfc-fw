/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <KFCForms.h>
#include "../src/plugins/sensor/sensor.h"
#include "Utility/ProgMemHelper.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_LED_MATRIX
#   define FORM_TITLE "LED Matrix Configuration"
#else
#   define FORM_TITLE "Clock Configuration"
#endif

void ClockPlugin::_createConfigureFormAnimation(AnimationType animation, FormUI::Form::BaseForm &form, ClockConfigType &cfg, TitleType titleType)
{
    FormUI::Group *group;
    switch(titleType) {
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
                    FireAnimationType::OrientationType::VERTICAL, "Vertical",
                    FireAnimationType::OrientationType::HORIZONTAL, "Horizontal"
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
        #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
            case AnimationType::VISUALIZER: {
                    auto visualizerTypeItems = FormUI::Container::List(
                        VisualizerType::RAINBOW, "Rainbow",
                        VisualizerType::RAINBOW_DOUBLE_SIDED, "Rainbow Double Sided",
                        VisualizerType::SINGLE_COLOR, "Single Color",
                        VisualizerType::SINGLE_COLOR_DOUBLE_SIDED, "Single Color Double Sided"
                    );
                    form.addObjectGetterSetter(F("vln"), FormGetterSetter(cfg.visualizer, type));
                    form.addFormUI(F("Visualization Type"), visualizerTypeItems);

                    form.add(F("vsc"), Color(cfg.visualizer.color).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
                        if (store) {
                            cfg.visualizer.color = Color::fromString(value);
                        }
                        return false;
                    });
                    form.addFormUI(F("Color For Single Color Mode"));

                    form.addObjectGetterSetter(F("vport"), FormGetterSetter(cfg.visualizer, port));
                    form.addFormUI(F("UDP Port"));
                    cfg.visualizer.addRangeValidatorFor_port(form);
                }
                break;
        #endif
        case AnimationType::RAINBOW: {
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
                form.addObjectGetterSetter(F("rb_bpm"), FormGetterSetter(cfg.rainbow, bpm));
                form.addFormUI(F("BPM"));
                cfg.rainbow.addRangeValidatorFor_bpm(form);

                form.addObjectGetterSetter(F("rb_hue"), FormGetterSetter(cfg.rainbow, hue));
                form.addFormUI(F("Hue"));
                cfg.rainbow.addRangeValidatorFor_hue(form);
            }
            break;
        case AnimationType::GRADIENT: {
                PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_CLOCK_GRADIENT_ENTRIES, color, pixels);
                for(uint8_t i = 0; i < cfg.gradient.kMaxEntries; i++) {

                    form.add(F_VAR(color, i), Color(cfg.rainbow.color.min.value).toString(), [&cfg, i](const String &value, FormUI::Field::BaseField &field, bool store) {
                        if (store) {
                            cfg.gradient.entries[i].color = Color::fromString(value);
                        }
                        return false;
                    });
                    form.addFormUI(F("Color"));

                    form.addObjectGetterSetter(F_VAR(pixels, i), FormGetterSetter(cfg.gradient.entries[i], pixel));
                    form.addFormUI(F("Position"));
                    // form.addValidator(FormUI::Validator::Range(0, _display.size() - 1));
                }
            }
            break;
        case AnimationType::MAX:
            break;
    }

    if (group) {
        group->end();
    }
}

void ClockPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    __LDBG_printf("callback_type=%u name=%s", type, formName.c_str());

    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = getWriteableConfig();

    // sub forms for the WebUI
    if (formName.startsWith(F("ani-"))) {
        auto animation = _getAnimationType(FPSTR(formName.c_str() + 4));
        __DBG_printf("form=%s animation=%u valid=%u", formName.c_str() + 4, animation, animation != AnimationType::MAX);
        if (animation != AnimationType::MAX) {
            auto &ui = form.createWebUI();
            ui.setContainerId(F("led-matrix-settings"));
            ui.setStyle(FormUI::WebUI::StyleType::WEBUI);
            _createConfigureFormAnimation(animation, form, cfg, TitleType::SET_TITLE);
            form.finalize();
        }
        return;
    }

    auto &ui = form.createWebUI();
    ui.setTitle(F(FORM_TITLE));
    ui.setContainerId(F("led-matrix-settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

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
        _createConfigureFormAnimation(AnimationType::GRADIENT, form, cfg, TitleType::ADD_GROUP);

        // --------------------------------------------------------------------
        _createConfigureFormAnimation(AnimationType::RAINBOW, form, cfg, TitleType::ADD_GROUP);

        // --------------------------------------------------------------------
        _createConfigureFormAnimation(AnimationType::RAINBOW_FASTLED, form, cfg, TitleType::ADD_GROUP);

        // --------------------------------------------------------------------
        #if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
            _createConfigureFormAnimation(AnimationType::VISUALIZER, form, cfg, TitleType::ADD_GROUP);
        #endif

        // --------------------------------------------------------------------
        _createConfigureFormAnimation(AnimationType::FIRE, form, cfg, TitleType::ADD_GROUP);

        // --------------------------------------------------------------------
        _createConfigureFormAnimation(AnimationType::INTERLEAVED, form, cfg, TitleType::ADD_GROUP);

        // --------------------------------------------------------------------
        _createConfigureFormAnimation(AnimationType::FADING, form, cfg, TitleType::ADD_GROUP);

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
    else if (formName == F("protection")) {

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

            form.addObjectGetterSetter(F("tpv"), FormGetterSetter(cfg.protection, regulator_margin));
            form.addFormUI(F("Extra Margin For Voltage Regulator"), FormUI::Suffix(FSPGM(UTF8_degreeC)));
            form.addValidator(FormUI::Validator::Range(-50, 50));

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

        form.addObjectGetterSetter(F("pl"), FormGetterSetter(cfg, power_limit));
        form.addFormUI(F("Limit Maximum Power"), FormUI::Suffix(F("Watt")), FormUI::IntAttribute(F("disabled-value"), 0));
        cfg.addRangeValidatorFor_fading_time(form, true);

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

            form.addObjectGetterSetter(F("mx_px"), FormGetterSetter(cfg.matrix, pixels));
            form.addFormUI(F("Number Of Pixels"), FormUI::ReadOnlyAttribute());
            // cfg.matrix.addRangeValidatorFor_pixels(form);

            form.addObjectGetterSetter(F("mx_ofs"), FormGetterSetter(cfg.matrix, offset));
            form.addFormUI(F("First Pixel Offset"));
            form.addValidator(FormUI::Validator::Range(0, _display.getMaxNumPixels()));
            validator = &form.addValidator(FormUI::Validator::CallbackTemplate<uint16_t>([&cfg, &validator, this](uint16_t offset, Field::BaseField &field) {
                auto rows = field.getForm().getField(F("mx_rows"))->getValue().toInt();
                auto cols = field.getForm().getField(F("mx_cols"))->getValue().toInt();
                if ((rows * cols) + offset <= static_cast<long>(_display.getMaxNumPixels())) {
                    return true;
                }
                validator->setMessage(PrintString(F("offset + (rows * cols) = %u + (%u * %u) = %u exceeds exceeds maximum number of pixels (%u)"),
                    static_cast<unsigned>(offset),
                    static_cast<unsigned>(rows),
                    static_cast<unsigned>(cols),
                    static_cast<unsigned>((rows * cols) + offset),
                    static_cast<unsigned>(_display.getMaxNumPixels()))
                );
                return false;
            }));


            form.addObjectGetterSetter(F("mx_rt"), FormGetterSetter(cfg.matrix, rotate));
            form.addFormUI(F("90\xc2\xb0 Rotation"), FormUI::BoolItems());

            form.addObjectGetterSetter(F("mx_il"), FormGetterSetter(cfg.matrix, interleaved));
            form.addFormUI(F("Interleaved"), FormUI::BoolItems());

            mainGroup.end();

        }
    #endif
    else {

        auto &mainGroup = form.addCardGroup(FSPGM(config));

        // --------------------------------------------------------------------
        #if IOT_CLOCK_SAVE_STATE
            auto initialStateItems = FormUI::Container::List(
                Clock::InitialStateType::OFF, F("Turn Off"),
                Clock::InitialStateType::ON, F("Turn On"),
                Clock::InitialStateType::RESTORE, F("Restore Last State")
            );

            form.addObjectGetterSetter(F("is"), cfg, cfg.get_int_initial_state, cfg.set_int_initial_state);
            form.addFormUI(F("After Reset"), initialStateItems);
        #endif

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

        form.addObjectGetterSetter(F("dt"), FormGetterSetter(cfg, dithering));
        form.addFormUI(F("FastLED Temporal Dithering"), FormUI::BoolItems(F("Enable"), F("Disable")));

        #if IOT_LED_MATRIX_STANDBY_PIN != -1
            form.addObjectGetterSetter(F("sbl"), cfg, cfg.get_bits_standby_led, cfg.set_bits_standby_led);
            form.addFormUI(F("Standby LED"), FormUI::BoolItems(F("Enable"), F("Disable")));
        #endif

        mainGroup.end();

    }

    form.finalize();
}
