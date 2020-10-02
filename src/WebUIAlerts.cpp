/**
 * Author: sascha_lammers@gmx.de
 */

#if WEBUI_ALERTS_ENABLED

#include <Arduino_compat.h>
#include <FileOpenMode.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <JsonTools.h>
#include <cerrno>
#include "WebUIAlerts.h"
#include "kfc_fw_config.h"

#if DEBUG_WEBUI_ALERTS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(alerts_storage_filename, WEBUI_ALERTS_SPIFF_STORAGE);

using namespace WebAlerts;

FileStorage FileStorage::_webAlerts;

bool Base::hasOption(OptionsType option)
{
    switch(option) {
        case OptionsType::GET_ALERTS:
            return true;
        case OptionsType::PRINT_ALERTS_JSON:
            return true;
        case OptionsType::PERSISTANT_STORAGE:
            return true;
        case OptionsType::ENABLED:
            return KFCConfigurationClasses::System::Flags::getConfig().is_webalerts_enabled;
        case OptionsType::IS_LOGGER:
            return false;
        case OptionsType::NONE:
            break;
    }
    return false;
}

void Message::remove()
{
    FileStorage::getInstance().dismissAlert(_id);
}

const char *Message::getFromString(const String &line, IdType &id, ExpiresType &type)
{
    char *endPtr = nullptr;
    type = ExpiresType::EXPIRED;
    errno = 0;
    id = strtoul(line.c_str(), &endPtr, 10);
    if (errno || id == 0) {
        id = 0;
        return nullptr;
    }
    if (*endPtr != ',') {
        return nullptr;
    }
    auto endPtr2 = ++endPtr;
    auto value = strtol(endPtr, &endPtr2, 10);
    if (errno || endPtr == endPtr2) {
        return nullptr;
    }
    type = static_cast<ExpiresType>(value);
    while(isspace(*endPtr2)) {
        endPtr2++;
    }
    if (*endPtr2 == 0) {
        return endPtr2;
    }
    if (*endPtr2 == ',') {
        return endPtr2 + 1;
    }
    return nullptr;
}

// format: 0:id,1:expires[,2:counter,3:type,4:dismissable,5:time,6:message]

bool Message::fromString(Message &alert, const String &line)
{
    IdType id;
    ExpiresType expires;
    auto start = getFromString(line, id, expires);
    if (!start) {
        __LDBG_printf("failed parsing %s", line.c_str());
        return false;
    }
    if (!*start) {
        alert = Message(id);
        __LDBG_printf("str=%s", [&alert]() { String tmp; Message::toString(tmp, alert); return tmp; }().c_str());
        return true;
    }
    StringVector items;
    explode(line.c_str(), ',', items, 7);
    __LDBG_printf("items=%d:%s", items.size(), implode(',', items).c_str());
    if (items.size() != 7) {
        __LDBG_printf("failed parsing %s", line.c_str());
        return false;
    }
    String_rtrim(items[6]);
    alert = Message(id, items[6], static_cast<Type>(items[3].toInt()), expires, items[4].toInt());
    __LDBG_printf("str=%s", [&alert]() { String tmp; Message::toString(tmp, alert); return tmp; }().c_str());
    return true;
}

void Message::toString(String &line, const Message &alert)
{
    line = PrintString(F("%u,%d,%u,%u,%u,%lu,"), alert._id, alert._expires, alert._counter, alert._type, alert._dismissable, (unsigned long)alert._time);
    line.reserve(line.length() + alert._message.length() + 2);
    line += alert._message;
    __LDBG_printf("str=%s", line.c_str());
    line += '\n';
}

void Message::toString(String &line, IdType alertId)
{
    line = PrintString(F("%u,%d"), alertId, ExpiresType::EXPIRED);
    __LDBG_printf("str=%s", line.c_str());
    line += '\n';
}


IdType FileStorage::addAlert(const String &message, Type type, ExpiresType expires, bool dismissable)
{
    __LDBG_printf("add=%s type=%u exp=%d dimiss=%u", message.c_str(), type, expires, dismissable);
    for(auto &alert: _alerts) {
        if (!alert.isExpired() && alert.getMessage().equals(message)) {
            alert.setCount(alert.getCount() + 1);
            __LDBG_printf("id=%u,msg=%s,counter=%d", alert.getId(), alert.getMessage().c_str(), alert.getCount());
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
            Message::toString(line, alert);
            file.write(line.c_str(), line.length());
        }
    }
    __LDBG_printf("id=%u,msg=%s,type=%s,time=%lu,exp=%u,dismissable=%d,persistent=%d",
        alert.getId(), alert.getMessage().c_str(), alert.getTypeStr(), alert.getTime(), alert.getExpires(), alert.isDismissable(), alert.isPersistent()
    );
    return _alertId;
}

void FileStorage::dismissAlert(IdType id)
{
    auto alertId = _removeAlert(id);
    if (alertId) {
        auto file = _openAlertStorage(true);
        if (file) {
            String line;
            Message::toString(line, alertId);
            file.write(line.c_str(), line.length());
        }
    }
}

void FileStorage::printAlertsAsJson(PrintHtmlEntitiesString &output, IdType minAlertId, bool separator)
{
    auto mode = output.setMode(PrintHtmlEntitiesString::Mode::RAW);
    output.print('[');
    for(auto &alert: getAlerts()) {
        if (!alert.isExpired() && alert.getId() >= minAlertId) {
            if (separator) {
                output.print(',');
            }
            else {
                separator = true;
            }

            output.printf_P(PSTR("{\"t\":\"%s\",\"i\":%d,\"m\":\""), alert.getTypeStr(), alert.getId());

            PrintHtmlEntitiesString message = F(HTML_SA(h5, HTML_A("class", "mb-0")));
            JsonTools::printToEscaped(output, message);
            message = alert.getMessage();
            JsonTools::printToEscaped(output, message);
            message = F(HTML_E(h5));
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
                message.strftime_P(PSTR("%Y-%m-%d %H:%M:%S"), ts);
            }
            if (ts || counter > 1) {
                message.print(F(HTML_E(small)));
            }
            JsonTools::printToEscaped(output, message);

            output.printf_P(PSTR("\",\"n\":%s}"), alert.isDismissable() ? SPGM(false) : SPGM(true));
        }
    }
    output.print(']');
    output.setMode(mode);
}

IdType FileStorage::_removeAlert(IdType id)
{
    IdType alertId = 0;
    _alerts.erase(std::remove_if(_alerts.begin(), _alerts.end(), [id, &alertId](const Message &alert) {
        if (alert.getId() == id) {
            if (alert.isPersistent()) {
                alertId = alert.getId();
            }
            __LDBG_printf("removed=%u,persistent=%d", alert.getId(), !!alertId);
            return true;
        }
        return false;
    }), _alerts.end());
    return alertId;
}

void FileStorage::_readAlertStorage()
{
    auto file = _openAlertStorage(false);
    if (file) {
        String line;
        for(;;) {
            line = file.readStringUntil('\n');
            __LDBG_printf("line=%s", line.c_str());
            if (line.length() == 0) {
                break;
            }
            Message alert;
            if (Message::fromString(alert, line)) {
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

File FileStorage::_openAlertStorage(bool append)
{
    __LDBG_printf("file=%s,append=%d", SPGM(alert_storage_filename), append);
    return KFCFS.open(FSPGM(alerts_storage_filename), append ? fs::FileOpenMode::appendplus : fs::FileOpenMode::read);
}

void FileStorage::_closeAlertStorage(File &file)
{
    if (file && file.size() > kReadwriteFilesize) {
        file.close();
        _rewriteAlertStorage();
    }
    else {
        file.close();
    }
}

void FileStorage::_rewriteAlertStorage()
{
    //TODO
    // remove expired and persistent messages
    // read storage to get persistent messages
    // sort by time
    // write new storage
    // delete old and rename new storage
    auto file = tmpfile(sys_get_temp_dir(), FSPGM(alerts_storage_filename));
    if (file) {
        String tmpfile = file.fullName();
        //TODO copy alerts
        file.close();
        KFCFS.remove(FSPGM(alerts_storage_filename));
        KFCFS.rename(tmpfile, FSPGM(alerts_storage_filename));
    }
}

#endif
