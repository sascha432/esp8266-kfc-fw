/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX

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

    auto &cfg = Plugins::Clock::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setTitle(F("LED Matrix Configuration"));
    ui.setContainerId(F("led_matrix_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addPointerTriviallyCopyable(FSPGM(brightness), &cfg.brightness);
    form.addFormUI(FormUI::Type::RANGE_SLIDER, FSPGM(Brightness), FormUI::MinMax(0, 255));

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

    form.addPointerTriviallyCopyable(F("auto_br"), &cfg.auto_brightness);
    form.addFormUI(F("Auto Brightness Value"), FormUI::SuffixHtml(F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>")));
    form.addValidator(FormUI::Validator::Range(-1, 1023));

#endif

    mainGroup.end();

    // --------------------------------------------------------------------
    #include "form_rainbow_ani.hpp"

    // --------------------------------------------------------------------
    auto &fadingGroup = form.addCardGroup(F("fading"), F("Random Color Fading"), true);

    form.addPointerTriviallyCopyable(F("fade_sp"), &cfg.fading.speed);
    form.addFormUI(F("Time Between Fading Colors"), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::Range(Clock::FadingAnimation::kMinSeconds, Clock::FadingAnimation::kMaxSeconds));

    form.addPointerTriviallyCopyable(F("fade_dy"), &cfg.fading.delay);
    form.addFormUI(F("Delay Before Start Fading To Next Random Color"), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::Range(0, Clock::FadingAnimation::kMaxDelay));

    form.add(F("fade_cf"), Color(cfg.fading.factor.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
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

    form.add(F("alrm_col"), Color(cfg.alarm.color.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.alarm.color.value = Color::fromString(value);
        }
        return false;
    });
    form.addFormUI(FSPGM(Color));

    form.addPointerTriviallyCopyable(F("alrm_sp"), &cfg.alarm.speed);
    form.addFormUI(F("Flashing Speed"), FormUI::Suffix(FSPGM(milliseconds)));
    form.addValidator(FormUI::Validator::Range(50, 0xffff));

    alarmGroup.end();

#endif

    // --------------------------------------------------------------------
    auto &protectionGroup = form.addCardGroup(F("prot"), FSPGM(Protection), true);

    form.addPointerTriviallyCopyable(F("temp_75"), &cfg.protection.temperature_75);
    form.addFormUI(F("Temperature To Reduce Brightness To 75%"), FormUI::Suffix(FSPGM(_degreeC)));
    form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 90));

    form.addPointerTriviallyCopyable(F("temp_50"), &cfg.protection.temperature_50);
    form.addFormUI(F("Temperature To Reduce Brightness To 50%"), FormUI::Suffix(FSPGM(_degreeC)));
    form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 90));

    form.addPointerTriviallyCopyable(F("max_temp"), &cfg.protection.max_temperature);
    form.addFormUI(F("Over Temperature Protection"), FormUI::Suffix(FSPGM(_degreeC)));
    form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 105));

    protectionGroup.end();

    form.finalize();
}

#endif
