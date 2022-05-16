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

    FormUI::Group *mainGroup;
    auto &cfg = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation::getWriteableConfig();
    auto &ui = form.createWebUI();
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    if (formName.equals(F("world-clock"))) {

        ui.setTitle(F("World Clock Configuration"));
        ui.setContainerId(F("wc_settings"));

        using WeatherStation = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation;
        mainGroup = &form.addCardGroup(F("w_clock"));

        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, WEATHER_STATION_MAX_CLOCKS, nm, tf);

        for(uint8_t i = 0; i < WEATHER_STATION_MAX_CLOCKS; i++) {

            form.addCallbackSetter(F_VAR(nm, i), WeatherStation::getTZ(i), [&cfg, i](const String &value, FormUI::Field::BaseField &field) {
                // cfg.setTZ(value.c_str(), i);
            });
            form.addFormUI(F("Display Name"));

            form.addObjectGetterSetter(F_VAR(tf, i), FormGetterSetter(cfg.additionalClocks[i], _time_format_24h));
            form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

            // form.addCallbackSetter(F_VAR(nm, i), String(cfg.additionalClocks[i].getTZ()), [&cfg, i](const String &value, FormUI::Field::BaseField &field) {
            //     cfg.additionalClocks[i].setTZ(value.c_str());
            // });
            // form.addFormUI(F("Timezone"));

            // form.addStringGetterSetter(F_VAR(tz, i), Plugins::NTPClient::getPosixTimezone, Plugins::NTPClient::setPosixTimezone);
            // form.addFormUI(FormUI::Type::SELECT, FSPGM(Timezone, "Timezone"));
            // form.addStringGetterSetter(F("tz_name"), Plugins::NTPClient::getTimezoneName, Plugins::NTPClient::setTimezoneName);
            // form.addFormUI(FormUI::Type::HIDDEN);

        }

    }
    else {

        ui.setTitle(F("Weather Station Configuration"));
        ui.setContainerId(F("ws_settings"));

        mainGroup = &form.addCardGroup(FSPGM(config));

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

        #if DEBUG_IOT_WEATHER_STATION
        #define NUM_SCREENS 7
        #else
        #define NUM_SCREENS 6
        #endif
        static_assert(NUM_SCREENS == WSDraw::kNumScreens, "number does not match");
        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, NUM_SCREENS, st/*, ds*/);
        #undef NUM_SCREENS
        // auto modeList = FormUI::Container::List(
        //     kDefaultScreenTimeInSeconds, F("Change Automatically"),
        //     kSkipScreen, F("Hide Screen"),
        //     kManualScreen, F("Manual Selection")
        // );

        for(uint8_t i = 0; i < WSDraw::kNumScreens; i++) {

            // auto &toggleModeHidden = form.addCallbackGetterSetter<uint8_t>(F_VAR(ds, i), [&cfg, i](uint8_t &value, Field::BaseField &field, bool store) {
            //     if (store) {
            //         if (value == kDefaultScreenTimeInSeconds) {
            //             // store default time only if the mode was manual or skip before
            //             switch(cfg.screenTimer[i]) {
            //                 case kManualScreen:
            //                 case kSkipScreen:
            //                     cfg.screenTimer[i] = value;
            //                     break;
            //             }
            //         }
            //     }
            //     else {
            //         switch(cfg.screenTimer[i]) {
            //             case kDefaultScreenTimeInSeconds:
            //             case kSkipScreen:
            //             case kManualScreen:
            //                 value = cfg.screenTimer[i];
            //                 break;
            //             default:
            //                 value = kDefaultScreenTimeInSeconds;
            //                 break;
            //         }
            //     }
            //     __LDBG_printf("i=%u checked=%u store=%u", i, value, store);
            //     return true;
            // });
            // form.addFormUI(FormUI::Type::HIDDEN, modeList);

            // auto &checkbox = form.addCallbackGetterSetter<bool>(F_VAR(ds, i),
            //     if (store) {
            //         if (value) {
            //             cfg.screenTimer[i] = kSkipScreen;
            //         }
            //         else if (cfg.screenTimer[i] == kSkipScreen) {
            //             cfg.screenTimer[i] = kManualScreen;
            //         }
            //     }
            //     else {
            //         value = (cfg.screenTimer[i] == kSkipScreen);
            //     }
            //     __LDBG_printf("i=%u checked=%u store=%u", i, value, store);
            //     return true;
            // });
            // form.addFormUI(FormUI::Type::HIDDEN);
            // bool isSkipChecked = checkbox.getValue().toInt();
            // __LDBG_printf("i=%u checkedbox_value=%s", i, checkbox.getValue().c_str());

            form.addCallbackGetterSetter<uint8_t>(F_VAR(st, i), [&cfg, i](uint8_t &value, Field::BaseField &field, bool store) {
                if (store) {
                    cfg.screenTimer[i] = value;
                }
                else {
                    value = cfg.screenTimer[i];
                }
                __LDBG_printf("i=%u value=%u store=%u", i, value, store);
                return true;
            });
            form.addFormUI(FormUI::Label(WSDraw::Base::getScreenName(i)), FormUI::Suffix(FSPGM(seconds)));
            // , FormUI::CheckboxButtonSuffix(toggleModeHidden, FSPGM(HIDDEN, "HIDDEN")));
            // FormUI::CheckboxButtonSuffix(checkbox, F("Skip Screen")));
            form.addValidator(FormUI::Validator::Range(0, 255));
        }

        form.addObjectGetterSetter(F("stft"), FormGetterSetter(cfg, show_webui));
        form.addFormUI(F("Show TFT Contents In WebUI"), FormUI::BoolItems(FSPGM(Yes), FSPGM(No)));

    }

    mainGroup->end();
    form.finalize();
}
