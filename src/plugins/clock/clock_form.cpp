/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "clock.h"
#include <KFCForms.h>
#include "./plugins/sensor/sensor.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void ClockPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    __LDBG_printf("type=%u name=%s", type, formName.c_str());
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = getWriteableConfig();

    #if IOT_LED_MATRIX

        auto animationTypeItems = FormUI::Container::List(
            AnimationType::SOLID, FSPGM(Solid_Color),
            AnimationType::RAINBOW, FSPGM(Rainbow),
            AnimationType::FIRE, F("Fire"),
            AnimationType::FLASHING, F("Flash"),
            AnimationType::FADING, F("Color Fade"),
            AnimationType::INTERLEAVED, F("Interleaved")
        );

        auto &ui = form.createWebUI();
        ui.setTitle(F("LED Matrix Configuration"));
        ui.setContainerId(F("led_matrix_settings"));
        ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    #else
        auto animationTypeItems = FormUI::Container::List(
            AnimationType::SOLID, FSPGM(Solid_Color),
            AnimationType::RAINBOW, FSPGM(Rainbow),
            AnimationType::FLASHING, F("Flash"),
            AnimationType::FADING, F("Color Fade")
        );

        auto &ui = form.createWebUI();
        ui.setTitle(F("Clock Configuration"));
        ui.setContainerId(F("clock_settings"));
        ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    #endif

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    #if !IOT_LED_MATRIX

        form.addObjectGetterSetter(F("tfmt"), cfg, cfg.get_bits_time_format_24h, cfg.set_bits_time_format_24h);
        form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

        form.addObjectGetterSetter(F("csp"), cfg, cfg.get_bits_blink_colon_speed, cfg.set_bits_blink_colon_speed);
        form.addFormUI(F("Colon Blink Speed"), FormUI::Suffix(F("milliseconds, 0 = solid")));
        cfg.addRangeValidatorFor_blink_colon_speed(form, true);

    #endif

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

    #if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
        form.addObjectGetterSetter(F("auto_br"), cfg, cfg.get_bits_auto_brightness, cfg.set_bits_auto_brightness);
        form.addFormUI(F("Auto Brightness Value"), FormUI::SuffixHtml(F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>")));
        cfg.addRangeValidatorFor_auto_brightness(form);

    #endif

    form.addObjectGetterSetter(F("ft"), cfg, cfg.get_bits_fading_time, cfg.set_bits_fading_time);
    form.addFormUI(F("Fading Time From 0 To 100%"), FormUI::Suffix(FSPGM(seconds)));
    cfg.addRangeValidatorFor_fading_time(form, true);

    #if IOT_CLOCK_USE_DITHERING

        form.addObjectGetterSetter(F("dt"), cfg, cfg.get_bits_dithering, cfg.set_bits_dithering);
        form.addFormUI(F("Dithering"), FormUI::BoolItems(F("Enable"), F("Disable")));

    #endif

    mainGroup.end();

    // --------------------------------------------------------------------
    auto &animationGroup = form.addCardGroup(F("anicfg"), FSPGM(Animation), true);

    form.addObjectGetterSetter(F("ani"), cfg, cfg.get_int_animation, cfg.set_int_animation);
    form.addFormUI(FSPGM(Type), animationTypeItems);
    //form.addValidator(FormUI::Validator::RangeEnum<AnimationType>());

    form.add(F("col"), Color(cfg.solid_color.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.solid_color.value = Color::fromString(value);
        }
        return false;
    }, FormUI::Field::Type::TEXT);
    form.addFormUI(FSPGM(Solid_Color));

    form.addObjectGetterSetter(F("fsp"), cfg, cfg.get_bits_flashing_speed, cfg.set_bits_flashing_speed);
    form.addFormUI(F("Flashing Speed"), FormUI::Suffix(FSPGM(milliseconds)));
    cfg.addRangeValidatorFor_flashing_speed(form);

    animationGroup.end();

    // --------------------------------------------------------------------
    auto &rainbowGroup = form.addCardGroup(F("rainbow"), F("Rainbow Animation"), true);

    form.addPointerTriviallyCopyable(F("rb_mul"), &cfg.rainbow.multiplier.value);
    form.addFormUI(FSPGM(Multiplier));
    form.addValidator(FormUI::Validator::Range(0.1f, 100.0f));

    form.addPointerTriviallyCopyable(F("rb_incr"), &cfg.rainbow.multiplier.incr);
    form.addFormUI(F("Multiplier Increment Per Frame"));
    form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));

    form.addPointerTriviallyCopyable(F("rb_min"), &cfg.rainbow.multiplier.min);
    form.addFormUI(F("Minimum Multiplier"));
    form.addValidator(FormUI::Validator::Range(0.1f, 100.0f));

    form.addPointerTriviallyCopyable(F("rb_max"), &cfg.rainbow.multiplier.max);
    form.addFormUI(F("Maximum Multiplier"));
    form.addValidator(FormUI::Validator::Range(0.1f, 100.0f));

    form.addPointerTriviallyCopyable(F("rb_sp"), &cfg.rainbow.speed);
    form.addFormUI(FSPGM(Speed));
    form.addValidator(FormUI::Validator::Range(kMinRainbowSpeed, 0xffff));

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

    form.addPointerTriviallyCopyable(F("rb_cre"), &cfg.rainbow.color.red_incr);
    form.addFormUI(F("Color Increment Per Frame (Red)"));
    form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));

    form.addPointerTriviallyCopyable(F("rb_cgr"), &cfg.rainbow.color.green_incr);
    form.addFormUI(F("Color Increment Per Frame (Green)"));
    form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));

    form.addPointerTriviallyCopyable(F("rb_cbl"), &cfg.rainbow.color.blue_incr);
    form.addFormUI(F("Color Increment Per Frame (Blue)"));
    form.addValidator(FormUI::Validator::Range(0.0f, 0.1f));

    rainbowGroup.end();

    // --------------------------------------------------------------------
    #if IOT_LED_MATRIX

        auto &fireGroup = form.addCardGroup(F("fire"), F("Fire Animation"), true);

        form.addPointerTriviallyCopyable(F("fic"), &cfg.fire.cooling);
        form.addFormUI(F("Cooling Value:"));
        form.addValidator(FormUI::Validator::Range(0, 255));

        form.addPointerTriviallyCopyable(F("fis"), &cfg.fire.sparking);
        form.addFormUI(F("Sparking Value"));
        form.addValidator(FormUI::Validator::Range(0, 255));

        form.addPointerTriviallyCopyable(F("fip"), &cfg.fire.speed);
        form.addFormUI(F("Speed"), FormUI::Suffix("milliseconds"));
        form.addValidator(FormUI::Validator::Range(5, 100));

        auto &invertHidden = form.addObjectGetterSetter(F("firei"), cfg.fire, cfg.fire.get_bit_invert_direction, cfg.fire.set_bit_invert_direction);
        form.addFormUI(FormUI::Type::HIDDEN);

        auto orientationItems = FormUI::List(
            Plugins::ClockConfig::FireAnimation_t::Orientation::VERTICAL, "Vertical",
            Plugins::ClockConfig::FireAnimation_t::Orientation::HORIZONTAL, "Horizontal"
        );

        form.addObjectGetterSetter(F("fio"), cfg.fire, cfg.fire.get_int_orientation, cfg.fire.set_int_orientation);
        form.addFormUI(F("Orientation"), orientationItems, FormUI::CheckboxButtonSuffix(invertHidden, F("Invert Direction")));

        form.add(F("fif"), Color(cfg.fire.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
            if (store) {
                cfg.fire.factor.value = Color::fromString(value);
            }
            return false;
        });
        form.addFormUI(F("Color Factor"));

        fireGroup.end();

    #endif

    // --------------------------------------------------------------------
    #if IOT_LED_MATRIX

        auto &skipRowsGroups = form.addCardGroup(F("ild"), F("Interleaved"), true);

        form.addPointerTriviallyCopyable(F("ilr"), &cfg.interleaved.rows);
        form.addFormUI(F("Display every nth row"));
        form.addValidator(FormUI::Validator::Range(0, IOT_LED_MATRIX_ROWS));

        form.addPointerTriviallyCopyable(F("ilc"), &cfg.interleaved.cols);
        form.addFormUI(F("Display every nth column"));
        form.addValidator(FormUI::Validator::Range(0, IOT_LED_MATRIX_COLS));

        form.addPointerTriviallyCopyable(F("ilt"), &cfg.interleaved.time);
        form.addFormUI(F("Rotate Through Rows And Columns"), FormUI::Suffix(F("milliseconds, 0 = disable")));

        skipRowsGroups.end();

    #endif

    // --------------------------------------------------------------------
    auto &fadingGroup = form.addCardGroup(F("fade"), F("Color Fade"), true);

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

    fadingGroup.end();

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

        form.addPointerTriviallyCopyable(F("asp"), &cfg.alarm.speed);
        form.addFormUI(F("Flashing Speed"), FormUI::Suffix(FSPGM(milliseconds)));
        form.addValidator(FormUI::Validator::Range(50, 0xffff));

        alarmGroup.end();

    #endif

    // --------------------------------------------------------------------
    auto &protectionGroup = form.addCardGroup(F("prot"), FSPGM(Protection), true);

    form.addPointerTriviallyCopyable(F("tmi"), &cfg.protection.temperature_reduce_range.min);
    form.addFormUI(F("Minimum Temperature To Reduce Brightness"), FormUI::Suffix(FSPGM(degree_Celsius_utf8)));
    form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 90));

    form.addPointerTriviallyCopyable(F("tma"), &cfg.protection.temperature_reduce_range.max);
    form.addFormUI(F("Maximum. Temperature To Reduce Brightness To 25%"), FormUI::Suffix(FSPGM(degree_Celsius_utf8)));
    form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 90));

    form.addPointerTriviallyCopyable(F("tpm"), &cfg.protection.max_temperature);
    form.addFormUI(F("Over Temperature Protection"), FormUI::Suffix(FSPGM(degree_Celsius_utf8)));
    form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 105));

    protectionGroup.end();

    auto &powerGroup = form.addCardGroup(F("pow"), F("Power"), true);

    form.addObjectGetterSetter(F("pl"), cfg, cfg.get_bits_power_limit, cfg.set_bits_power_limit);
    form.addFormUI(F("Limit Maximum Power"), FormUI::Suffix(F("Watt, 0 = Disable")));
    cfg.addRangeValidatorFor_fading_time(form, true);

    form.addPointerTriviallyCopyable(F("plr"), &cfg.power.red);
    form.addFormUI(F("Power Consumption For 256 LEDs at 100% Red"), FormUI::Suffix(F("mW")));
    form.addValidator(FormUI::Validator::Range(0, 65535));

    form.addPointerTriviallyCopyable(F("plg"), &cfg.power.green);
    form.addFormUI(F("Power Consumption 100% Green"), FormUI::Suffix(F("mW")));
    form.addValidator(FormUI::Validator::Range(0, 65535));

    form.addPointerTriviallyCopyable(F("plb"), &cfg.power.blue);
    form.addFormUI(F("Power Consumption 100% Blue"), FormUI::Suffix(F("mW")));
    form.addValidator(FormUI::Validator::Range(0, 65535));

    form.addPointerTriviallyCopyable(F("pli"), &cfg.power.idle);
    form.addFormUI(F("Power Consumption While Idle"), FormUI::Suffix(F("mW")));
    form.addValidator(FormUI::Validator::Range(0, 65535));

    powerGroup.end();

    form.finalize();
}
