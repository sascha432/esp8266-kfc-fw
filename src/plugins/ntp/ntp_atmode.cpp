/**
 * Author: sascha_lammers@gmx.de
 */

#if AT_MODE_SUPPORTED

#include "ntp_plugin_class.h"
#include "kfc_fw_config.h"

#if DEBUG_NTP_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

#include "at_mode.h"
#include "esp_settimeofday_cb.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(NOW, "NOW", "<update>", "Display current time or update NTP");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(TZ, "TZ", "<timezone>", "Set timezone", "Show timezone information");

void NTPPlugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(NOW), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(TZ), name);
}

bool NTPPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(NOW))) {
        if (args.size()) {
            execConfigTime();
            args.print(F("Waiting up to 5 seconds for a valid time..."));
            auto end = millis() + 5000;
            while(millis() < end && !isTimeValid()) {
                delay(10);
            }
        }
        auto now = time(nullptr);
        char timestamp[64];
        auto rtc = RTCMemoryManager::readTime();
        args.printf_P(PSTR("Now=" TIME_T_FMT " stored=%u status=%s"), now, rtc.getTime(), rtc.getStatus());
        if (isTimeValid(now)) {
            strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), gmtime(&now));
            args.printf_P(PSTR("gmtime=%s, unixtime=" TIME_T_FMT), timestamp, now);

            strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), localtime(&now));
            args.printf_P(PSTR("localtime=%s"), timestamp);
        }
        else {
            args.printf_P(PSTR("Time is currently not set (" TIME_T_FMT "). NTP is %s"), now, (System::Flags::getConfig().is_ntp_client_enabled ? SPGM(enabled) : SPGM(disabled)));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(TZ))) {
        if (args.isQueryMode()) {
            auto fmt = PSTR("%FT%T %Z %z");
            String fmtStr = fmt;
            auto &tz = *__gettzinfo();
            Serial.printf_P(PSTR("_tzname[0]=%s,_tzname[1]=%s\n"), _tzname[0], _tzname[1]);
            Serial.printf_P(PSTR("__tznorth=%u,__tzyear=%u\n"), tz.__tznorth, tz.__tzyear);
            for(int i = 0; i < 2; i++) {
                Serial.printf_P(PSTR("ch=%c,m=%d,n=%d,d=%d,s=%d,change=" TIME_T_FMT ",offset=%ld\n"), tz.__tzrule[i].ch, tz.__tzrule[i].m, tz.__tzrule[i].n, tz.__tzrule[i].d, tz.__tzrule[i].s, (long)tz.__tzrule[i].change, tz.__tzrule[i].offset);
            }
            auto str = getenv("TZ");
            Serial.printf_P(PSTR("TZ=%s"), str ? (PGM_P)str : PSTR("(none)"));
        } else if (args.requireArgs(2, 2)) {
            auto arg = args.get(1);
            if (args.isAnyMatchIgnoreCase(0, F("tz"))) {
                setTZ(arg);
                auto now = time(nullptr);
                localtime(&now); // update __gettzinfo()
                args.printf_P(PSTR("TZ set to '%s'"), arg);
            }
            else if (args.isAnyMatchIgnoreCase(0, F("ntp"))) {
                _ntpRefreshTimeMillis = 15000;
                configTime(arg, "pool.ntp.org");
                args.printf_P(PSTR("configTime called with '%s'"), arg);
            }
        }
        return true;
    }
    return false;
}

#endif
