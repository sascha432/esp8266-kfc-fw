/**
 * Author: sascha_lammers@gmx.de
 */

#if WEBUI_ALERTS_ENABLED

#inclue "WebUIAlerts.h"

#if DEBUG_WEBUI_ALERTS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(alerts_storage_filename, WEBUI_ALERTS_SPIFF_STORAGE);


void KFCFWConfiguration::AlertMessage::remove()
{
    config.dismissAlert(_id);
}

// format: 0:id,1:expires[,2:counter,3:type,4:dismissable,5:time,6:message]

bool KFCFWConfiguration::AlertMessage::fromString(AlertMessage &alert, const String &line)
{
    StringVector items;
    explode(line.c_str(), ',', items, 7);
    debug_printf_P(PSTR("items=%d:%s\n"), items.size(), implode(',', items).c_str());
    if (items.size() >= 2) {
        uint32_t id = items[0].toInt();
        int32_t expires = items[1].toInt();
        if (id) {
            if (items.size() < 7) {
                alert = AlertMessage(id);
            }
            else {
                auto message = items[6];
                String_rtrim_P(message, PSTR("\r\n"));
                alert = AlertMessage(id, message, static_cast<AlertMessage::TypeEnum_t>(items[3].toInt()), expires, items[4].toInt());
            }
    //#if DEBUG_KFC_CONFIG
    #if DEBUG
            String tmp;
            AlertMessage::toString(tmp, alert);
            debug_printf_P(PSTR("str=%s\n"), tmp.c_str());
    #endif
            return true;
        }
    }
    debug_printf_P(PSTR("failed parsing %s\n"), line.c_str());
    return false;
}

void KFCFWConfiguration::AlertMessage::toString(String &line, const AlertMessage &alert)
{
    line = PrintString(F("%u,%d,%u,%u,%u,%lu,%s\n"), alert._id, alert._expires, alert._counter, alert._type, alert._dismissable, (unsigned long)alert._time, alert._message.c_str());
    debug_printf_P(PSTR("str=%s\n"), line.c_str());
}

void KFCFWConfiguration::AlertMessage::toString(String &line, uint32_t alertId)
{
    line = PrintString(F("%u,%d\n"), alertId, ExpiresEnum_t::EXPIRED);
    debug_printf_P(PSTR("str=%s\n"), line.c_str());
}


uint32_t KFCFWConfiguration::addAlert(const String &message, AlertMessage::TypeEnum_t type, int32_t expires, bool dismissable)
{
    for(auto &alert: _alerts) {
        if (!alert.isExpired() && alert.getMessage().equals(message)) {
            alert.setCount(alert.getCount() + 1);
            debug_printf_P(PSTR("id=%u,msg=%s,counter=%d\n"), alert.getId(), alert.getMessage().c_str(), alert.getCount());
            return alert.getId();
        }
    }
    _alertId++;
    _alerts.emplace_back(_alertId, message, type, expires, dismissable);
    auto &alert = _alerts.back();
    if (alert.isPersistent()) {
        auto file = _openAlertStorage(true);
        if (file) {
            String line;
            AlertMessage::toString(line, alert);
            file.write(line.c_str(), line.length());
        }
    }
    debug_printf_P(PSTR("id=%u,msg=%s,type=%s,time=%lu,exp=%u,dismissable=%d,persistent=%d\n"), alert.getId(), alert.getMessage().c_str(), alert.getTypeStr(), alert.getTime(), alert.getExpires(), alert.isDismissable(), alert.isPersistent());
    return _alertId;
}

void KFCFWConfiguration::dismissAlert(uint32_t id)
{
    auto alertId = _removeAlert(id);
    if (alertId) {
        auto file = _openAlertStorage(true);
        if (file) {
            String line;
            AlertMessage::toString(line, alertId);
            file.write(line.c_str(), line.length());
        }
    }
}

void KFCFWConfiguration::printAlertsAsJson(Print &output, bool sep, uint32_t minAlertId)
{
    for(auto &alert: config.getAlerts()) {
        if (!alert.isExpired() && alert.getId() >= minAlertId) {
            if (sep) {
                output.print(',');
            }
            else {
                sep = true;
            }
            PrintHtmlEntitiesString message;
            JsonTools::printToEscaped(message, alert.getMessage());
            message.print(F(HTML_E(h5)));
            auto ts = alert.getTime();
            auto counter = alert.getCount();
            if (ts || counter > 1) {
                message.print(F(HTML_S(small)));
                if (counter > 1) {
                    message.printf_P(PSTR("Message repeated %u times"), counter);
                    if (ts) {
                        message.print(F(", last timestamp "));
                    }
                }
            }
            if (ts) {
                char buf[32];
                timezone_strftime_P(buf, sizeof(buf), PSTR("%Y-%m-%d %H:%M:%S"), timezone_localtime(&ts));
                message.print(buf);
            }
            if (ts || counter > 1) {
                message.print(F(HTML_E(small)));
            }
            output.printf_P(PSTR("{'t':'%s','i':%d,'m':'"), alert.getTypeStr(), alert.getId());
            output.print(F(HTML_SA(h5, HTML_A("class", "mb-0"))));
            output.print(message);
            output.printf_P(PSTR("','n':%s}"), alert.isDismissable() ? FSPGM(false) : FSPGM(true));
        }
    }
}

uint32_t KFCFWConfiguration::_removeAlert(uint32_t id)
{
    uint32_t alertId = 0;
    _alerts.erase(std::remove_if(_alerts.begin(), _alerts.end(), [id, &alertId](const AlertMessage &alert) {
        if (alert.getId() == id) {
            if (alert.isPersistent()) {
                alertId = alert.getId();
            }
            debug_printf_P(PSTR("removed=%u,persistent=%d\n"), alert.getId(), !!alertId);
            return true;
        }
        return false;
    }), _alerts.end());
    return alertId;
}

void KFCFWConfiguration::_readAlertStorage()
{
    auto file = _openAlertStorage(false);
    if (file) {
        String line;
        for(;;) {
            line = file.readStringUntil('\n');
            debug_printf_P(PSTR("line=%s\n"), line.c_str());
            if (line.length() == 0) {
                break;
            }
            AlertMessage alert;
            if (AlertMessage::fromString(alert, line)) {
                if (alert.isExpired()) {

                }
                else {
                    _alerts.push_back(alert);
                    _alertId = std::max(_alertId, alert.getId());
                }
            }
        }
    }
}

File KFCFWConfiguration::_openAlertStorage(bool append)
{
    debug_printf_P(PSTR("file=%s,append=%d\n"), SPGM(alert_storage_filename), append);
    return SPIFFS.open(FSPGM(alerts_storage_filename), append ? FileOpenMode::appendplus : FileOpenMode::read);
}

void KFCFWConfiguration::_closeAlertStorage(File &file)
{
    if (file && file.size() > 3072) {
        file.close();
        _rewriteAlertStorage();
    }
    else {
        file.close();
    }
}

void KFCFWConfiguration::_rewriteAlertStorage()
{
    //TODO
    // remove expired and persistent messages
    // read storage to get persistent messages
    // sort by time
    // write new storage
    // delete old and rename new storage
}

#endif
