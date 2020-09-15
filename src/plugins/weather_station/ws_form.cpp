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

using KFCConfigurationClasses::Plugins;

void WeatherStationPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::WeatherStation::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setTitle(F("Weather Station Configuration"));
    ui.setContainerId(F("dimmer_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config), String());

    form.add(F("tf"), _H_W_STRUCT_VALUE(cfg, time_format_24h));
    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    form.add(F("im"), _H_W_STRUCT_VALUE(cfg, is_metric));
    form.addFormUI(F("Units"), FormUI::BoolItems(F("Metric"), F("Imperial")));

    form.add(F("api"), _H_W_STRUCT_VALUE(cfg, weather_poll_interval));
    form.addFormUI(F("Weather Poll Interval"), FormUI::Suffix(FSPGM(minutes)));
    form.addValidator(FormUI::Validator::Range(5, 240));

    form.add(F("ato"), _H_W_STRUCT_VALUE(cfg, api_timeout));
    form.addFormUI(F("API Timeout"), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::Range(30, 900));

    form.add(F("bll"), _H_W_STRUCT_VALUE(cfg, backlight_level));
    form.addFormUI(F("Backlight Level"), FormUI::Suffix('%'));
    form.addValidator(FormUI::Validator::Range(0, 100));

    form.add(F("tth"), _H_W_STRUCT_VALUE(cfg, touch_threshold));
    form.addFormUI(F("Touch Threshold"));

    form.add(F("trt"), _H_W_STRUCT_VALUE(cfg, released_threshold));
    form.addFormUI(F("Release Threshold"));

    form.add(F("t_ofs"), _H_W_STRUCT_VALUE(cfg, temp_offset));
    form.addFormUI(F("BMP280 Temperature Offset"), FormUI::Suffix('%'));

    form.add(F("h_ofs"), _H_W_STRUCT_VALUE(cfg, humidity_offset));
    form.addFormUI(F("Humidity Offset"), FormUI::Suffix('%'));

    form.add(F("p_ofs"), _H_W_STRUCT_VALUE(cfg, pressure_offset));
    form.addFormUI(F("Pressure Offset"), FormUI::Suffix(FSPGM(hPa)));

    for(uint8_t i = 0; i < WeatherStationPlugin::ScreenEnum_t::NUM_SCREENS; i++) {
        PrintString str;
        if (i == 0) {
            str = F("Display screen for the specified time and switch to next one.<br>");
        }
        str.printf_P(PSTR("Screen #%u, %s:"), i + 1, WeatherStationPlugin::getScreenName(i));

        form.add(PrintString(F("st_%u"), i), _H_W_STRUCT_VALUE_TYPE(cfg, screenTimer[i], uint8_t, i));
        form.addFormUI(FormUI::Label(str, true), FormUI::Suffix(FSPGM(seconds)));
    }

    form.add(F("stft"), _H_W_STRUCT_VALUE(cfg, show_webui));
    form.addFormUI(F("Show TFT Contents In WebUI"), FormUI::BoolItems(FSPGM(Yes), FSPGM(No)));

    mainGroup.end();

    form.finalize();
}
