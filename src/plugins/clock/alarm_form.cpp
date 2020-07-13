/**
 * Author: sascha_lammers@gmx.de
 */


#include <PrintHtmlEntitiesString.h>
#include <ESPAsyncWebServer.h>
#include "../include/templates.h"
#include "plugins.h"
#include "alarm_form.h"
#include "kfc_fw_config_classes.h"

#if DEBUG_ALARM_FORM
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void AlarmForm::setup(PluginSetupMode_t mode)
{
    _debug_println();
}

void AlarmForm::reconfigure(PGM_P source)
{
    _debug_println();
}

void AlarmForm::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    using Alarm = KFCConfigurationClasses::Plugins::Alarm;
    auto cfg = &Alarm::getWriteableConfig();

    for(uint8_t i = 0; i < Alarm::MAX_ALARMS; i++) {
        String prefix = PrintString(F("alarm_%u_"), i);
        auto alarm = &cfg->alarms[i];

        _debug_printf_P(PSTR("%s: %u\n"), String(prefix + FSPGM(enabled)).c_str(), Alarm::isEnabled(*alarm));

        form.add<bool>(prefix + FSPGM(enabled), Alarm::isEnabled(*alarm), [alarm](bool value, FormField &, bool) {
            Alarm::setEnabled(*alarm, value);
            return false;
        });

        form.add<uint8_t>(prefix + F("hour"), &alarm->time.hour);
        form.add<uint8_t>(prefix + F("minute"), &alarm->time.minute);

        form.add<uint8_t>(prefix + F("type"), &alarm->mode);

// TODO runs out of memory
// D00025711 (alarm_form.cpp:38<16992> createConfigureForm): alarm_0_enabled: 0
// D00025719 (alarm_form.cpp:38<15544> createConfigureForm): alarm_1_enabled: 0
// D00025726 (alarm_form.cpp:38<14104> createConfigureForm): alarm_2_enabled: 0
// D00025732 (alarm_form.cpp:38<12600> createConfigureForm): alarm_3_enabled: 0
// D00025739 (alarm_form.cpp:38<11224> createConfigureForm): alarm_4_enabled: 0
// D00025746 (alarm_form.cpp:38<9848> createConfigureForm): alarm_5_enabled: 0
// D00025752 (alarm_form.cpp:38<8216> createConfigureForm): alarm_6_enabled: 0
// D00025759 (alarm_form.cpp:38<6840> createConfigureForm): alarm_7_enabled: 0
// D00025766 (alarm_form.cpp:38<5464> createConfigureForm): alarm_8_enabled: 0
// D00025772 (alarm_form.cpp:38<4088> createConfigureForm): alarm_9_enabled: 0

        for(uint8_t j = 0; j < 7; j++) {
            form.add<bool>(prefix + PrintString(F("weekday_%u"), j), alarm->time.week_day.week_days & _BV(j), [alarm, j](bool value, FormField &, bool) {
                if (value) {
                    alarm->time.week_day.week_days |= _BV(j);
                }
                else {
                    alarm->time.week_day.week_days &= ~_BV(j);
                }
                return false;
            }, FormField::InputFieldType::CHECK);
        }

    }

    form.finalize();
}
