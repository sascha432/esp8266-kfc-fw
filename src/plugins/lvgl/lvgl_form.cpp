/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_plugin.h"

#if IOT_LVGL_SUPPORT

#include <KFCForms.h>
#include <kfc_fw_config.h>

using Display = KFCConfigurationClasses::Plugins::DisplayConfigNS::Display;
using DisplayConfig = Display::Config_t;

void LVGLPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        // the display configuration is written by the framework and applied by reconfigure()
        return;
    }

    auto &cfg = Display::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setTitle(F("LVGL Display Configuration"));
    ui.setContainerId(F("lvgl_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &group = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("bl"), FormGetterSetter(cfg, backlight_level));
    form.addFormUI(F("Display Brightness"), FormUI::Suffix(F("%")));
    cfg.addRangeValidatorFor_backlight_level(form);

    group.end();

    auto &powerGroup = form.addCardGroup(F("pws"), F("Power Saving"), true);

    // The power saving members were added to the display configuration later. The stored blob of
    // an existing device is zero filled when it is resized to the new length, a value outside the
    // valid range means "not initialized" and the default is used until the form is saved
    form.addCallbackGetterSetter<uint8_t>(F("pbl"), [&cfg](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.power_saving_level = value;
        }
        else {
            value = cfg.power_saving_level;
            if (value < DisplayConfig::kMinValueFor_power_saving_level || value > DisplayConfig::kMaxValueFor_power_saving_level) {
                value = DisplayConfig::kDefaultValueFor_power_saving_level;
            }
        }
        return true;
    });
    form.addFormUI(F("Idle Brightness"),
        FormUI::PlaceHolder(DisplayConfig::kDefaultValueFor_power_saving_level),
        FormUI::MinMax(DisplayConfig::kMinValueFor_power_saving_level, DisplayConfig::kMaxValueFor_power_saving_level),
        FormUI::Type::NUMBER_RANGE, FormUI::Suffix(F("%")));
    form.addValidator(FormUI::Validator::Range(DisplayConfig::kMinValueFor_power_saving_level, DisplayConfig::kMaxValueFor_power_saving_level, false));

    // The two timeouts are new members of the display configuration, see above
    form.addCallbackGetterSetter<uint32_t>(F("pto"), [&cfg](uint32_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.power_saving_timeout = value;
        }
        else {
            value = cfg.power_saving_timeout;
            if (value < DisplayConfig::kMinValueFor_power_saving_timeout || value > DisplayConfig::kMaxValueFor_power_saving_timeout) {
                value = DisplayConfig::kDefaultValueFor_power_saving_timeout;
            }
        }
        return true;
    });
    form.addFormUI(F("Idle Timeout"),
        FormUI::PlaceHolder(DisplayConfig::kDefaultValueFor_power_saving_timeout),
        FormUI::MinMax(DisplayConfig::kMinValueFor_power_saving_timeout, DisplayConfig::kMaxValueFor_power_saving_timeout),
        FormUI::Type::NUMBER_RANGE, FormUI::Attribute(F("step"), 10), FormUI::Suffix(F("seconds")));
    form.addValidator(FormUI::Validator::Range(DisplayConfig::kMinValueFor_power_saving_timeout, DisplayConfig::kMaxValueFor_power_saving_timeout, false));

    form.addCallbackGetterSetter<uint32_t>(F("sbt"), [&cfg](uint32_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.standby_timeout = value;
        }
        else {
            value = cfg.standby_timeout;
            if (value < DisplayConfig::kMinValueFor_standby_timeout || value > DisplayConfig::kMaxValueFor_standby_timeout) {
                value = DisplayConfig::kDefaultValueFor_standby_timeout;
            }
        }
        return true;
    });
    form.addFormUI(F("Standby Timeout"),
        FormUI::PlaceHolder(DisplayConfig::kDefaultValueFor_standby_timeout),
        FormUI::MinMax(DisplayConfig::kMinValueFor_standby_timeout, DisplayConfig::kMaxValueFor_standby_timeout),
        FormUI::Type::NUMBER_RANGE, FormUI::Attribute(F("step"), 10), FormUI::Suffix(F("seconds")));
    form.addValidator(FormUI::Validator::Range(DisplayConfig::kMinValueFor_standby_timeout, DisplayConfig::kMaxValueFor_standby_timeout, false));

    powerGroup.end();
}

#endif
