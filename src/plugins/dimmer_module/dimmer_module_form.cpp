/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintHtmlEntitiesString.h>
#include "../include/templates.h"
#include "progmem_data.h"
#include "plugins.h"
#if IOT_ATOMIC_SUN_V2
#include "../atomic_sun/atomic_sun_v2.h"
#else
#include "dimmer_module.h"
#endif

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

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

    PrintHtmlEntitiesString code;
#if IOT_ATOMIC_SUN_V2
    auto dimmerInstance = Driver_4ChDimmer::getInstance();
    if (dimmerInstance) {
        auto discovery = dimmerInstance->createAutoDiscovery(MQTTAutoDiscovery::FORMAT_YAML);
        code.print(discovery->getPayload());
        delete discovery;
    }
#else
    auto dimmerInstance = Driver_DimmerModule::getInstance();
    if (dimmerInstance) {
        dimmerInstance->createAutoDiscovery(MQTTAutoDiscovery::FORMAT_YAML, code);
    }
#endif
    
    reinterpret_cast<SettingsForm &>(form).getTokens().push_back(std::make_pair<String, String>(F("HASS_YAML"), code.c_str()));
}
