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

    auto &cfg = config._H_W_GET(Config().clock);

    form.setFormUI(F("Clock Configuration"));

    form.add<bool>(F("timef"), _H_W_STRUCT_VALUE(cfg, time_format_24h))
        ->setFormUI((new FormUI(FormUI::SELECT, F("Time Format")))->setBoolItems(F("24h"), F("12h")));

    form.add<float>(FSPGM(brightness), ((int)(cfg.brightness * (1000.0 / 255.0)) + 0.5f) / 10.0f, [](float value, FormField &, bool store) {
        if (store) {
            config._H_W_GET(Config().clock).brightness = value * (255.0 / 100.0);
        }
        return false;

    })->setFormUI((new FormUI(FormUI::TEXT, F("Brightness")))->setSuffix(F("&percnt;")));
    form.addValidator(new FormRangeValidator(0.0f, 100.0f));

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

    form.add<int16_t>(F("auto_br"), _H_W_STRUCT_VALUE(cfg, auto_brightness))
        ->setFormUI((new FormUI(FormUI::TEXT, F("Auto Brightness Value")))->setSuffix(F("-1 = disable")));

#endif

    // --------------------------------------------------------------------
    auto &animationGroup = form.addHrGroup(String(), F("Animation"));

    form.add<int8_t>(F("ani"), _H_W_STRUCT_VALUE(cfg, animation))->setFormUI(
        (new FormUI(FormUI::SELECT, F("Type")))
            ->addItems(String(AnimationEnum_t::NONE), F("Solid"))
            ->addItems(String(AnimationEnum_t::RAINBOW), F("Rainbow"))
            ->addItems(String(AnimationEnum_t::FLASHING), F("Flashing"))
            ->addItems(String(AnimationEnum_t::FADE), F("Fading"))
    );
    form.addValidator(new FormRangeValidator(AnimationEnum_t::NONE, (int)AnimationEnum_t::MAX - 1));

    form.add(F("col"), Color(config._H_GET(Config().clock).solid_color.value).toString(), [](const String &value, FormField &field, bool store) {
        if (store) {
            config._H_W_GET(Config().clock).solid_color.value = Color::fromString(value);
        }
        return false;
    }, FormField::InputFieldType::TEXT)->setFormUI(new FormUI(FormUI::TEXT, F("Solid Color")));

    form.add<float>(F("colon_sp"), _H_W_STRUCT_VALUE(cfg, blink_colon_speed))->setFormUI((new FormUI(FormUI::TEXT, F("Blink Colon Speed")))->setSuffix(F("milliseconds, 0 = solid")));
    form.addValidator(new FormRangeValidator(kMinBlinkColonSpeed, 0xffff, true));

    form.add<uint16_t>(F("flash_sp"), _H_W_STRUCT_VALUE(cfg, flashing_speed))->setFormUI((new FormUI(FormUI::TEXT, F("Flashing Speed")))->setSuffix(F("milliseconds")));
    form.addValidator(new FormRangeValidator(kMinFlashingSpeed, 0xffff));

    animationGroup.end();

    // --------------------------------------------------------------------
    auto &rainbowGroup = form.addHrGroup(String(), F("Rainbow Animation"));

    form.add<float>(F("rb_mul"), _H_W_STRUCT_VALUE(cfg, rainbow.multiplier))->setFormUI((new FormUI(FormUI::TEXT, F("Multiplier"))));
    form.addValidator(new FormRangeValidator(0.1f, 100.0f));

    form.add<uint16_t>(F("rb_sp"), _H_W_STRUCT_VALUE(cfg, rainbow.speed))->setFormUI((new FormUI(FormUI::TEXT, F("Speed"))));
    form.addValidator(new FormRangeValidator(kMinRainbowSpeed, 0xffff));

    form.add(F("rb_fac"), Color(config._H_GET(Config().clock).rainbow.factor.value).toString(), [](const String &value, FormField &field, bool store) {
        if (store) {
            config._H_W_GET(Config().clock).rainbow.factor.value = Color::fromString(value);
        }
        return false;
    }, FormField::InputFieldType::TEXT)->setFormUI(new FormUI(FormUI::TEXT, F("Color Factor")));

    rainbowGroup.end();

    // --------------------------------------------------------------------
    auto &fadingGroup = form.addHrGroup(String(), F("Fading"));

    form.add<float>(F("fade_sp"), _H_W_STRUCT_VALUE(cfg, fading.speed))->setFormUI((new FormUI(FormUI::TEXT, F("Time Between Fading Colors")))->setSuffix(F("seconds")));
    form.addValidator(new FormRangeValidator(Clock::FadingAnimation::kMinSeconds, Clock::FadingAnimation::kMaxSeconds));

    form.add<uint16_t>(F("fade_dy"), _H_W_STRUCT_VALUE(cfg, fading.delay))->setFormUI((new FormUI(FormUI::TEXT, F("Delay Before Next Color Change")))->setSuffix(F("seconds")));
    form.addValidator(new FormRangeValidator(0, Clock::FadingAnimation::kMaxDelay));

    fadingGroup.end();

#if IOT_ALARM_PLUGIN_ENABLED

    // --------------------------------------------------------------------
    auto &alarmGroup = form.addHrGroup(String(), F("Alarm"));

    form.add(F("alrm_col"), Color(config._H_GET(Config().clock).alarm.color.value).toString(), [](const String &value, FormField &field, bool store) {
        if (store) {
            config._H_W_GET(Config().clock).alarm.color.value = Color::fromString(value);
        }
        return false;
    }, FormField::InputFieldType::TEXT)->setFormUI(new FormUI(FormUI::TEXT, F("Color")));

    form.add<uint16_t>(F("alrm_sp"), _H_W_STRUCT_VALUE(cfg, alarm.speed))->setFormUI((new FormUI(FormUI::TEXT, F("Flashing Speed")))->setSuffix(F("milliseconds")));
    form.addValidator(new FormRangeValidator(50, 0xffff));

    alarmGroup.end();

#endif

    // --------------------------------------------------------------------
    auto &protectionGroup = form.addHrGroup(String(), F("Protection"));

    form.add<uint8_t>(F("temp_75"), _H_W_STRUCT_VALUE(cfg, protection.temperature_75))
        ->setFormUI((new FormUI(FormUI::TEXT, F("Temperature to reduce brightness to 75%")))->setSuffix(FSPGM(_degreeC)));

    form.add<uint8_t>(F("temp_50"), _H_W_STRUCT_VALUE(cfg, protection.temperature_50))
        ->setFormUI((new FormUI(FormUI::TEXT, F("Temperature to reduce brightness to 50%")))->setSuffix(FSPGM(_degreeC)));

    form.add<uint8_t>(F("max_temp"), _H_W_STRUCT_VALUE(cfg, protection.max_temperature))
        ->setFormUI((new FormUI(FormUI::TEXT, F("Over temperature protection")))->setSuffix(FSPGM(_degreeC)));

    protectionGroup.end();

    form.finalize();
}