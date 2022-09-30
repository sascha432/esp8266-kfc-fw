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

        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, WEATHER_STATION_MAX_CLOCKS, nm, tf, tz, tn);

        for(uint8_t i = 0; i < WEATHER_STATION_MAX_CLOCKS; i++) {

            form.addCallbackSetter(F_VAR(nm, i), WeatherStation::getName(i), [&cfg, i](const String &value, FormUI::Field::BaseField &field) {
                cfg.additionalClocks[i]._set__enabled(value.length() ? 1 : 0);
                WeatherStation::setName(i, value.c_str());
            });
            form.addFormUI(F("Display Name"), FormUI::PlaceHolder(F("Disabled")));
            form.addValidator(FormUI::Validator::Length(4, 16, true));

            form.addCallbackSetter(F_VAR(tz, i), WeatherStation::getTZ(i), [i](const String &value, FormUI::Field::BaseField &field) {
                WeatherStation::setTZ(i, value.c_str());
            }).setOptional(true);
            form.addFormUI(FormUI::Type::SELECT, F("Time Zone"));

            form.addCallbackSetter(F_VAR(tn, i), WeatherStation::getTZName(i), [i](const String &value, FormUI::Field::BaseField &field) {
                WeatherStation::setTZName(i, value.c_str());
            });
            form.addFormUI(FormUI::Type::HIDDEN);

            form.addObjectGetterSetter(F_VAR(tf, i), FormGetterSetter(cfg.additionalClocks[i], _time_format_24h));
            form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

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

        #define NUM_SCREENS 5//8
        static_assert(NUM_SCREENS == WSDraw::kNumScreens, "since Boost cannot use constexpr or enum, update macro by hand");
        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, NUM_SCREENS, st/*, ds*/);
        #undef NUM_SCREENS

        // auto modeList = FormUI::Container::List(
        //     kDefaultScreenTimeInSeconds, F("Change Automatically"),
        //     kSkipScreen, F("Hide Screen"),
        //     kManualScreen, F("Manual Selection")
        // );

        for(uint8_t i = 0; i < WSDraw::kNumScreens; i++) {

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
            form.addValidator(FormUI::Validator::Range(0, 255));

            #if HAVE_WEATHER_STATION_CURATED_ART
                if (static_cast<ScreenType>(i) == ScreenType::CURATED_ART) {
                    form.addObjectGetterSetter(F("gur"), FormGetterSetter(cfg, gallery_update_rate));
                    form.addFormUI(F("Curated Art Update Rate"), FormUI::Suffix(FSPGM(seconds)));
                    cfg.addRangeValidatorFor_gallery_update_rate(form);
                }
            #endif

        }

    }

    mainGroup->end();
    form.finalize();
}
