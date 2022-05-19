/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>

struct FormatTime {

    static String getTimeStr(bool format24h, struct tm *tm) {
        return PrintString(format24h ? F("%H:%M:%S") : F("%I:%M:%S%p"), tm);
    }

    static String getTimeStr(bool format24h, time_t time) {
        return getTimeStr(format24h, localtime(&time));
    }

    static String getDateTimeStr(bool format24h, struct tm *tm) {
        return PrintString(format24h ? F("%Y-%m-%d %H:%M") : F("%Y-%m-%d %I:%M%p"), tm);
    }

    static String getDateTimeStr(bool format24h, time_t time) {
        return getDateTimeStr(format24h, localtime(&time));
    }

    static String getTimeTZStr(bool format24h, struct tm *tm) {
        return PrintString(format24h ? F("%H:%M:%S %Z") : F("%I:%M:%S%p %Z"), tm);
    }

    static String getTimeTZStr(bool format24h, time_t time) {
        return getTimeStr(format24h, localtime(&time));
    }

    static String getTimezoneStr(bool format24h, struct tm *tm) {
        return PrintString(format24h ? F("%Z") : F("%p %Z"), tm);
    }

    static String getTimezoneStr(bool format24h, time_t time) {
        return getTimezoneStr(format24h, localtime(&time));
    }

};
