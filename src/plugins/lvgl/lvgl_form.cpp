/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_plugin.h"

#if IOT_LVGL_SUPPORT

#include <KFCForms.h>
#include <kfc_fw_config.h>

using Display = KFCConfigurationClasses::Plugins::DisplayConfigNS::Display;

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
}

#endif
