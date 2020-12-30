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

void DimmerModuleForm::_createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form)
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

    auto &ui = form.createWebUI();
    ui.setTitle(F("Dimmer Configuration"));
    ui.setContainerId(F("dimmer_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto configValidAttr = FormUI::Conditional<FormUI::DisabledAttribute>(!cfg.version, FormUI::DisabledAttribute());

    auto &mainGroup = form.addCardGroup(FSPGM(config), F("General"), true);

    form.addPointerTriviallyCopyable(F("onft"), &cfg.on_fadetime);
    // form.add(F("onft"), _H_W_STRUCT_VALUE(cfg, on_fadetime));
    form.addFormUI(F("On Fade Time"), FormUI::PlaceHolder(7.5, 1), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::RangeDouble(1.0, 30.0));

    form.addPointerTriviallyCopyable(F("offft"), &cfg.off_fadetime);
    // form.add(F("offft"), _H_W_STRUCT_VALUE(cfg, off_fadetime));
    form.addFormUI(F("Off Fade Time"), FormUI::PlaceHolder(7.5, 1), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::RangeDouble(1.0, 30.0));

    auto &offDelaySignal = form.addObjectGetterSetter(F("offdlys"), cfg, cfg.get_bits_off_delay_signal, cfg.set_bits_off_delay_signal);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addObjectGetterSetter(F("offdly"), cfg, cfg.get_bits_off_delay, cfg.set_bits_off_delay);
    form.addFormUI(F("Off Delay"), FormUI::PlaceHolder(0), FormUI::Suffix(FSPGM(seconds)), FormUI::CheckboxButtonSuffix(offDelaySignal, F("Confirm Signal")));
    cfg.addRangeValidatorFor_off_delay(form);

#if DIMMER_FIRMWARE_VERSION <= 0x020105
    form.add(F("lcf"), _H_W_STRUCT_VALUE(cfg, fw.linear_correction_factor));
    form.addFormUI(F("Linear Correction Factor"), configValidAttr, FormUI::PlaceHolder(1.0, 1));
#endif

    form.add<bool>(F("restore"), _H_W_STRUCT_VALUE(cfg, cfg.bits.restore_level));
    form.addFormUI(F("After Power Failure"), configValidAttr, FormUI::BoolItems(F("Restore last brightness level"), F("Do not turn on")));

    form.addReference(F("maxtmp"), cfg.cfg.max_temp);
    // form.add<uint8_t>(F("max_temp"), _H_W_STRUCT_VALUE(cfg, cfg.max_temp));
    form.addFormUI(F("Max. Temperature"), configValidAttr, FormUI::PlaceHolder(75), FormUI::Suffix(FSPGM(_degreeC)));
    form.addValidator(FormUI::Validator::Range(F("Temperature out of range: %min%-%max%"), 55, 125));

    form.addReference(F("meint"), cfg.cfg.report_metrics_interval);
    // form.add<uint8_t>(F("metricsint"), _H_W_STRUCT_VALUE(cfg, cfg.report_metrics_interval));
    form.addFormUI(F("Metrics Report Interval"), configValidAttr, FormUI::PlaceHolder(10), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormUI::Validator::Range(2, 60));

    mainGroup.end();

#if IOT_ATOMIC_SUN_V2

    auto &channelsGroup = form.addCardGroup(FSPGM(channels), F("Channels"), false);

    form.add<int8_t>(F("channel0"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[0], int8_t));
    form.addFormUI(F("Channel Warm White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW1));
    form.addValidator(FormUI::Validator::Range(0, 3));

    form.add<int8_t>(F("channel1"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[1], int8_t));
    form.addFormUI(F("Channel Warm White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_WW2));
    form.addValidator(FormUI::Validator::Range(0, 3));

    form.add<int8_t>(F("channel2"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[2], int8_t));
    form.addFormUI(F("Channel Cold White 1"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW1));

    form.addValidator(FormUI::Validator::Range(0, 3));
    form.add<int8_t>(F("channel3"), _H_W_STRUCT_VALUE_TYPE(cfg, channel_mapping[3], int8_t));
    form.addFormUI(F("Channel Cold White 2"), FormUI::PlaceHolder(IOT_ATOMIC_SUN_CHANNEL_CW2));
    form.addValidator(FormUI::Validator::Range(0, 3));

    channelsGroup.end();

#endif

#if IOT_DIMMER_MODULE_HAS_BUTTONS

    auto &buttonGroup = form.addCardGroup(F("btncfg"), F("Button Configuration"), false);

    form.addObjectGetterSetter(F("sptime"), cfg, cfg.get_bits_shortpress_time, cfg.set_bits_shortpress_time);
    form.addFormUI(F("Click Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(cfg.kDefaultValueFor_shortpress_time));
    cfg.addRangeValidatorFor_longpress_time(form);

    form.addObjectGetterSetter(F("lptime"), cfg, cfg.get_bits_longpress_time, cfg.set_bits_longpress_time);
    form.addFormUI(F("Long Press Time"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(cfg.kDefaultValueFor_longpress_time));
    cfg.addRangeValidatorFor_longpress_time(form);

    form.add<float>(F("lpft"), _H_W_STRUCT_VALUE(cfg, lp_fadetime));
    form.addFormUI(F("Button Held Fade Time"), FormUI::Suffix(FSPGM(seconds)), FormUI::PlaceHolder(3.5, 1));
    form.addValidator(FormUI::Validator::RangeDouble(1.0, 30.0));

    form.addObjectGetterSetter(F("sstep"), cfg, cfg.get_bits_shortpress_steps, cfg.set_bits_shortpress_steps);
    form.addFormUI(F("Brightness Steps"), FormUI::Suffix(F("per 100%")), FormUI::PlaceHolder(cfg.kDefaultValueFor_shortpress_steps));
    cfg.addRangeValidatorFor_shortpress_steps(form);

    form.addObjectGetterSetter(F("snrt"), cfg, cfg.get_bits_single_click_time, cfg.set_bits_single_click_time);
    form.addFormUI(F("Double Click Speed:"), FormUI::Suffix(FSPGM(milliseconds)), FormUI::PlaceHolder(cfg.kDefaultValueFor_shortpress_time));
    cfg.addRangeValidatorFor_single_click_time(form);

    form.addObjectGetterSetter(F("maxbr"), cfg, cfg.get_bits_max_brightness, cfg.set_bits_max_brightness);
    form.addFormUI(F("Max. Brightness"), FormUI::Suffix(F("%")), FormUI::PlaceHolder(cfg.kDefaultValueFor_max_brightness));
    cfg.addRangeValidatorFor_max_brightness(form);

    form.addObjectGetterSetter(F("minbr"), cfg, cfg.get_bits_min_brightness, cfg.set_bits_min_brightness);
    form.addFormUI(F("Min. Brightness"), FormUI::Suffix(F("%")), FormUI::PlaceHolder(cfg.kDefaultValueFor_min_brightness));
    cfg.addRangeValidatorFor_min_brightness(form);

    form.addObjectGetterSetter(F("lpmaxb"), cfg, cfg.get_bits_longpress_max_brightness, cfg.set_bits_longpress_max_brightness);
    form.addFormUI(F("Long Press Up/Max. Brightness"), FormUI::Suffix(F("%")), FormUI::PlaceHolder(cfg.kDefaultValueFor_longpress_max_brightness));
    cfg.addRangeValidatorFor_longpress_max_brightness(form);

    form.addObjectGetterSetter(F("lpminb"), cfg, cfg.get_bits_longpress_min_brightness, cfg.set_bits_longpress_min_brightness);
    form.addFormUI(F("Long Press Down/Min. Brightness"), FormUI::Suffix(F("%")), FormUI::PlaceHolder(cfg.kDefaultValueFor_longpress_min_brightness));
    cfg.addRangeValidatorFor_longpress_min_brightness(form);

    buttonGroup.end();

#endif

    FormUI::Container::List pins(KFCConfigurationClasses::createFormPinList());

    auto &buttonPinGroup = form.addCardGroup(F("btncfg"), F("Button Pin Configuration"), false);

    auto &pin0Inverted = form.addObjectGetterSetter(F("pupi"), cfg, cfg.get_bits_pin_ch0_up_inverted, cfg.set_bits_pin_ch0_up_inverted);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addObjectGetterSetter(F("pinup"), cfg, cfg.get_bits_pin_ch0_up, cfg.set_bits_pin_ch0_up);
    form.addFormUI(F("Button Up Pin #"), FormUI::CheckboxButtonSuffix(pin0Inverted, F("Active Low")), pins);

    auto &pin1Inverted = form.addObjectGetterSetter(F("pdbi"), cfg, cfg.get_bits_pin_ch0_down_inverted, cfg.set_bits_pin_ch0_down_inverted);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addObjectGetterSetter(F("pindn"),  cfg, cfg.get_bits_pin_ch0_down, cfg.set_bits_pin_ch0_down);
    form.addFormUI(F("Button Down Pin #"), FormUI::CheckboxButtonSuffix(pin1Inverted, F("Active Low")), pins);

    buttonPinGroup.end();

    auto &fwGroup = form.addCardGroup(F("fwcfg"), F("Advanced Firmware Configuration"), false);

#if DIMMER_FIRMWARE_VERSION >= 0x020200

    form.addCallbackSetter(F("dmode"), cfg.cfg.bits.leading_edge, [&cfg](const bool value, FormField &) {
        cfg.cfg.bits.leading_edge = value;
    });
    form.addFormUI(F("Operation Mode"), configValidAttr, FormUI::Container::List(0, F("Trailing Edge"), 1, F("Leading Edge")));

    form.addPointerTriviallyCopyable(F("rbeg"), &cfg.cfg.range_begin);
    // form.add<uint16_t>(F("lrs"), _H_W_STRUCT_VALUE(cfg, cfg.range_begin));
    form.addFormUI(F("Level Range Start"), configValidAttr, FormUI::Suffix(F("0 - Range End")));
    form.addValidator(FormUI::Validator::Range(0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS));

    form.addCallbackSetter(F("rend"), cfg.cfg.get_range_end(), [&cfg](uint16_t value, FormField &) {
        cfg.cfg.set_range_end(value);
    });
    // form.add<uint16_t>(F("rev"), _H_W_STRUCT_VALUE(cfg, cfg.range_end));
    form.addFormUI(F("Level Range End"), configValidAttr, FormUI::Suffix(F("Range Start - " _STRINGIFY(IOT_DIMMER_MODULE_MAX_BRIGHTNESS))));
    form.addValidator(FormUI::Validator::Range(0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS));

#endif

    auto &negativeZCOffset = form.addCallbackSetter(F("nzcd"), cfg.cfg.bits.negative_zc_delay, [&cfg](const uint8_t value, FormField &) {
        cfg.cfg.bits.negative_zc_delay = value;
    });
    form.addFormUI(FormUI::Type::HIDDEN, configValidAttr);

    form.addPointerTriviallyCopyable(F("zc_offset"), &cfg.cfg.zero_crossing_delay_ticks);
    // form.add<uint8_t>(F("zc_offset"), _H_W_STRUCT_VALUE(cfg, cfg.zero_crossing_delay_ticks));
    form.addFormUI(F("Zero Crossing Offset"), configValidAttr, FormUI::Suffix(FSPGM(ticks, "ticks")), FormUI::CheckboxButtonSuffix(negativeZCOffset, F("Negative Offset")));
    form.addValidator(FormUI::Validator::Range(0, 65535));

#if DIMMER_FIRMWARE_VERSION >= 0x020200

    form.addReference(F("adjhwc"), cfg.cfg.halfwave_adjust_cycles);
    //form.add<int8_t>(c, _H_W_STRUCT_VALUE(cfg, cfg.halfwave_adjust_cycles));
    form.addFormUI(F("Adjust Halfwave Length"), configValidAttr, FormUI::Suffix(F("Clock cycles")));
    form.addValidator(FormUI::Validator::Range(-128, 127));

#endif

    form.addPointerTriviallyCopyable(F("min_on"), &cfg.cfg.minimum_on_time_ticks);
    // form.add<uint16_t>(F("min_on"), _H_W_STRUCT_VALUE(cfg, cfg.minimum_on_time_ticks));
    form.addFormUI(F("Minimum On-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
    form.addValidator(FormUI::Validator::Range(1, 65535));

    form.addPointerTriviallyCopyable(F("min_off"), &cfg.cfg.minimum_off_time_ticks);
    // form.add<uint16_t>(F("min_off"), _H_W_STRUCT_VALUE(cfg, cfg.minimum_off_time_ticks));
    form.addFormUI(F("Minimum Off-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
    form.addValidator(FormUI::Validator::Range(1, 65535));

#if DIMMER_FIRMWARE_VERSION >= 0x020200

    form.addPointerTriviallyCopyable(F("swon_min_on"), &cfg.cfg.switch_on_minimum_ticks);
    // form.add<uint16_t>(F("swon_min_on"), _H_W_STRUCT_VALUE(cfg, cfg.switch_on_minimum_ticks));
    form.addFormUI(F("Switch-On Minimum On-time"), configValidAttr, FormUI::Suffix(FSPGM(ticks)));
    form.addValidator(FormUI::Validator::Range(0, 65535));

    form.addPointerTriviallyCopyable(F("swon_count"), &cfg.cfg.switch_on_count);
    // form.add<uint8_t>(F("swon_count"), _H_W_STRUCT_VALUE(cfg, cfg.switch_on_count));
    form.addFormUI(F("Switch-On Minimum On-time"), configValidAttr, FormUI::Suffix(F("Half Cycles")));
    form.addValidator(FormUI::Validator::Range(0, 250));

#endif

    form.addCallbackSetter<float>(F("vref11"), cfg.cfg.internal_vref11, [&cfg](float value, FormField &) {
        cfg.cfg.internal_vref11 = value;
    });
    // form.add<float>(F("vref11"), _H_W_STRUCT_VALUE(cfg, cfg.internal_vref11));
    form.addFormUI(F("ATmega 1.1V Reference Calibration"), configValidAttr, FormUI::PlaceHolder(1.1, 1), FormUI::Suffix(F("V")));
    form.addValidator(FormUI::Validator::RangeDouble(internal_vref11_t((int8_t)-128), internal_vref11_t((int8_t)127), 1));


#if DIMMER_FIRMWARE_VERSION < 0x020200

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

#else

    form.add<float>(F("temp_ofs"), cfg.cfg.ntc_temp_cal_offset, [&cfg](const float value, FormField &, bool) {
        cfg.cfg.ntc_temp_cal_offset = value;
        return false;
    });
    form.addFormUI(F("Temperature Offset (NTC)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(FSPGM(_degreeC)));

    form.addPointerTriviallyCopyable(F("tsofs"), &cfg.cfg.internal_temp_calibration.ts_offset);
    form.addFormUI(F("TS Offset (ATmega Sensor)"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(F("temperature sensor offset correction")));
    form.addValidator(FormUI::Validator::Range(0, 255));

    form.addPointerTriviallyCopyable(F("tsgain"), &cfg.cfg.internal_temp_calibration.ts_gain);
    form.addFormUI(F("TS Gain"), configValidAttr, FormUI::PlaceHolder(0), FormUI::Suffix(F("8-bit unsigned fixed point 1/128th units")));
    form.addValidator(FormUI::Validator::Range(0, 255));
    // , [&cfg](const uint8_t value, FormField &) {
    //     cfg.cfg.int_temp_offset = value;
    //     return false;
    // });

#endif

    fwGroup.end();

    form.finalize();
}
