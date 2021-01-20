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
    auto animationTypeItems = FormUI::Container::List(
        AnimationType::NONE, FSPGM(Solid_Color),
        AnimationType::RAINBOW, FSPGM(Rainbow),
        AnimationType::FIRE, F("Fire"),
        AnimationType::FLASHING, FSPGM(Flashing),
        AnimationType::FADING, FSPGM(Fading),
        AnimationType::SKIP_ROWS, F("Skip Rows or Columns")
    );

    auto initialStateItems = FormUI::Container::List(
        Clock::InitialStateType::OFF, F("Turn Off"),
        Clock::InitialStateType::ON, F("Turn On"),
        Clock::InitialStateType::RESTORE, F("Restore Last State")
    );

    auto &ui = form.createWebUI();
    ui.setTitle(F("LED Matrix Configuration"));
    ui.setContainerId(F("led_matrix_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("is"), cfg, cfg.get_int_initial_state, cfg.set_int_initial_state);
    form.addFormUI(F("After Reset"), initialStateItems);

    form.addPointerTriviallyCopyable(FSPGM(brightness), &cfg.brightness);
    form.addFormUI(FormUI::Type::RANGE_SLIDER, FSPGM(Brightness), FormUI::MinMax(0, 255));

#if IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL

    form.addPointerTriviallyCopyable(F("auto_br"), &cfg.auto_brightness);
    form.addFormUI(F("Auto Brightness Value"), FormUI::SuffixHtml(F("<span class=\"input-group-text\">0-1023</span><span id=\"abr_sv\" class=\"input-group-text\"></span><button class=\"btn btn-secondary\" type=\"button\" id=\"dis_auto_br\">Disable</button>")));
    form.addValidator(FormUI::Validator::Range(-1, 1023));

#endif

    mainGroup.end();

    // --------------------------------------------------------------------
    #include "form_parts/form_animation.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_rainbow_ani.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_fading.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_fire_ani.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_skip_rows.hpp"

    // --------------------------------------------------------------------
    #include "form_parts/form_rest.hpp"

    form.finalize();
}

#endif
