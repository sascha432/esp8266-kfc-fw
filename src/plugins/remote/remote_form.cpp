/**
  Author: sascha_lammers@gmx.de
*/

#include <Form.h>
#include "Utility/ProgMemHelper.h"
#include "remote_form.h"
#include "remote.h"

using KFCConfigurationClasses::MainConfig;
using KFCConfigurationClasses::Plugins;

void RemoteControlPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &ui = form.createWebUI();
    ui.setTitle(F("Remote Control Configuration"));
    ui.setContainerId(F("remotectrl_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
    auto &cfg = Plugins::RemoteControl::getWriteableConfig();

    auto &mainGroup = form.addCardGroup(F("main"), F("General"));

    form.addObjectGetterSetter(F("auto_sleep_time"), cfg, cfg.get_bits_autoSleepTime, cfg.set_bits_autoSleepTime);
    form.addFormUI(F("Auto Sleep Time"), FormUI::Suffix(F("seconds")));
    cfg.addRangeValidatorFor_autoSleepTime(form);

    form.addObjectGetterSetter(F("deep_sleep_time"), cfg, cfg.get_bits_deepSleepTime, cfg.set_bits_deepSleepTime);
    form.addFormUI(F("Deep Sleep Time"), FormUI::Suffix(F("seconds (0 = indefinitely)")));
    cfg.addRangeValidatorFor_deepSleepTime(form);

    form.addObjectGetterSetter(F("long_press_time"), cfg, cfg.get_bits_longpressTime, cfg.set_bits_longpressTime);
    form.addFormUI(F("Long Press Time"), FormUI::Suffix(F("milliseconds")));
    cfg.addRangeValidatorFor_longpressTime(form);

    form.addObjectGetterSetter(F("repeat_time"), cfg, cfg.get_bits_repeatTime, cfg.set_bits_repeatTime);
    form.addFormUI(F("Repeat Time"), FormUI::Suffix(F("milliseconds")));
    cfg.addRangeValidatorFor_repeatTime(form);

    mainGroup.end();

    FormUI::Container::List actions(0, F("None"));
    Plugins::HomeAssistant::ActionVector vector;
    Plugins::HomeAssistant::getActions(vector);

    for(const auto &action: vector) {
        auto str = action.getEntityId();
        str += F(": ");
        str += action.getActionFStr();
        //auto str = PrintString(F("%s: %s"), action.getEntityId().c_str(), action.getActionFStr());
        if (action.getNumValues()) {
            // str.printf_P(PSTR(" %d"), action.getValue(0));
            str += ' ';
            str += String(action.getValue(0));
        }
        actions.emplace_back(action.getId(), str);
    }

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_REMOTE_CONTROL_BUTTON_COUNT, grp, spa, lpa, ra);

    for(uint8_t i = 0; i < _buttons.size(); i++) {

        auto &group = form.addGroup(F_VAR(grp, i), PrintString(F("Button %u Action"), i + 1),
            (cfg.actions[i].shortpress || cfg.actions[i].longpress || cfg.actions[i].repeat)
        );

        form.addObjectGetterSetter(F_VAR(spa, i), cfg.actions[i], cfg.actions[0].get_bits_shortpress, cfg.actions[0].set_bits_shortpress);
        form.addFormUI(F("Short Press"), actions);

        form.addObjectGetterSetter(F_VAR(lpa, i), cfg.actions[i], cfg.actions[0].get_bits_longpress, cfg.actions[0].set_bits_longpress);
        form.addFormUI(F("Long Press"), actions);

        form.addObjectGetterSetter(F_VAR(ra, i), cfg.actions[i], cfg.actions[0].get_bits_repeat, cfg.actions[0].set_bits_repeat);
        form.addFormUI(F("Repeat"), actions);

        group.end();
    }

    form.finalize();
}
