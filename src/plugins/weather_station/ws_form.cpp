/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "weather_station.h"
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include "Utility/ProgMemHelper.h"

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void WeatherStationPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setTitle(F("Weather Station Configuration"));
    ui.setContainerId(F("ws_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("tf"), FormGetterSetter(cfg, time_format_24h));
    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    form.addObjectGetterSetter(F("im"), FormGetterSetter(cfg, is_metric));
    form.addFormUI(F("Units"), FormUI::BoolItems(F("Metric"), F("Imperial")));

    form.addObjectGetterSetter(F("api"), FormGetterSetter(cfg, weather_poll_interval));
    form.addFormUI(F("Weather Poll Interval"), FormUI::Suffix(FSPGM(minutes)));
    cfg.addRangeValidatorFor_weather_poll_interval(form);

    form.addObjectGetterSetter(F("ato"), FormGetterSetter(cfg, api_timeout));
    form.addFormUI(F("API Timeout"), FormUI::Suffix(FSPGM(seconds)));
    cfg.addRangeValidatorFor_api_timeout(form);

    form.addObjectGetterSetter(F("bll"), FormGetterSetter(cfg, backlight_level));
    form.addFormUI(F("Backlight Level"), FormUI::Suffix(F("%")));
    cfg.addRangeValidatorFor_backlight_level(form);

    form.addObjectGetterSetter(F("tth"), FormGetterSetter(cfg, touch_threshold));
    form.addFormUI(F("Touch Threshold"));
    cfg.addRangeValidatorFor_touch_threshold(form);

    form.addObjectGetterSetter(F("trt"), FormGetterSetter(cfg, released_threshold));
    form.addFormUI(F("Release Threshold"));
    cfg.addRangeValidatorFor_released_threshold(form);

    #define NUM_SCREENS 6
    static_assert(NUM_SCREENS == WSDraw::kNumScreens, "number does not match");
    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, NUM_SCREENS, st, ds);
    #undef NUM_SCREENS

    for(uint8_t i = 0; i < WSDraw::kNumScreens; i++) {
        auto &checkbox = form.addCallbackGetterSetter<bool>(F_VAR(ds, i), [&cfg, i](bool &value, Field::BaseField &field, bool store) {
            if (store) {
                if (value) {
                    cfg.screenTimer[i] = 255;
                }
                else if (cfg.screenTimer[i] == 255) {
                    cfg.screenTimer[i] = 0;
                }
            }
            else {
                value = (cfg.screenTimer[i] == 255);
            }
            __LDBG_printf("i=%u checked=%u store=%u", i, value, store);
            return true;
        });
        form.addFormUI(FormUI::Type::HIDDEN);
        bool isSkipChecked = checkbox.getValue().toInt();
        __LDBG_printf("i=%u checkedbox_value=%s", i, checkbox.getValue().c_str());

        form.addCallbackGetterSetter<uint8_t>(F_VAR(st, i), [&cfg, i, isSkipChecked](uint8_t &value, Field::BaseField &field, bool store) {
            if (store) {
                cfg.screenTimer[i] = value;
            }
            else {
                value = cfg.screenTimer[i];
            }
            __LDBG_printf("i=%u value=%u store=%u", i, value, store);
            return true;
        });
        form.addFormUI(FormUI::Label(WSDraw::Base::getScreenName(i)), FormUI::Suffix(FSPGM(seconds)), FormUI::CheckboxButtonSuffix(checkbox, F("Skip Screen")));
        // if (cfg.screenTimer[i] != 255 && !isSkipChecked) {
        //     form.addValidator(FormUI::Validator::Range(0, 240));
        // }
    }

    form.addObjectGetterSetter(F("stft"), FormGetterSetter(cfg, show_webui));
    form.addFormUI(F("Show TFT Contents In WebUI"), FormUI::BoolItems(FSPGM(Yes), FSPGM(No)));

    mainGroup.end();

    form.finalize();
}
