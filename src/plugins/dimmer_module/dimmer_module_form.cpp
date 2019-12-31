/**
 * Author: sascha_lammers@gmx.de
 */


#if IOT_ATOMIC_SUN_V2 || IOT_DIMMER_MODULE

#include "dimmer_module_form.h"
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

void DimmerModuleForm::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    _debug_printf_P(PSTR("DimmerModuleForm::createConfigureForm()\n"));
    auto *dimmer = &config._H_W_GET(Config().dimmer);

    // form.setFormUI(F("Dimmer Configuration"));
    // auto seconds = String(F("seconds"));

    form.add<float>(F("fade_time"), &dimmer->fade_time); //->setFormUI((new FormUI(FormUI::TEXT, F("Fade in/out time")))->setPlaceholder(String(5.0, 1))->setSuffix(seconds));

    form.add<float>(F("on_fade_time"), &dimmer->on_fade_time); //->setFormUI((new FormUI(FormUI::TEXT, F("Turn on/off fade time")))->setPlaceholder(String(7.5, 1))->setSuffix(seconds));

    form.add<float>(F("linear_correction"), &dimmer->linear_correction); //->setFormUI((new FormUI(FormUI::TEXT, F("Linear correction factor")))->setPlaceholder(String(1.0, 1)));

    form.add<uint8_t>(F("restore_level"), &dimmer->restore_level); //->setFormUI((new FormUI(FormUI::SELECT, F("On power failure")))->setBoolItems(F("Restore last brightness level"), F("Do not turn on")));

    form.add<uint8_t>(F("max_temperature"), &dimmer->max_temperature); //->setFormUI((new FormUI(FormUI::TEXT, F("Max. temperature")))->setPlaceholder(String(75))->setSuffix(F("&deg;C")));
    form.addValidator(new FormRangeValidator(F("Temperature out of range: %min%-%max%"), 45, 110));

    form.add<uint8_t>(F("metrics_int"), &dimmer->metrics_int); //->setFormUI((new FormUI(FormUI::TEXT, F("Metrics report interval")))->setPlaceholder(String(30))->setSuffix(seconds));
    form.addValidator(new FormRangeValidator(F("Invalid interval: %min%-%max%"), 10, 255));

#if IOT_DIMMER_MODULE_HAS_BUTTONS

    auto *dimmer_buttons = &config._H_W_GET(Config().dimmer_buttons);
    //auto milliseconds = String(F("milliseconds"));
    //auto percent = String(F("&#37;"));

    form.add<uint16_t>(F("shortpress_time"), &dimmer_buttons->shortpress_time); //->setFormUI((new FormUI(FormUI::TEXT, F("Short press time")))->setPlaceholder(String(250))->setSuffix(milliseconds));
    form.addValidator(new FormRangeValidator(F("Invalid time"), 50, 1000));

    form.add<uint16_t>(F("longpress_time"), &dimmer_buttons->longpress_time); //->setFormUI((new FormUI(FormUI::TEXT, F("Long press time")))->setPlaceholder(String(600))->setSuffix(milliseconds));
    form.addValidator(new FormRangeValidator(F("Invalid time"), 250, 2000));

    form.add<uint16_t>(F("repeat_time"), &dimmer_buttons->repeat_time); //->setFormUI((new FormUI(FormUI::TEXT, F("Hold/repeat time")))->setPlaceholder(String(150))->setSuffix(milliseconds));
    form.addValidator(new FormRangeValidator(F("Invalid time"), 50, 500));

    form.add<uint8_t>(F("shortpress_step"), &dimmer_buttons->shortpress_step); //->setFormUI((new FormUI(FormUI::TEXT, F("Brightness steps")))->setPlaceholder(String(5))->setSuffix(percent));
    form.addValidator(new FormRangeValidator(F("Invalid level"), 1, 100));

    form.add<uint16_t>(F("shortpress_no_repeat_time"), &dimmer_buttons->shortpress_no_repeat_time); //->setFormUI((new FormUI(FormUI::TEXT, F("Short press down = off/no repeat time")))->setPlaceholder(String(800))->setSuffix(milliseconds));
    form.addValidator(new FormRangeValidator(F("Invalid time"), 250, 2500));

    form.add<uint8_t>(F("min_brightness"), &dimmer_buttons->min_brightness); //->setFormUI((new FormUI(FormUI::TEXT, F("Min. brightness")))->setPlaceholder(String(15))->setSuffix(percent));
    form.addValidator(new FormRangeValidator(F("Invalid brightness"), 0, 100));

    form.add<uint8_t>(F("longpress_max_brightness"), &dimmer_buttons->longpress_max_brightness); //->setFormUI((new FormUI(FormUI::TEXT, F("Long press up/max. brightness")))->setPlaceholder(String(100))->setSuffix(percent));
    form.addValidator(new FormRangeValidator(F("Invalid brightness"), 0, 100));

    form.add<uint8_t>(F("longpress_min_brightness"), &dimmer_buttons->longpress_min_brightness); //->setFormUI((new FormUI(FormUI::TEXT, F("Long press down/min. brightness")))->setPlaceholder(String(33))->setSuffix(percent));
    form.addValidator(new FormRangeValidator(F("Invalid brightness"), 0, 100));

    form.add<float>(F("shortpress_fadetime"), &dimmer_buttons->shortpress_fadetime); //->setFormUI((new FormUI(FormUI::TEXT, F("Short press fade time")))->setPlaceholder(String(1.0, 1))->setSuffix(seconds));

    form.add<float>(F("longpress_fadetime"), &dimmer_buttons->longpress_fadetime); //->setFormUI((new FormUI(FormUI::TEXT, F("Long press fade time")))->setPlaceholder(String(5.0, 1))->setSuffix(seconds));

    form.add<uint8_t>(F("pin0"), &dimmer_buttons->pins[0]); //->setFormUI((new FormUI(FormUI::TEXT, F("Button up pin #"))));

    form.add<uint8_t>(F("pin1"), &dimmer_buttons->pins[1]); //->setFormUI((new FormUI(FormUI::TEXT, F("Button down pin #"))));

#endif

    form.finalize();

    PrintHtmlEntitiesString code;
    MQTTComponent::MQTTAutoDiscoveryVector vector;
    createAutoDiscovery(MQTTAutoDiscovery::FORMAT_YAML, vector);
    for(auto &&discovery: vector) {
        const auto &payload = discovery->getPayload();
        code.write((const uint8_t *)payload.c_str(), payload.length());
    }

    reinterpret_cast<SettingsForm &>(form).getTokens().push_back(std::make_pair<String, String>(F("HASS_YAML"), code.c_str()));
}

#endif
