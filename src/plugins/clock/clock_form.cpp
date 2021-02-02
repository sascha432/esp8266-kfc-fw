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

    auto &cfg = getWriteableConfig();

    auto animationTypeItems = FormUI::Container::List(
        AnimationType::SOLID, FSPGM(Solid_Color),
        AnimationType::RAINBOW, FSPGM(Rainbow),
        AnimationType::FLASHING, FSPGM(Flashing),
        AnimationType::FADING, F("Color Fade")
    );

    auto &ui = form.createWebUI();
    ui.setTitle(F("Clock Configuration"));
    ui.setContainerId(F("clock_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("timef"), cfg, cfg.get_bits_time_format_24h, cfg.set_bits_time_format_24h);
    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    // --------------------------------------------------------------------
    #include "form_parts/form_start.hpp"

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
