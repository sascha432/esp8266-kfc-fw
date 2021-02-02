/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ALARM_PLUGIN_ENABLED

// --------------------------------------------------------------------
auto &alarmGroup = form.addCardGroup(FSPGM(alarm), FSPGM(Alarm), true);

form.add(F("alrm_col"), Color(cfg.alarm.color.value).toString(), [&cfg](const String &value, FormUI::Field::BaseField &field, bool store) {
    if (store) {
        cfg.alarm.color.value = Color::fromString(value);
    }
    return false;
});
form.addFormUI(FSPGM(Color));

form.addPointerTriviallyCopyable(F("alrm_sp"), &cfg.alarm.speed);
form.addFormUI(F("Flashing Speed"), FormUI::Suffix(FSPGM(milliseconds)));
form.addValidator(FormUI::Validator::Range(50, 0xffff));

alarmGroup.end();

#endif

// --------------------------------------------------------------------
auto &protectionGroup = form.addCardGroup(F("prot"), FSPGM(Protection), true);

form.addPointerTriviallyCopyable(F("trmin"), &cfg.protection.temperature_reduce_range.min);
form.addFormUI(F("Minimum Temperature To Reduce Brightness"), FormUI::Suffix(FSPGM(degree_Celsius_utf8)));
form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 90));

form.addPointerTriviallyCopyable(F("trmax"), &cfg.protection.temperature_reduce_range.max);
form.addFormUI(F("Maximum. Temperature To Reduce Brightness To 25%"), FormUI::Suffix(FSPGM(degree_Celsius_utf8)));
form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 90));

form.addPointerTriviallyCopyable(F("tpmax"), &cfg.protection.max_temperature);
form.addFormUI(F("Over Temperature Protection"), FormUI::Suffix(FSPGM(degree_Celsius_utf8)));
form.addValidator(FormUI::Validator::Range(kMinimumTemperatureThreshold, 105));

protectionGroup.end();
