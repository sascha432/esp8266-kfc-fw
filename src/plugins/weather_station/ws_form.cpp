/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "weather_station.h"
#include <KFCForms.h>
#include <kfc_fw_config.h>

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using WeatherStationX = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation;

void WeatherStationPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = WeatherStationX::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setTitle(F("Weather Station Configuration"));
    ui.setContainerId(F("dimmer_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("tf"), FormGetterSetter(cfg, time_format_24h));
    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    form.addObjectGetterSetter(F("im"), FormGetterSetter(cfg, is_metric));
    form.addFormUI(F("Units"), FormUI::BoolItems(F("Metric"), F("Imperial")));

    form.addObjectGetterSetter(F("api"), FormGetterSetter(cfg, weather_poll_interval));
    form.addFormUI(F("Weather Poll Interval"), FormUI::Suffix(FSPGM(minutes)));
    form.addValidator(FormUI::Validator::Range(5, 240));

    form.addObjectGetterSetter(F("ato"), FormGetterSetter(cfg, api_timeout));
    form.addFormUI(F("API Timeout"), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::Range(30, 900));

    form.addObjectGetterSetter(F("bll"), FormGetterSetter(cfg, backlight_level));
    form.addFormUI(F("Backlight Level"), FormUI::Suffix(F("%")));
    form.addValidator(FormUI::Validator::Range(0, 100));

    form.addObjectGetterSetter(F("tth"), FormGetterSetter(cfg, touch_threshold));
    form.addFormUI(F("Touch Threshold"));

    form.addObjectGetterSetter(F("trt"), FormGetterSetter(cfg, released_threshold));
    form.addFormUI(F("Release Threshold"));


    for(uint8_t i = 0; i < WSDraw::kNumScreens; i++) {
        PrintString str(F("Screen #%u, %s:"), i + 1, WeatherStationPlugin::getScreenName(i));
        // form.add(PrintString(F("st_%u"), i), _H_W_STRUCT_VALUE_TYPE(cfg, screenTimer[i], uint8_t, i));
        // form.addFormUI(FormUI::Label(str, true), FormUI::Suffix(FSPGM(seconds)));
    }

    form.addObjectGetterSetter(F("stft"), FormGetterSetter(cfg, show_webui));
    form.addFormUI(F("Show TFT Contents In WebUI"), FormUI::BoolItems(FSPGM(Yes), FSPGM(No)));

    mainGroup.end();

    form.finalize();
}
