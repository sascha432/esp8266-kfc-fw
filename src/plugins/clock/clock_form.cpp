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

void ClockPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    __LDBG_printf("type=%u name=%s", type, formName.c_str());
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Clock::getWriteableConfig();

    auto &ui = form.getFormUIConfig();
    ui.setTitle(F("Clock Configuration"));
    ui.setContainerId(F("clock_settings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config), String());

    form.add<bool>(F("timef"), _H_W_STRUCT_VALUE(cfg, time_format_24h));
    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    form.add<uint8_t>(FSPGM(brightness), _H_W_STRUCT_VALUE(cfg, brightness));
    form.addFormUI(FormUI::Type::RANGE_SLIDER, FSPGM(Brightness), FormUI::MinMax(0, 255));

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

    form.add<int16_t>(F("auto_br"), _H_W_STRUCT_VALUE(cfg, auto_brightness));
    form.addFormUI(F("Auto Brightness Value"), FormUI::FPSuffix(F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>")));
    form.addValidator(FormRangeValidator(-1, 1023));

#endif

    mainGroup.end();

    // --------------------------------------------------------------------
    auto &animationGroup = form.addCardGroup(F("anicfg"), FSPGM(Animation), true);

    form.add<int8_t>(F("ani"), _H_W_STRUCT_VALUE(cfg, animation));
    form.addFormUI(FSPGM(Type), FormUI::ItemsList(
        AnimationType::NONE, FSPGM(Solid_Color),
        AnimationType::RAINBOW, FSPGM(Rainbow),
        AnimationType::FLASHING, FSPGM(Flashing),
        AnimationType::FADING, FSPGM(Fading)
    ));
    form.addValidator(FormRangeValidatorEnum<AnimationType>());

    form.add(F("col"), Color(cfg.solid_color.value).toString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.solid_color.value = Color::fromString(value);
        }
        return false;
    }, FormField::Type::TEXT);
    form.addFormUI(FSPGM(Solid_Color));

    form.add<float>(F("colon_sp"), _H_W_STRUCT_VALUE(cfg, blink_colon_speed));
    form.addFormUI(F("Colon Blink Speed"), FormUI::FPSuffix(F("milliseconds, 0 = solid")));
    form.addValidator(FormRangeValidator(kMinBlinkColonSpeed, 0xffff, true));

    form.add<uint16_t>(F("flash_sp"), _H_W_STRUCT_VALUE(cfg, flashing_speed));
    form.addFormUI(F("Flashing Speed"), FormUI::FPSuffix(FSPGM(milliseconds)));
    form.addValidator(FormRangeValidator(kMinFlashingSpeed, 0xffff));

    animationGroup.end();

    // --------------------------------------------------------------------
    auto &rainbowGroup = form.addCardGroup(F("rainbow"), F("Rainbow Animation"), true);

    form.add<float>(F("rb_mul"), _H_W_STRUCT_VALUE(cfg, rainbow.multiplier));
    form.addFormUI(FSPGM(Multiplier));
    form.addValidator(FormRangeValidator(0.1f, 100.0f));

    form.add<uint16_t>(F("rb_sp"), _H_W_STRUCT_VALUE(cfg, rainbow.speed));
    form.addFormUI(FSPGM(Speed));
    form.addValidator(FormRangeValidator(kMinRainbowSpeed, 0xffff));

    form.add(F("rb_cf"), Color(cfg.rainbow.factor.value).toString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.rainbow.factor.value = Color::fromString(value);
        }
        return false;
    });
    form.addFormUI(F("Color Factor"));

    form.add(F("rb_mv"), Color(cfg.rainbow.minimum.value).toString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.rainbow.minimum.value = Color::fromString(value);
        }
        return false;
    });
    form.addFormUI(F("Minimum Color Value"));

    rainbowGroup.end();

    // --------------------------------------------------------------------
    auto &fadingGroup = form.addCardGroup(F("fading"), F("Random Color Fading"), true);

    form.add<float>(F("fade_sp"), _H_W_STRUCT_VALUE(cfg, fading.speed));
    form.addFormUI(F("Time Between Fading Colors"), FormUI::FPSuffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidator(Clock::FadingAnimation::kMinSeconds, Clock::FadingAnimation::kMaxSeconds));

    form.add<uint16_t>(F("fade_dy"), _H_W_STRUCT_VALUE(cfg, fading.delay));
    form.addFormUI(F("Delay Before Start Fading To Next Random Color"), FormUI::FPSuffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidator(0, Clock::FadingAnimation::kMaxDelay));

    form.add(F("fade_cf"), Color(cfg.fading.factor.value).toString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.fading.factor.value = Color::fromString(value);
        }
        return false;
    });
    form.addFormUI(F("Random Color Factor"));

    fadingGroup.end();

#if IOT_ALARM_PLUGIN_ENABLED

    // --------------------------------------------------------------------
    auto &alarmGroup = form.addCardGroup(FSPGM(alarm), FSPGM(Alarm), true);

    form.add(F("alrm_col"), Color(cfg.alarm.color.value).toString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.alarm.color.value = Color::fromString(value);
        }
        return false;
    });
    form.addFormUI(FSPGM(Color));

    form.add<uint16_t>(F("alrm_sp"), _H_W_STRUCT_VALUE(cfg, alarm.speed));
    form.addFormUI(F("Flashing Speed"), FormUI::FPSuffix(FSPGM(milliseconds)));
    form.addValidator(FormRangeValidator(50, 0xffff));

    alarmGroup.end();

#endif

    // --------------------------------------------------------------------
    auto &protectionGroup = form.addCardGroup(F("prot"), FSPGM(Protection), true);

    form.add<uint8_t>(F("temp_75"), _H_W_STRUCT_VALUE(cfg, protection.temperature_75));
    form.addFormUI(F("Temperature To Reduce Brightness To 75%"), FormUI::FPSuffix(FSPGM(_degreeC)));
    form.addValidator(FormRangeValidator(kMinimumTemperatureThreshold, 90));

    form.add<uint8_t>(F("temp_50"), _H_W_STRUCT_VALUE(cfg, protection.temperature_50));
    form.addFormUI(F("Temperature To Reduce Brightness To 50%"), FormUI::FPSuffix(FSPGM(_degreeC)));
    form.addValidator(FormRangeValidator(kMinimumTemperatureThreshold, 90));

    form.add<uint8_t>(F("max_temp"), _H_W_STRUCT_VALUE(cfg, protection.max_temperature));
    form.addFormUI(F("Over Temperature Protection"), FormUI::FPSuffix(FSPGM(_degreeC)));
    form.addValidator(FormRangeValidator(kMinimumTemperatureThreshold, 105));

    protectionGroup.end();

    form.finalize();
}
