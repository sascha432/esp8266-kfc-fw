/**
 * Author: sascha_lammers@gmx.de
 */

#include "../include/templates.h"
#include "progmem_data.h"
#include "plugins.h"

void dimmer_module_create_settings_form(AsyncWebServerRequest *request, Form &form) {

    auto dimmer = config._H_W_GET(Config().dimmer);

    form.add<float>(F("fade_time"), &dimmer.fade_time);

    form.add<float>(F("on_fade_time"), &dimmer.on_fade_time);

    form.add<float>(F("linear_correction"), &dimmer.linear_correction);

    form.add<uint8_t>(F("restore_level"), &dimmer.restore_level);

    form.add<uint8_t>(F("max_temperature"), &dimmer.max_temperature);
    form.addValidator(new FormRangeValidator(F("Invalid temperature"), 45, 150));

    form.add<uint8_t>(F("temp_check_int"), &dimmer.temp_check_int);
    form.addValidator(new FormRangeValidator(F("Invalid interval"), 10, 255));

    form.finalize();
}
