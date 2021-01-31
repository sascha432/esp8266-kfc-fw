/**
 * Author: sascha_lammers@gmx.de
 */

#if !IOT_LED_MATRIX

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

    auto animation = _config.getAnimation();
    auto &cfg = Plugins::Clock::getWriteableConfig();
    cfg.solid_color = _color.get();
    cfg.animation = static_cast<uint8_t>(animation);
    cfg.brightness = _targetBrightness >> 8;

    auto animationTypeItems = FormUI::Container::List(
        AnimationType::NONE, FSPGM(Solid_Color),
        AnimationType::RAINBOW, FSPGM(Rainbow),
        AnimationType::FLASHING, FSPGM(Flashing),
        AnimationType::FADING, F("Color Fading")
    );

    auto &ui = form.createWebUI();
    ui.setTitle(F("Clock Configuration"));
    ui.setContainerId(F("clock_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("timef"), cfg, cfg.get_bits_time_format_24h, cfg.set_bits_time_format_24h);

    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    form.addPointerTriviallyCopyable(FSPGM(brightness), &cfg.brightness);
    form.addFormUI(FormUI::Type::RANGE_SLIDER, FSPGM(Brightness), FormUI::MinMax(0, 255));

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

    form.addPointerTriviallyCopyable(F("auto_br"), &cfg.auto_brightness);
    form.addFormUI(F("Auto Brightness Value"), FormUI::SuffixHtml(F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>")));
    form.addValidator(FormUI::Validator::Range(-1, 1023));

#endif

    form.addObjectGetterSetter(F("ft"), cfg, cfg.get_bits_fading_time, cfg.set_bits_fading_time);
    form.addFormUI(F("Fading Time From 0 To 100%"), FormUI::SuffixHtml(F("milliseconds")));
    cfg.addRangeValidatorFor_fading_time(form, true);

    mainGroup.end();

    // --------------------------------------------------------------------
    #include "form_parts/form_animation.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_rainbow_ani.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_fading.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_rest.hpp"

    form.finalize();
}

#endif
