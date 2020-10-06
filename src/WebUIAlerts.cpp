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

IdType FileStorage::addAlert(const String &message, Type type, ExpiresType expires, IdType updateId)
{
    auto file = _openAlertStorage(true);
    __LDBG_printf("add=%s type=%u file=%u pos=%u size=%u", message.c_str(), type, !!file, file.position(), file.size());
    if (file) {
        auto now = time(nullptr);
        if (!IS_TIME_VALID(now)) {
            now = 0;
        }
        if (file.size()) {
            auto trunc = file.truncate(file.size() - 2);
            auto seek = file.seek(0, SeekMode::SeekEnd);
            auto written = file.print(F(",\n"));
            __LDBG_printf("truncate=%d seek=%d written=%d size=%u pos=%u", trunc, seek, written, file.size(), file.position());
        }
        else {
            file.printf_P(PSTR("[{\"created\":%lu,\"alert_id\":%u},"), now, _alertId);
        }
        if (updateId == ~0U) {
            updateId = ++_alertId;
        }
        if (type == Type::NONE || expires == ExpiresType::DELETED) {
            file.printf_P(PSTR("{\"i\":%u,\"d\":%u"), ++_alertId, updateId);
        }
        else {
            file.printf_P(PSTR("{\"i\":%u,\"ts\":%lu,\"t\":\"%s\",\"e\":%u"), updateId, now, getTypeStr(type), expires);
            if (message.length()) {
                file.printf_P(PSTR(",\"m\":\""));
                JsonTools::printToEscaped(file, message);
                file.print('"');
            }
        }
        file.print("}]\n");
        _closeAlertStorage(file);
    }
    //  __LDBG_printf("id=%u,msg=%s,type=%s,time=%lu,exp=%u,dismissable=%d,persistent=%d",
    //     alert.getId(), alert.getMessage().c_str(), alert.getTypeStr(), alert.getTime(), alert.getExpires(), alert.isDismissable(), alert.isPersistent()
    // );
    return _alertId;
}

void FileStorage::dismissAlert(IdType id)
{
    addAlert(emptyString, Type::NONE, ExpiresType::DELETED, id);
}

File FileStorage::_openAlertStorage(bool append)
{
    __LDBG_printf("file=%s,append=%d", SPGM(alert_storage_filename), append);
    return KFCFS.open(FSPGM(alerts_storage_filename), append ? fs::FileOpenMode::appendplus : fs::FileOpenMode::read);
}

void FileStorage::_closeAlertStorage(File &file)
{
    if (file && file.size() > kReadwriteFilesize) {
        _rewriteAlertStorage(file, false);
    }
    else {
        file.close();
    }
}

String FileStorage::_readLine(File &file)
{
    String line;
    int c = file.read();
    while (c >= 0 && c != '\n') {
        line += (char)c;
        c = file.read();
    }
    return line;
}

void FileStorage::_rewriteAlertStorage(File &file, bool reboot)
{
    time_t created = 0;
    File tmpFile;

    if (file && file.seek(0, SeekMode::SeekSet)) {
        std::vector<IdType> removeItems;
        size_t lines = 0;
        size_t messageCount = 0;
        while(true) {
            int pos;
            String line = _readLine(file);
            if (line.length() == 0) {
                break;
            }
            __LDBG_printf("line=%s num=%u", line.c_str(), lines);
            lines++;
            if (lines  == 1) {
                pos = line.indexOf(F("\"created\":"));
                if (pos != -1) {
                    created = atol(line.c_str() + pos + 10);
                    __LDBG_printf("created=%lu", created);
                }
                pos = line.indexOf(F("\"alert_id\":"));
                if (pos != -1) {
                    auto aid = (IdType)atol(line.c_str() + pos + 11);
                    __LDBG_printf("alert_id=%u this->_alertid=%u", aid, _alertId);
                    if (aid > _alertId) {
                        _alertId = aid;
                    }
                }
            }
            else {
                IdType id = 0;
                // add deleted messages to removeItems
                pos = line.indexOf(F("\"d\":"));
                if (pos != -1) {
                    auto id = (IdType)atol(line.c_str() + pos + 4);
                    __LDBG_printf("delete=%u", id);
                    if (id > 0) {
                        if (std::find(removeItems.begin(), removeItems.end(), id) == removeItems.end()) {
                            removeItems.push_back(id);
                        }
                    }
                }
                else {
                    // get id
                    pos = line.indexOf(F("\"i\":"));
                    if (pos != -1) {
                        id = (IdType)atol(line.c_str() + pos + 4);
                        __LDBG_printf("id=%u", id);
                        messageCount++;
                    }
                    // add non persistent message to removeItems
                    if (reboot && id) {
                        pos = line.indexOf(F("\"e\":"));
                        if (pos != -1) {
                            auto expires = static_cast<ExpiresType>(atoi(line.c_str() + pos + 4));
                            __LDBG_printf("id=%u expires=%u reboot=%u", id, expires, reboot);
                            if (expires == ExpiresType::REBOOT) {
                                if (std::find(removeItems.begin(), removeItems.end(), id) == removeItems.end()) {
                                    removeItems.push_back(id);
                                }
                            }
                        }
                    }
                }
            }
        }
        auto now = time(nullptr);
        if (!IS_TIME_VALID(now)) {
            now = 0;
        }
        __LDBG_printf("messageCount=%u removeItems=%u ", messageCount, removeItems.size());
        if (messageCount <= removeItems.size()) {
            file.truncate(0);
            file.printf_P(PSTR("[{\"created\":%lu,\"alert_id\":%u}]\n"), now, _alertId);
            file.close();
        }
        else if (file.seek(0, SeekMode::SeekSet)) {

            tmpFile = tmpfile(sys_get_temp_dir(), FSPGM(alerts_storage_filename));
            String tmpFileName = tmpFile.fullName();
            __LDBG_printf("tmpfile=%s", tmpFileName.c_str());
            if (tmpFile) {
                tmpFile.printf_P(PSTR("[{\"created\":%lu,\"alert_id\":%u}"), now, _alertId);
                while(true) {
                    int pos;
                    String line = _readLine(file);
                    if (line.length() == 0) {
                        break;
                    }
                    // get id
                    IdType id = 0;
                    pos = line.indexOf(F("\"i\":"));
                    if (pos != -1) {
                        id = (IdType)atol(line.c_str() + pos + 4);
                    }
                    __LDBG_printf("line=%s id=%u remove=%u", line.c_str(), id, std::find(removeItems.begin(), removeItems.end(), id) != removeItems.end());
                    if (id && std::find(removeItems.begin(), removeItems.end(), id) == removeItems.end()) {
                        // copy this message
                        String_rtrim_P(line, PSTR(",]\n"));
                        tmpFile.print(F(",\n"));
                        tmpFile.print(line);
                    }
                }
                tmpFile.print(F("]\n"));

                __LDBG_printf("old_size=%u new_size=%u rename=%s to %s", file.size(), tmpFile.size(), tmpFileName.c_str(), FSPGM(alerts_storage_filename));

                tmpFile.close();
                file.close();
                KFCFS.remove(FSPGM(alerts_storage_filename));
                if (!KFCFS.rename(tmpFileName, FSPGM(alerts_storage_filename))) {
                    KFCFS.remove(tmpFileName);
                }
            }
        }
    }
}

#endif
