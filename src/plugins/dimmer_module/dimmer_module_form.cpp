/**
 * Author: sascha_lammers@gmx.de
 */


#include "dimmer_module_form.h"
#include <PrintHtmlEntitiesString.h>
#include "../include/templates.h"
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

void DimmerModuleForm::_createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
   if (!PluginComponent::isCreateFormCallbackType(type)) {
        return;
    }

    auto isInvalid = !config._H_GET(Config().dimmer).config_valid;

    form.setFormUI(F("Dimmer Configuration"));

    form.add<float>(F("fade_time"), _H_STRUCT_VALUE(Config().dimmer, fade_time))->setFormUI((new FormUI(FormUI::TEXT, F("Fade In/Out Time")))->setPlaceholder(String(5.0, 1))->setSuffix(FSPGM(seconds)));

    form.add<float>(F("fade_onoff"), _H_STRUCT_VALUE(Config().dimmer, on_off_fade_time))->setFormUI((new FormUI(FormUI::TEXT, F("Turn On/Off Fade Time")))->setPlaceholder(String(7.5, 1))->setSuffix(FSPGM(seconds)));

#if DIMMER_FIRMWARE_VERSION < 0x030000
    form.add<float>(F("lcf"), _H_STRUCT_VALUE(Config().dimmer, cfg.linear_correction_factor))->setFormUI((new FormUI(FormUI::TEXT, F("Linear Correction Factor")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setPlaceholder(String(1.0, 1)));
#endif

    form.add<bool>(F("restore"), _H_FLAGS_BOOL_VALUE(Config().dimmer, cfg.bits.restore_level))->setFormUI((new FormUI(FormUI::SELECT, F("After Power Failure")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setBoolItems(F("Restore last brightness level"), F("Do not turn on")));

#if IOT_ATOMIC_SUN_V2
    auto channelErrorMsg = String(F("Channel out of range: %min%-%max%"));
    form.add<int8_t>(F("channel0"), _H_STRUCT_VALUE_TYPE(Config().dimmer, channel_mapping[0], int8_t))->setFormUI((new FormUI(FormUI::TEXT, F("Channel Warm White 1")))->setPlaceholder(String(IOT_ATOMIC_SUN_CHANNEL_WW1)));
    form.addValidator(new FormRangeValidator(channelErrorMsg, 0, 3));
    form.add<int8_t>(F("channel1"), _H_STRUCT_VALUE_TYPE(Config().dimmer, channel_mapping[1], int8_t))->setFormUI((new FormUI(FormUI::TEXT, F("Channel Warm White 2")))->setPlaceholder(String(IOT_ATOMIC_SUN_CHANNEL_WW2)));
    form.addValidator(new FormRangeValidator(channelErrorMsg, 0, 3));
    form.add<int8_t>(F("channel2"), _H_STRUCT_VALUE_TYPE(Config().dimmer, channel_mapping[2], int8_t))->setFormUI((new FormUI(FormUI::TEXT, F("Channel Cold White 1")))->setPlaceholder(String(IOT_ATOMIC_SUN_CHANNEL_CW1)));
    form.addValidator(new FormRangeValidator(channelErrorMsg, 0, 3));
    form.add<int8_t>(F("channel3"), _H_STRUCT_VALUE_TYPE(Config().dimmer, channel_mapping[3], int8_t))->setFormUI((new FormUI(FormUI::TEXT, F("Channel Cold White 2")))->setPlaceholder(String(IOT_ATOMIC_SUN_CHANNEL_CW2)));
    form.addValidator(new FormRangeValidator(channelErrorMsg, 0, 3));
#endif

    auto &group = form.addGroup(F("advanced"), F("Advanced Firmware Configuration"), false);

    form.add<uint8_t>(F("max_temp"), _H_STRUCT_VALUE(Config().dimmer, cfg.max_temp))->setFormUI((new FormUI(FormUI::TEXT, F("Max. Temperature")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setPlaceholder(String(80))->setSuffix(F("&deg;C")));
    form.addValidator(new FormRangeValidator(F("Temperature out of range: %min%-%max%"), 45, 110));

    form.add<uint8_t>(F("metrics_int"), _H_STRUCT_VALUE(Config().dimmer, cfg.report_metrics_max_interval))->setFormUI((new FormUI(FormUI::TEXT, F("Metrics Report Interval")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setPlaceholder(String(10))->setSuffix(FSPGM(seconds)));
    form.addValidator(new FormRangeValidator(5, 255));

    form.add<uint8_t>(F("zc_offset"), _H_STRUCT_VALUE(Config().dimmer, cfg.zero_crossing_delay_ticks))->setFormUI((new FormUI(FormUI::TEXT, F("Zero Crossing Offset")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setSuffix(F("ticks")));
    form.addValidator(new FormRangeValidator(0, 255));

    form.add<uint16_t>(F("min_on"), _H_STRUCT_VALUE(Config().dimmer, cfg.minimum_on_time_ticks))->setFormUI((new FormUI(FormUI::TEXT, F("Minimum On-time")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setSuffix(F("ticks")));
    form.addValidator(new FormRangeValidator(1, 65535));

    form.add<uint16_t>(F("min_off"), _H_STRUCT_VALUE(Config().dimmer, cfg.adjust_halfwave_time_ticks))->setFormUI((new FormUI(FormUI::TEXT, F("Minimum Off-time")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setSuffix(F("ticks")));
    form.addValidator(new FormRangeValidator(1, 65535));

    form.add<float>(F("vref11"), _H_STRUCT_VALUE(Config().dimmer, cfg.internal_1_1v_ref))->setFormUI((new FormUI(FormUI::TEXT, F("ATmega 1.1V Reference Calibration")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setPlaceholder(String(1.1, 1))->setSuffix(F("V")));
    form.addValidator(new FormRangeValidatorDouble(0.9, 1.3, 1));

    form.add<float>(F("temp_ofs"), (config._H_GET(Config().dimmer).cfg.ntc_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [](const float &value, FormField &, bool) {
        auto &cfg = config._H_W_GET(Config().dimmer).cfg;
        cfg.ntc_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    })->setFormUI((new FormUI(FormUI::TEXT, F("Temperature Offset (NTC)")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setPlaceholder(String(0))->setSuffix(F("&deg;C")));

    form.add<float>(F("temp2_ofs"), (config._H_GET(Config().dimmer).cfg.int_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [](const float &value, FormField &, bool) {
        auto &cfg = config._H_W_GET(Config().dimmer).cfg;
        cfg.int_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    })->setFormUI((new FormUI(FormUI::TEXT, F("Temperature Offset 2 (ATmega)")))->addConditionalAttribute(isInvalid, FSPGM(disabled), FSPGM(disabled))->setPlaceholder(String(0))->setSuffix(F("&deg;C")));

    group.end();


#if IOT_DIMMER_MODULE_HAS_BUTTONS

    form.add<uint16_t>(F("sptime"), _H_STRUCT_VALUE(Config().dimmer_buttons, shortpress_time))->setFormUI((new FormUI(FormUI::TEXT, F("Short Press Time")))->setPlaceholder(String(250))->setSuffix(FSPGM(milliseconds)));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_time, "Invalid time"), 50, 1000));

    form.add<uint16_t>(F("lptime"), _H_STRUCT_VALUE(Config().dimmer_buttons, longpress_time))->setFormUI((new FormUI(FormUI::TEXT, F("Long Press Time")))->setPlaceholder(String(600))->setSuffix(FSPGM(milliseconds)));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_time), 250, 2000));

    form.add<uint16_t>(F("rtime"), _H_STRUCT_VALUE(Config().dimmer_buttons, repeat_time))->setFormUI((new FormUI(FormUI::TEXT, F("Hold/Repeat Time")))->setPlaceholder(String(150))->setSuffix(FSPGM(milliseconds)));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_time), 50, 500));

    form.add<uint8_t>(F("sstep"), _H_STRUCT_VALUE(Config().dimmer_buttons, shortpress_step))->setFormUI((new FormUI(FormUI::TEXT, F("Brightness Steps")))->setPlaceholder(String(5))->setSuffix(F("&#37;")));
    form.addValidator(new FormRangeValidator(F("Invalid level"), 1, 100));

    form.add<uint16_t>(F("snrt"), _H_STRUCT_VALUE(Config().dimmer_buttons, shortpress_no_repeat_time))->setFormUI((new FormUI(FormUI::TEXT, F("Short Press Down = Off/No Repeat Time")))->setPlaceholder(String(800))->setSuffix(FSPGM(milliseconds)));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_time), 250, 2500));

    form.add<uint8_t>(F("minbr"), _H_STRUCT_VALUE(Config().dimmer_buttons, min_brightness))->setFormUI((new FormUI(FormUI::TEXT, F("Min. Brightness")))->setPlaceholder(String(15))->setSuffix(F("&#37;")));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_brightness, "Invalid brightness"), 0, 100));

    form.add<uint8_t>(F("lpmaxb"), _H_STRUCT_VALUE(Config().dimmer_buttons, longpress_max_brightness))->setFormUI((new FormUI(FormUI::TEXT, F("Long Press Up/Max. Brightness")))->setPlaceholder(String(100))->setSuffix(F("&#37;")));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_brightness), 0, 100));

    form.add<uint8_t>(F("lpminb"), _H_STRUCT_VALUE(Config().dimmer_buttons, longpress_min_brightness))->setFormUI((new FormUI(FormUI::TEXT, F("Long Press Down/Min. Brightness")))->setPlaceholder(String(33))->setSuffix(F("&#37;")));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_brightness), 0, 100));

    form.add<float>(F("spft"), _H_STRUCT_VALUE(Config().dimmer_buttons, shortpress_fadetime))->setFormUI((new FormUI(FormUI::TEXT, F("Short Press Fade Time")))->setPlaceholder(String(1.0, 1))->setSuffix(FSPGM(seconds)));

    form.add<float>(F("lpfd"), _H_STRUCT_VALUE(Config().dimmer_buttons, longpress_fadetime))->setFormUI((new FormUI(FormUI::TEXT, F("Long Press Fade Time")))->setPlaceholder(String(5.0, 1))->setSuffix(FSPGM(seconds)));

    form.add<uint8_t>(F("pin0"), _H_STRUCT_VALUE_TYPE(Config().dimmer_buttons, pins[0], uint8_t))->setFormUI((new FormUI(FormUI::TEXT, F("Button Up Pin #"))));

    form.add<uint8_t>(F("pin1"), _H_STRUCT_VALUE_TYPE(Config().dimmer_buttons, pins[1], uint8_t))->setFormUI((new FormUI(FormUI::TEXT, F("Button Down Pin #"))));

#endif

    form.finalize();
}
