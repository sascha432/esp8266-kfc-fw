/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX

#include <Arduino_compat.h>
#include "clock.h"
#include <KFCForms.h>
#include "../src/plugins/sensor/sensor.h"

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
        AnimationType::FIRE, F("Fire"),
        AnimationType::FLASHING, FSPGM(Flashing),
        AnimationType::FADING, F("Color Fading"),
        AnimationType::SKIP_ROWS, F("Skip Rows or Columns")
    );

    auto &ui = form.createWebUI();
    ui.setTitle(F("LED Matrix Configuration"));
    ui.setContainerId(F("led_matrix_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

#if IOT_CLOCK_SAVE_STATE
    auto initialStateItems = FormUI::Container::List(
        Clock::InitialStateType::OFF, F("Turn Off"),
        Clock::InitialStateType::ON, F("Turn On"),
        Clock::InitialStateType::RESTORE, F("Restore Last State")
    );

    form.addObjectGetterSetter(F("is"), cfg, cfg.get_int_initial_state, cfg.set_int_initial_state);
    form.addFormUI(F("After Reset"), initialStateItems);
#endif

    // --------------------------------------------------------------------
    #include "form_parts/form_start.hpp"

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
