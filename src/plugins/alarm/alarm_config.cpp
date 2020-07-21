/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"

namespace KFCConfigurationClasses {

    Plugins::Alarm::Alarm() : cfg({})
    {
        cfg.alarms[0].time.week_day.week_days_enum = WeekDaysType::WEEK_DAYS;
        cfg.alarms[0].time.hour = 6;
        cfg.alarms[0].time.minute = 30;
        cfg.alarms[1].time.week_day.week_days_enum = WeekDaysType::WEEK_END;
        cfg.alarms[1].time.hour = 10;
        cfg.alarms[1].time.minute = 0;
        for(uint8_t i = 0; i < MAX_ALARMS; i++) {
            cfg.alarms[i].max_duration = DEFAULT_MAX_DURATION;
        }
    }

    Plugins::Alarm::TimeType Plugins::Alarm::getTime(const struct tm *_tm, SingleAlarm_t &alarm)
    {
        if (!alarm.is_enabled) {
            return 0;
        }
        if (alarm.time.timestamp) {
            return alarm.time.timestamp;
        }
        if (alarm.time.week_day.week_days == 0) {
            return 0;
        }
        struct tm tmp = *_tm;
        struct tm *tm = &tmp;
        // debug_printf_P(PSTR("wday=%u time=%02u:%02u week_days=0x%02x&0x%02x=%u time=%02u:%02u today=%u\n"),
        //     _tm->tm_wday, _tm->tm_hour, _tm->tm_min, alarm.time.week_day.week_days, _BV(_tm->tm_wday), ((alarm.time.week_day.week_days & _BV(_tm->tm_wday))),
        //     alarm.time.hour, alarm.time.minute, ((alarm.time.hour > _tm->tm_hour) || (alarm.time.hour == _tm->tm_hour && alarm.time.minute > _tm->tm_min))
        // );
        if (
            ((alarm.time.week_day.week_days & _BV(_tm->tm_wday)) == 0) ||                                                       // not today
            !((alarm.time.hour > _tm->tm_hour) || (alarm.time.hour == _tm->tm_hour && alarm.time.minute > _tm->tm_min))         // too late for today
        ) {
            // find next active day
            while((tm->tm_wday = (tm->tm_wday + 1) % 7) != _tm->tm_wday) {
                tm->tm_mday++;
                if ((alarm.time.week_day.week_days & _BV(tm->tm_wday)) != 0) {
                    break;
                }
            }
        }
        tm->tm_hour = alarm.time.hour;
        tm->tm_min = alarm.time.minute;
        tm->tm_sec = 0;
        return mktime(tm);
    }

    void Plugins::Alarm::updateTimestamps(const struct tm *tm, Alarm_t &cfg)
    {
        for(uint8_t i = 0; i < MAX_ALARMS; i++) {
            updateTimestamp(tm, cfg.alarms[i]);
        }
    }

    void Plugins::Alarm::updateTimestamp(const struct tm *_tm, SingleAlarm_t &alarm)
    {
        if (alarm.is_enabled && alarm.time.week_day.week_days == 0) {
            struct tm tmp = *_tm;
            struct tm *tm = &tmp;
            if (!((alarm.time.hour > _tm->tm_hour) || (alarm.time.hour == _tm->tm_hour && alarm.time.minute > _tm->tm_min))) {
                // alarm for tomorrow
                tm->tm_mday++;
                /* mktime(): tm_mday may contain values above 31, which are interpreted accordingly as the days that follow the last day of the selected month. */
            }
            tm->tm_hour = alarm.time.hour;
            tm->tm_min = alarm.time.minute;
            tm->tm_sec = 0;
            alarm.time.timestamp = mktime(tm); // convert to unixtime
        }
        else {
            alarm.time.timestamp = 0;
        }
    }

    void Plugins::Alarm::defaults()
    {
        Alarm alarm;
        config._H_SET(MainConfig().plugins.alarm.cfg, alarm.cfg);
    }

    void Plugins::Alarm::getWeekDays(Print &output, uint8_t weekdays, char none)
    {
        static auto weekdays_P = PSTR("SMTWTFS");
        for(uint8_t i = 0; i < 7; i++) {
            output.print((weekdays & _BV(i)) ? (char)pgm_read_byte(weekdays_P + i) : none);
        }
    }

    String Plugins::Alarm::getWeekDaysString(uint8_t weekdays, char none)
    {
        PrintString str;
        getWeekDays(str, weekdays, none);
        return str;
    }


    void Plugins::Alarm::dump(Print &output, Alarm_t &cfg)
    {

        for(uint8_t i = 0; i < MAX_ALARMS; i++) {
            const auto &alarm = cfg.alarms[i];
            output.printf_P(PSTR("alarm %u: enabled=%u mode=%u duration=%u ts=%u time=%02u:%02u weekdays="),
                i, alarm.is_enabled, alarm.mode, alarm.max_duration,
                alarm.time.timestamp, alarm.time.hour, alarm.time.minute
            );
            getWeekDays(output, alarm.time.week_day.week_days);
            if (alarm.time.timestamp) {
                char buf[32];
                time_t now = alarm.time.timestamp;
                strftime_P(buf, sizeof(buf), PSTR("%FT%T %Z"), localtime(&now));
                output.print(F(" localtime="));
                output.print(buf);
            }
            output.println();
        }
    }


    Plugins::Alarm::Alarm_t &Plugins::Alarm::getWriteableConfig()
    {
        return config._H_W_GET(MainConfig().plugins.alarm.cfg);
    }

    Plugins::Alarm::Alarm_t Plugins::Alarm::getConfig()
    {
        return config._H_GET(MainConfig().plugins.alarm.cfg);
    }

    void Plugins::Alarm::setConfig(Alarm_t &alarm)
    {
        config._H_SET(MainConfig().plugins.alarm.cfg, alarm);
    }

}
