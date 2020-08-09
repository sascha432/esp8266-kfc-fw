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

using KFCConfigurationClasses::Plugins;

void DimmerModuleForm::_createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
   if (!PluginComponent::isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Dimmer::getWriteableConfig();

    auto &ui = form.getFormUIConfig();
    ui.setTitle(F("Dimmer Configuration"));
    ui.setContainerId(F("dimmer_settings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config), String());

    form.add(F("fade_time"), _H_W_STRUCT_VALUE(cfg, fade_time));
    form.addFormUI(F("Fade In/Out Time"), FormUI::PlaceHolder(5.0, 1), FormUI::FPSuffix(FSPGM(seconds)));

    form.add(F("fade_onoff"), _H_W_STRUCT_VALUE(cfg, on_off_fade_time));
    form.addFormUI(F("Turn On/Off Fade Time"), FormUI::PlaceHolder(7.5, 1), FormUI::FPSuffix(FSPGM(seconds)));

#if DIMMER_FIRMWARE_VERSION < 0x030000
    form.add(F("lcf"), _H_W_STRUCT_VALUE(cfg, fw.linear_correction_factor));
    form.addFormUI(F("Linear Correction Factor"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::PlaceHolder(1.0, 1));
#endif

    form.add<bool>(F("restore"), _H_W_STRUCT_VALUE(cfg, fw.bits.restore_level));
    form.addFormUI(F("After Power Failure"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::BoolItems(F("Restore last brightness level"), F("Do not turn on")));

    mainGroup.end();

#if IOT_ATOMIC_SUN_V2

    auto &channelsGroup = form.addCardGroup(FSPGM(channels), F("Channels"), false);

    form.add<int8_t>(F("channel0"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[0], int8_t));
    form.addFormUI(F("Channel Warm White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW1));
    form.addValidator(FormRangeValidator(0, 3));

    form.add<int8_t>(F("channel1"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[1], int8_t));
    form.addFormUI(F("Channel Warm White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW2));
    form.addValidator(FormRangeValidator(0, 3));

    form.add<int8_t>(F("channel2"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[2], int8_t));
    form.addFormUI(F("Channel Cold White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW1));

    form.addValidator(FormRangeValidator(0, 3));
    form.add<int8_t>(F("channel3"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[3], int8_t));
    form.addFormUI(F("Channel Cold White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW2));
    form.addValidator(FormRangeValidator(0, 3));

    channelsGroup.end();

#endif

#if IOT_DIMMER_MODULE_HAS_BUTTONS

    auto &buttonGroup = form.addCardGroup(F("btncfg"), F("Buttons"), false);

    form.add<uint16_t>(F("sptime"), _H_W_STRUCT_VALUE(cfg, shortpress_time));
    form.addFormUI(F("Short Press Time"), FormUI::FPSuffix(FSPGM(milliseconds)), FormUI::PlaceHolder(250));
    form.addValidator(FormRangeValidator(50, 1000));

    form.add<uint16_t>(F("lptime"), _H_W_STRUCT_VALUE(cfg, longpress_time));
    form.addFormUI(F("Long Press Time"), FormUI::FPSuffix(FSPGM(milliseconds)), FormUI::PlaceHolder(600));
    form.addValidator(FormRangeValidator(250, 2000));

    form.add<uint16_t>(F("rtime"), _H_W_STRUCT_VALUE(cfg, repeat_time));
    form.addFormUI(F("Hold/Repeat Time"), FormUI::FPSuffix(FSPGM(milliseconds)), FormUI::PlaceHolder(150));
    form.addValidator(FormRangeValidator(50, 500));

    form.add<uint8_t>(F("sstep"), _H_W_STRUCT_VALUE(cfg, shortpress_step));
    form.addFormUI(F("Brightness Steps"), FormUI::Suffix('%'), FormUI::PlaceHolder(5));
    form.addValidator(FormRangeValidator(1, 100));

    form.add<uint16_t>(F("snrt"), _H_W_STRUCT_VALUE(cfg, shortpress_no_repeat_time));
    form.addFormUI(F("Short Press Down = Off/No Repeat Time"), FormUI::FPSuffix(FSPGM(milliseconds)), FormUI::PlaceHolder(800));
    form.addValidator(FormRangeValidator(250, 2500));

    form.add<uint8_t>(F("minbr"), _H_W_STRUCT_VALUE(cfg, min_brightness));
    form.addFormUI(F("Min. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(15));
    form.addValidator(FormRangeValidator(0, 100));

    form.add<uint8_t>(F("lpmaxb"), _H_W_STRUCT_VALUE(cfg, longpress_max_brightness));
    form.addFormUI(F("Long Press Up/Max. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(100));
    form.addValidator(FormRangeValidator(0, 100));

    form.add<uint8_t>(F("lpminb"), _H_W_STRUCT_VALUE(cfg, longpress_min_brightness));
    form.addFormUI(F("Long Press Down/Min. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(33));
    form.addValidator(FormRangeValidator(0, 100));

    form.add<float>(F("spft"), _H_W_STRUCT_VALUE(cfg, shortpress_fadetime));
    form.addFormUI(F("Short Press Fade Time"), FormUI::FPSuffix(FSPGM(seconds)), FormUI::PlaceHolder(1.0, 1));

    form.add<float>(F("lpfd"), _H_W_STRUCT_VALUE(cfg, longpress_fadetime));
    form.addFormUI(F("Long Press Fade Time"), FormUI::FPSuffix(FSPGM(seconds)), FormUI::PlaceHolder(5.0, 1));

    form.add<uint8_t>(F("pin0"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[0], uint8_t));
    form.addFormUI(F("Button Up Pin #"));

    form.add<uint8_t>(F("pin1"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[1], uint8_t));
    form.addFormUI(F("Button Down Pin #"));

    buttonGroup.end();

#endif

    auto &fwGroup = form.addCardGroup(F("fwcfg"), F("Advanced Firmware Configuration"), false);

    form.add<uint8_t>(F("max_temp"), _H_W_STRUCT_VALUE(cfg, fw.max_temp));
    form.addFormUI(F("Max. Temperature"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::PlaceHolder(80), FormUI::FPSuffix(FSPGM(_degreeC)));
    form.addValidator(FormRangeValidator(F("Temperature out of range: %min%-%max%"), 45, 110));

    form.add<uint8_t>(F("metricsint"), _H_W_STRUCT_VALUE(cfg, fw.report_metrics_max_interval));
    form.addFormUI(F("Metrics Report Interval"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::PlaceHolder(10), FormUI::FPSuffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidator(5, 255));

    form.add<uint8_t>(F("zc_offset"), _H_W_STRUCT_VALUE(cfg, fw.zero_crossing_delay_ticks));
    form.addFormUI(F("Zero Crossing Offset"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::FPSuffix(FSPGM(ticks, "ticks")));
    form.addValidator(FormRangeValidator(0, 255));

    form.add<uint16_t>(F("min_on"), _H_W_STRUCT_VALUE(cfg, fw.minimum_on_time_ticks));
    form.addFormUI(F("Minimum On-time"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::FPSuffix(FSPGM(ticks)));
    form.addValidator(FormRangeValidator(1, 65535));

    form.add<uint16_t>(F("min_off"), _H_W_STRUCT_VALUE(cfg, fw.adjust_halfwave_time_ticks));
    form.addFormUI(F("Minimum Off-time"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::FPSuffix(FSPGM(ticks)));
    form.addValidator(FormRangeValidator(1, 65535));

    form.add<float>(F("vref11"), _H_W_STRUCT_VALUE(cfg, fw.internal_1_1v_ref));
    form.addFormUI(F("ATmega 1.1V Reference Calibration"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::PlaceHolder(1.1, 1), FormUI::Suffix('V'));
    form.addValidator(FormRangeValidatorDouble(0.9, 1.3, 1));

    form.add<float>(F("temp_ofs"), (cfg.fw.ntc_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [&cfg](const float &value, FormField &, bool) {
        cfg.fw.ntc_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    });
    form.addFormUI(F("Temperature Offset (NTC)"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::PlaceHolder(0), FormUI::FPSuffix(FSPGM(_degreeC)));

    form.add<float>(F("temp2_ofs"), (cfg.fw.int_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [&cfg](const float &value, FormField &, bool) {
        cfg.fw.int_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    });
    form.addFormUI(F("Temperature Offset 2 (ATmega)"), FormUI::ConditionalAttribute(!cfg.config_valid, FSPGM(disabled), FSPGM(disabled)), FormUI::PlaceHolder(0), FormUI::FPSuffix(FSPGM(_degreeC)));

    fwGroup.end();

    form.finalize();
}
