/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2
#include "../src/plugins/atomic_sun/atomic_sun_v2.h"
#else
#include "dimmer_plugin.h"
#endif

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void DimmerModuleForm::_createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (type == PluginComponent::FormCallbackType::SAVE) {
        writeConfig(Plugins::Dimmer::getWriteableConfig());
        return;
    }
    else if (!PluginComponent::isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Dimmer::getWriteableConfig();
    if (type == PluginComponent::FormCallbackType::CREATE_GET) {
        readConfig(cfg);
    }

    auto &ui = form.getFormUIConfig();
    ui.setTitle(F("Dimmer Configuration"));
    ui.setContainerId(F("dimmer_settings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    auto configValidAttr = FormUI::ConditionalAttribute(cfg.config_valid == false, FSPGM(disabled), FSPGM(disabled));

    auto &mainGroup = form.addCardGroup(FSPGM(config), F("Common"), true);

    form.add(F("fade_time"), _H_W_STRUCT_VALUE(cfg, fade_time));
    form.addFormUI(F("Fade In/Out Time"), FormUI::PlaceHolder(5.0, 1), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidatorDouble(1.0, 30.0));

    form.add(F("fade_onoff"), _H_W_STRUCT_VALUE(cfg, on_off_fade_time));
    form.addFormUI(F("Turn On/Off Fade Time"), FormUI::PlaceHolder(7.5, 1), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidatorDouble(1.0, 30.0));

#if DIMMER_FIRMWARE_VERSION < 0x030000
    form.add(F("lcf"), _H_W_STRUCT_VALUE(cfg, fw.linear_correction_factor));
    form.addFormUI(F("Linear Correction Factor"), configValidAttr, FormUI::PlaceHolder(1.0, 1));
#endif

    form.add<bool>(F("restore"), _H_W_STRUCT_VALUE(cfg, fw.bits.restore_level));
    form.addFormUI(F("After Power Failure"), configValidAttr, FormUI::BoolItems(F("Restore last brightness level"), F("Do not turn on")));

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
    form.addFormUI(F("Click Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(275));
    form.addValidator(FormRangeValidator(50, 1500));

    form.add<uint16_t>(F("lptime"), _H_W_STRUCT_VALUE(cfg, longpress_time));
    form.addFormUI(F("Long Press Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(1000));
    form.addValidator(FormRangeValidator(250, 5000));

    // form.add<uint16_t>(F("rtime"), _H_W_STRUCT_VALUE(cfg, repeat_time));
    // form.addFormUI(F("Repeat Click Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(150));
    // form.addValidator(FormRangeValidator(50, 500 * kFactor));

    form.add<float>(F("lpfd"), _H_W_STRUCT_VALUE(cfg, longpress_fadetime));
    form.addFormUI(F("Fade Time"), FormUI::Suffix(FSPGM(seconds)), FormUI::PlaceHolder(3.5, 1));
    form.addValidator(FormRangeValidatorDouble(1.0, 30.0));

    form.add<uint8_t>(F("sstep"), _H_W_STRUCT_VALUE(cfg, shortpress_steps));
    form.addFormUI(F("Brightness Steps"), FormUI::Suffix(F("per 100%")), FormUI::PlaceHolder(15));
    form.addValidator(FormRangeValidator(4, 50));

    form.add<uint16_t>(F("snrt"), _H_W_STRUCT_VALUE(cfg, single_click_time));
    form.addFormUI(F("Double Click Speed:"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(1500));
    form.addValidator(FormRangeValidator(500, 15000));

    form.add<uint8_t>(F("minbr"), _H_W_STRUCT_VALUE(cfg, min_brightness));
    form.addFormUI(F("Min. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(15));
    form.addValidator(FormRangeValidator(0, 50));

    form.add<uint8_t>(F("lpmaxb"), _H_W_STRUCT_VALUE(cfg, longpress_max_brightness));
    form.addFormUI(F("Long Press Up/Max. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(85));
    form.addValidator(FormRangeValidator(20, 100));

    form.add<uint8_t>(F("lpminb"), _H_W_STRUCT_VALUE(cfg, longpress_min_brightness));
    form.addFormUI(F("Long Press Down/Min. Brightness"), FormUI::Suffix('%'), FormUI::PlaceHolder(33));
    form.addValidator(FormRangeValidator(0, 80));

    buttonGroup.end();

#endif

    auto &fwGroup = form.addCardGroup(F("fwcfg"), F("Advanced Firmware Configuration"), false);

    form.add<uint8_t>(F("pin0"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[0], uint8_t));
    form.addFormUI(F("Button Up Pin #"));

    form.add<uint8_t>(F("pin1"), _H_W_STRUCT_VALUE_TYPE(cfg, pins[1], uint8_t));
    form.addFormUI(F("Button Down Pin #"));

    form.add<uint8_t>(F("max_temp"), _H_W_STRUCT_VALUE(cfg, fw.max_temp));
    form.addFormUI(F("Max. Temperature"), configValidAttr, FormUI::PlaceHolder(80), FormUI::Suffix(FSPGM(_degreeC)));
    form.addValidator(FormRangeValidator(F("Temperature out of range: %min%-%max%"), 45, 110));

    form.add<uint8_t>(F("metricsint"), _H_W_STRUCT_VALUE(cfg, fw.report_metrics_max_interval));
    form.addFormUI(F("Metrics Report Interval"), configValidAttr, FormUI::PlaceHolder(10), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidator(5, 255));

    form.add<uint8_t>(F("zc_offset"), _H_W_STRUCT_VALUE(cfg, fw.zero_crossing_delay_ticks));
    form.addFormUI(F("Zero Crossing Offset"), configValidAttr, FormUI::Suffix(FSPGM(ticks, "ticks")));
    form.addValidator(FormRangeValidator(0, 255));

    form.add<uint16_t>(F("min_on"), _H_W_STRUCT_VALUE(cfg, fw.minimum_on_time_ticks));
    form.addFormUI(F("Minimum On-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
    form.addValidator(FormRangeValidator(1, 65535));

    form.add<uint16_t>(F("min_off"), _H_W_STRUCT_VALUE(cfg, fw.adjust_halfwave_time_ticks));
    form.addFormUI(F("Minimum Off-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
    form.addValidator(FormRangeValidator(1, 65535));

    form.add<float>(F("vref11"), _H_W_STRUCT_VALUE(cfg, fw.internal_1_1v_ref));
    form.addFormUI(F("ATmega 1.1V Reference Calibration"), configValidAttr, FormUI::PlaceHolder(1.1, 1), FormUI::Suffix('V'));
    form.addValidator(FormRangeValidatorDouble(0.9, 1.3, 1));

    form.add<float>(F("temp_ofs"), (cfg.fw.ntc_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [&cfg](const float &value, FormField &, bool) {
        cfg.fw.ntc_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    });
    form.addFormUI(F("Temperature Offset (NTC)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(FSPGM(_degreeC)));

    form.add<float>(F("temp2_ofs"), (cfg.fw.int_temp_offset / DIMMER_TEMP_OFFSET_DIVIDER), [&cfg](const float &value, FormField &, bool) {
        cfg.fw.int_temp_offset = value * DIMMER_TEMP_OFFSET_DIVIDER;
        return false;
    });
    form.addFormUI(F("Temperature Offset 2 (ATmega)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(FSPGM(_degreeC)));

    fwGroup.end();

    form.finalize();
}
