/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

#ifndef IOT_ALARM_PLUGIN_MAX_ALERTS
#define IOT_ALARM_PLUGIN_MAX_ALERTS                         10
#endif
#ifndef IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION
#define IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION               900
#endif

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // Alarm

        namespace AlarmConfigNS {

            enum class ModeType : uint8_t {
                BOTH,       // can be used if silent or buzzer is not available
                SILENT,
                BUZZER,
                MAX
            };

            enum class WeekDaysType : uint8_t {
                NONE,
                SUNDAY = _BV(0),
                MONDAY = _BV(1),
                TUESDAY = _BV(2),
                WEDNESDAY = _BV(3),
                THURSDAY = _BV(4),
                FRIDAY = _BV(5),
                SATURDAY = _BV(6),
                WEEK_DAYS = _BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5),
                WEEK_END = _BV(0)|_BV(6),
            };

            using TimeType = uint32_t;
            using CallbackType = std::function<void(ModeType mode, uint16_t maxDuration)>;

            union __attribute__packed__ WeekDay_t {
                WeekDaysType week_days_enum;
                uint8_t week_days;
                struct {
                    uint8_t sunday: 1;              // 0
                    uint8_t monday: 1;              // 1
                    uint8_t tuesday: 1;             // 2
                    uint8_t wednesday: 1;           // 3
                    uint8_t thursday: 1;            // 4
                    uint8_t friday: 1;              // 5
                    uint8_t saturday: 1;            // 6
                };
            };

            struct __attribute__packed__ AlarmTime_t {
                using Type = AlarmTime_t;
                TimeType timestamp;
                CREATE_UINT8_BITFIELD(hour, 8);
                CREATE_UINT8_BITFIELD(minute, 8);
                WeekDay_t week_day;

                static void set_bits_weekdays(Type &obj, uint8_t value) { \
                    obj.week_day.week_days = value;
                } \
                static uint8_t get_bits_weekdays(const Type &obj) { \
                    return obj.week_day.week_days; \
                }
            };

            struct __attribute__packed__ SingleAlarmType {
                using Type = SingleAlarmType;
                AlarmTime_t time;
                CREATE_UINT16_BITFIELD(max_duration, 16); // limit in seconds, 0 = unlimited
                CREATE_ENUM_BITFIELD(mode, ModeType);
                CREATE_UINT8_BITFIELD(is_enabled, 1);
            };

            struct __attribute__packed__ AlarmConfigType
            {
                using  Type = AlarmConfigType;
                static constexpr uint8_t MAX_ALARMS = IOT_ALARM_PLUGIN_MAX_ALERTS;
                static constexpr uint16_t DEFAULT_MAX_DURATION = IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION;
                static constexpr uint16_t STOP_ALARM = std::numeric_limits<uint16_t>::max() - 1;

                SingleAlarmType alarms[MAX_ALARMS];
            };

            class Alarm : public AlarmConfigType, public KFCConfigurationClasses::ConfigGetterSetter<AlarmConfigType, _H(MainConfig().plugins.alarm.cfg) CIF_DEBUG(, &handleNameAlarm_t)> {
            public:
                Alarm();

                // update timestamp for a single alarm
                // set now to the current time plus a safety margin to let the system install the alarm
                // auto now = time(nullptr) + 30;
                // auto tm = localtime(&now);
                // - if the alarm is disabled, timestamp is set to 0
                // - if any weekday is selected, the timestamp is set to 0
                // - if none of the weekdays are selected, the timestamp is set to unixtime at hour:minute of today. if hour:minute has
                // passed already, the alarm is set for tomorrow (+1 day) at hour:minute
                static void updateTimestamp(const struct tm *tm, SingleAlarmType &alarm);

                // calls updateTimestamp() for each entry
                static void updateTimestamps(const struct tm *tm, AlarmConfigType &cfg);

                // return time of the next alarm
                static TimeType getTime(const struct tm *tm, SingleAlarmType &alarm);

                static void getWeekDays(Print &output, uint8_t weekdays, char none = 'x');
                static String getWeekDaysString(uint8_t weekdays, char none = 'x');

                static void defaults();
                static void dump(Print &output, AlarmConfigType &cfg);

                AlarmConfigType cfg;
            };

        }
    }
}
