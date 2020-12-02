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

struct SplitStringIntoInteger {

    using SplitIdListCallback = std::function<bool(long long)>;

    static void split(const char *idStr, SplitIdListCallback callback, int base = 10) {
        char *endPtr = nullptr;
        auto nextPtr = idStr;
        long long value;
        do {
            value = strtoll(nextPtr, &endPtr, base);
            if (!callback(value) || !endPtr) {
                break;
            }
            while(isspace(*endPtr) || *endPtr == ',') {
                endPtr++;
            }
            if (endPtr == nextPtr) {
                break;
            }
            nextPtr = endPtr;
        } while(*endPtr);
    }
};

PROGMEM_STRING_DEF(alerts_storage_filename, WEBUI_ALERTS_SPIFF_STORAGE);

using namespace WebAlerts;

#if WEBUI_ALERTS_USE_MQTT

AbstractStorage *AbstractStorage::create(StorageType type) {
    switch(type) {
#if WEBUI_ALERTS_USE_MQTT
        case StorageType::MQTT:
            return new MQTTStorage();
#endif
        case StorageType::FILE:
            return new FileStorage();
        default:
            break;
    }
    return nullptr;
}

void AbstractStorage::changeStorageToMQTT()
{
    delete _storage;
    // TODO copy local storage to MQTT
    _storage = AbstractStorage::create(AbstractStorage::StorageType::MQTT);
    __DBG_printf("_storage %p");
}

AbstractStorage *AbstractStorage::_storage = AbstractStorage::create(AbstractStorage::StorageType::FILE);

#else

static FileStorage _fileStorage;
AbstractStorage *AbstractStorage::_storage = &_fileStorage;

#endif


struct Json {
    static void printHeader(Print &output, time_t time, IdType alertId) {
        output.printf_P(PSTR("{\"created\":%lu,\"alert_id\":%u}"), (unsigned long)time, alertId);
    }

    template<typename _Ta>
    static bool getValue(const __FlashStringHelper *fpstr, const String &object, _Ta &value) {
        const uint8_t findLength = strlen_P((PGM_P)fpstr) + 3;
        char find[findLength + 1];
        *find = '"';
        strcpy_P(find + 1, (PGM_P)fpstr);
        strcat_P(find + 1, PSTR("\":"));
        auto strPtr = strstr(object.c_str(), find);
        if (strPtr) {
            value = static_cast<_Ta>(atol(strPtr + findLength));
            return true;
        }
        return false;
    }

};


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
        case OptionsType::IS_MQTT:
            return AbstractStorage::getInstance().isMqtt();
        case OptionsType::NONE:
            break;
    }
    return false;
}


#if WEBUI_ALERTS_USE_MQTT

MQTTStorage::MQTTStorage() :
    AbstractStorage(),
    MQTTComponent(MQTTComponent::ComponentType::STORAGE)
{
}

IdType MQTTStorage::addAlert(const String &message, Type type, ExpiresType expires, AddAlertActionType action)
{
    __DBG_printf("addAlert %s", message.c_str());
    switch(action)  {
        case AddAlertActionType::ADD: {
                //TODO
                bool success = false;
                auto &alertId = getMaxAlertId();
                String topic = _getTopic(++alertId, type, expires);
                if (_canSend()) {
                    __DBG_printf("topic %s: %s", topic.c_str(), message.c_str());
                    MQTTClient::getClient()->publish(topic, true, message, 2);
                    success = true; //TODO currently failures are not reported
                }
                if (!success) {
                    _messages.emplace(topic, message);
                    // _messages.emplace_back(topic, message);
                }
                return alertId;
            }
        case AddAlertActionType::REMOVE:
            dismissAlert(message.toInt());
            break;
        case AddAlertActionType::REMOVE_LIST:
            dismissAlerts(message);
            break;
    }
    return 1;
}

void MQTTStorage::dismissAlert(IdType id)
{
    __DBG_printf("dismissAlerts %d", id);
    bool success = false;
    // generate unique id
    PrintString idStr(F("%08x%03%05x"), time(nullptr), micros(), millis());
    idStr.printf_P(PSTR("%03x"), micros());
    String topic = _getRemoveTopic(idStr.c_str());
    if (_canSend()) {
        MQTTClient::getClient()->publish(topic, true, String(id), 2);
        success = true;
    }
    if (!success) {
        _messages.emplace(topic, String(id));
        //_messages.emplace_back(topic, String(id));
    }
}

void MQTTStorage::dismissAlerts(const String &idStr)
{
    SplitStringIntoInteger::split(idStr.c_str(), [this](long long value) {
        auto id = static_cast<IdType>(value);
        if (id > 0) {
            dismissAlert(id);
        }
        return true;
    });
}

void MQTTStorage::onConnect(MQTTClient *client)
{
    // subscribe to remove queue first
    __DBG_printf("subscribe %s", _getRemoveTopic().c_str());
    client->subscribe(this, _getRemoveTopic());
    // publish sync message and read all ids queued for removal
    client->publish(_getRemoveTopic(), false, F("SYNC"), 2);
    if (_messages.size()) {
        _startQueue();
    }
}

void MQTTStorage::onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason)
{
    __DBG_printf("disconnect %u, queue %u", reason, _messages.size());
    _stopQueue();
}

void MQTTStorage::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    auto topicLen = strlen(topic);
    if (strncmp(topic, _getRemoveTopic().c_str(), topicLen) == 0) {
        // done reading removal queue, subscribe to messages
        if (strcmp(payload, PSTR("SYNC")) == 0) {
            __DBG_printf("subscribe %s", _getMessageTopic(true).c_str());
            client->subscribe(this, _getMessageTopic(true));
        }
        else {
            __DBG_printf("%s: remove %s", topic, payload);
            _remove.emplace_back(static_cast<IdType>(atoll(payload)));
        }
    }
    else {
        IdType id = 0;
        if (topicLen > -WEBUI_ALERTS_MQTT_TOPIC_RPOS) {
            char *endPtr = nullptr;
            id = static_cast<IdType>(strtoul(topic + topicLen + WEBUI_ALERTS_MQTT_TOPIC_RPOS, &endPtr, 16));
            __DBG_printf("id '%s' end %s id %u / %08x", topic + topicLen + WEBUI_ALERTS_MQTT_TOPIC_RPOS, endPtr, id, id);
            if (endPtr && *endPtr == '/' && id) {
                if (std::find(_remove.begin(), _remove.end(), id) != _remove.end()) {
                    __DBG_printf("removing message %s", topic);
                    client->publish(topic, false, String(), 2); // publish empty message to remove it
                }
            }
        }
        __DBG_printf("%s: id %u message %s", topic, id, payload);
    }
}

String MQTTStorage::_getTopic(IdType id, Type type, ExpiresType expires) const
{
    return MQTTClient::formatTopic(String(), F(WEBUI_ALERTS_MQTT_TOPIC), id, expires, type);
}

String MQTTStorage::_getMessageTopic(bool wildcard) const
{
    return MQTTClient::formatTopic(String(), F(WEBUI_ALERTS_MQTT_MSG_TOPIC), wildcard ? PSTR("+") : emptyString.c_str());
}

String MQTTStorage::_getRemoveTopic(const char *suffix) const
{
    return MQTTClient::formatTopic(String(), F(WEBUI_ALERTS_MQTT_REMOVE_TOPIC), suffix ? suffix : PSTR("+"));
}

bool MQTTStorage::_canSend() const
{
    __DBG_printf("client %p connected %u", MQTTClient::getClient(), MQTTClient::getClient() ? MQTTClient::getClient()->isConnected() : false);
    return (MQTTClient::getClient() && MQTTClient::getClient()->isConnected());
}

void MQTTStorage::_startQueue()
{
    __DBG_printf("_startQueue");

    // send queue slowly and only if nothing else is in the MQTT queue
    _queueTimer.add(100, true, [this](Event::CallbackTimerPtr timer) {
        if (_canSend()) {
            auto client = MQTTClient::getClient();
            __DBG_printf("queued %u mqtt queue %u", _messages.size(), client->getQueueSize());
            if (client->getQueueSize() == 0) {
                auto &msg = _messages.front();
                client->publish(msg.getTopic(), true, msg.getMessage(), 2);
                _messages.pop();
                //_messages.erase(_messages.begin());
            }
        }
        if (_messages.empty()) {
            timer->disarm();
        }
    });
}

void MQTTStorage::_stopQueue()
{
    __DBG_printf("_stopQueue");
    _queueTimer.remove();
}

#endif

IdType FileStorage::addAlert(const String &message, Type type, ExpiresType expires, AddAlertActionType action)
{
    auto file = _openAlertStorage(false);
    __LDBG_printf("add=%s type=%u file=%u pos=%u size=%u", message.c_str(), type, !!file, file.position(), file.size());
    if (file) {
        auto now = time(nullptr);
        if (!IS_TIME_VALID(now)) {
            now = 0;
        }
        if (file.size()) {
            file.truncate(file.size() - 2);
            file.seek(0, SeekMode::SeekEnd);
            file.print(F(",\n"));
        }
        else {
            file.print('[');
            Json::printHeader(file, now, _alertId);
            file.print(F(",\n"));
        }
        switch(action) {
            case AddAlertActionType::REMOVE_LIST: {
                uint16_t count = 0;
                SplitStringIntoInteger::split(message.c_str(), [this, &count, &file](long long value) {
                    auto id = static_cast<IdType>(value);
                    if (id > 0) {
                        if (count++ == 0) {
                            file.print('{');
                        }
                        else {
                            file.print(F("},\n{"));
                        }
                        file.printf_P(PSTR("\"i\":%u,\"d\":%u"), ++_alertId, id);
                    }
                    return true;
                });

                // StringVector ids;
                // explode(message.c_str(), ',', ids);
                // file.print('{');
                // if (!ids.empty()) {
                //     uint16_t n = 0;
                //     for(const auto &id: ids) {
                //         if (n++ > 0) {
                //             file.print(F("},\n{"));
                //         }
                //         file.printf_P(PSTR("\"i\":%u,\"d\":%u"), ++_alertId, (uint32_t)id.toInt());
                //     }
                // }
            }
            break;
            case AddAlertActionType::REMOVE: {
                file.printf_P(PSTR("{\"i\":%u,\"d\":%u"), ++_alertId, message.toInt());
            }
            break;
            case AddAlertActionType::ADD: {
                file.printf_P(PSTR("{\"i\":%u,"), ++_alertId);
                if (now) {
                    file.printf_P(PSTR("\"ts\":%lu,"), (unsigned long)now);
                }
                file.printf_P(PSTR("\"t\":\"%s\",\"e\":%u"), getTypeStr(type), expires);
                if (message.length()) {
                    file.printf_P(PSTR(",\"m\":\""));
                    JsonTools::printToEscaped(file, message);
                    file.print('"');
                }
            }
            break;
        }
        file.print(F("}]\n"));
        _closeAlertStorage(file);
    }
    //  __LDBG_printf("id=%u,msg=%s,type=%s,time=%lu,exp=%u,dismissable=%d,persistent=%d",
    //     alert.getId(), alert.getMessage().c_str(), alert.getTypeStr(), alert.getTime(), alert.getExpires(), alert.isDismissable(), alert.isPersistent()
    // );
    return _alertId;
}

void FileStorage::dismissAlert(IdType id)
{
    addAlert(String(id), Type::NONE, ExpiresType::REBOOT, AddAlertActionType::REMOVE);
}

void FileStorage::dismissAlerts(const String &ids)
{
    addAlert(ids, Type::NONE, ExpiresType::REBOOT, AddAlertActionType::REMOVE_LIST);
}

File FileStorage::_openAlertStorage(bool readOnly)
{
    __LDBG_printf("file=%s read_only=%d safe_mode=%u", SPGM(alert_storage_filename), readOnly, config.isSafeMode());
    if (config.isSafeMode()) {
        return BSFS.open(FSPGM(alerts_storage_filename), readOnly ? fs::FileOpenMode::read : fs::FileOpenMode::appendplus);
    }
    return KFCFS.open(FSPGM(alerts_storage_filename), readOnly ? fs::FileOpenMode::read : fs::FileOpenMode::appendplus);
}

void FileStorage::_closeAlertStorage(File &file)
{
    if (file) {
        if (_getTimeSinceLastRewrite() >= kMinRewriteInterval) {
            __LDBG_printf("close rewrite time=%ums max=%ums", _getTimeSinceLastRewrite(), kMinRewriteInterval);
            _rewriteAlertStorage(file, RewriteType::KEEP_NON_PERSISTENT);
        }
#if 0
        else if (file.size() > kRewriteFilesize) {
            _resetLastWriteTime();
            __LDBG_printf("close rewrite file_size=%u max=%u", file.size(), kRewriteFilesize);
            _rewriteAlertStorage(file, RewriteType::KEEP_NON_PERSISTENT);
        }
#endif
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

void FileStorage::_rewriteAlertStorage(File &file, RewriteType rewriteType)
{
    struct Item;

    using ItemsVector = std::vector<Item>;

    ItemsVector items;
    struct Item {
        enum class ActionType {
            KEEP,
            DELETE
        };

        IdType id;
        ActionType action;

        bool operator==(IdType _id) const {
            return id == _id;
        }

        static bool canCopy(IdType id, ItemsVector &items) {
            auto iterator = std::find(items.begin(), items.end(), id);
            if (iterator == items.end()) {
                __LDBG_assert_printf(F("can_copy cannot find id") == nullptr, "can_copy id=%u cannot find id", id);
                return false;
            }
            return (iterator->action != ActionType::DELETE);
        }

        static Item *get(IdType id, ItemsVector &items) {
            auto iter = std::find(items.begin(), items.end(), id);
            if (iter == items.end()) {
                return nullptr;
            }
            return &(*iter);
        }

        // type KEEP adds item id does not exist
        // type DELETE adds item if id does not exists or replaces type KEEP
        static void add(IdType id, ActionType type, ItemsVector &items) {
            auto iterator = std::find(items.begin(), items.end(), id);
            if (iterator == items.end()) {
                items.emplace_back(id, type);
            }
            else if (type == ActionType::DELETE) {
                iterator->action = ActionType::DELETE;
            }
        }

        Item() : id(0), action(ActionType::DELETE) {}
        Item(IdType _id, ActionType _action) : id(_id), action(_action) {}
    };

    uint32_t created = 0;
    File tmpFile;
    bool readOnly = (rewriteType == RewriteType::READ_ONLY) || (_getTimeSinceLastRewrite() < kMinRewriteInterval);

    if (!file) {
        file = _openAlertStorage(readOnly);
    }

    if (file && file.seek(0, SeekMode::SeekSet)) {
        auto now = time(nullptr);
        if (!IS_TIME_VALID(now)) {
            now = 0;
        }

        while(file.available()) {
            String line = _readLine(file);
            if (line.length() == 0) {
                continue;
            }
            // __LDBG_printf("line=%s", line.c_str());

            if (Json::getValue(F("created"), line, created)) {
                uint32_t tmpId;
                if (Json::getValue(F("alert_id"), line, tmpId)) {
                    if (tmpId > _alertId) {
                        _alertId = tmpId;
                    }
                }
            }
            else {
                // alert entries
                IdType id = 0;
                if (Json::getValue(F("i"), line, id)) {
                    if (id > _alertId) {
                        _alertId = id;
                    }
                }
                if (id && readOnly == false) {
                    // add deleted messages to removeItems
                    uint32_t removeId;
                    if (Json::getValue(F("d"), line, removeId) && removeId) {
                        // __LDBG_printf("delete=%u", removeId);
                        Item::add(id, Item::ActionType::DELETE, items);
                    }
                    // remove non persistent messages
                    ExpiresType expires;
                    if (Json::getValue(F("e"), line, expires)) {
                        // __LDBG_printf("id=%u expires=%u reboot=%u", id, expires, reboot);
                        Item::add(
                            id,
                            (((rewriteType == RewriteType::REMOVE_NON_PERSISTENT) && (expires != ExpiresType::PERSISTENT)) ?
                                Item::ActionType::DELETE :
                                Item::ActionType::KEEP),
                            items
                        );
                    }
                }
            }
        }

#if !DEBUG_WEBUI_ALERTS
        if (readOnly) {
            return;
        }
#endif

        // count items and deleted items
        size_t removedCount = std::count_if(items.begin(), items.end(), [](const Item &item) {
            return item.action == Item::ActionType::DELETE;
        });
        size_t msgCount = items.size() - removedCount;

        __LDBG_printf("messages=%u remove=%u items=%u", msgCount, removedCount, items.size());

#if DEBUG_WEBUI_ALERTS
        if (readOnly) {
            __LDBG_printf("read only mode");
            return;
        }
#endif

        if (removedCount == 0) {
            __LDBG_printf("nothing to remove");
            return;
        }

        _lastRewrite = millis();

        if (msgCount == 0) {
            // we can replace the file with the header
            file.close();
            if (config.isSafeMode()) {
                file = BSFS.open(FSPGM(alerts_storage_filename), fs::FileOpenMode::write);
            }
            else {
                file = KFCFS.open(FSPGM(alerts_storage_filename), fs::FileOpenMode::write);
            }
            if (file) {
                file.print('[');
                Json::printHeader(file, now, _alertId);
                file.print(F("]\n"));
                file.close();
            }
        }
        else if (file.seek(0, SeekMode::SeekSet)) {

            // store content in new file and rename it when done to avoid corruption
            tmpFile = tmpfile(sys_get_temp_dir(), FSPGM(alerts_storage_filename));
            String tmpFileName = tmpFile.fullName();
            __LDBG_printf("tmpfile=%s", tmpFileName.c_str());
            if (tmpFile) {
                tmpFile.print('[');
                Json::printHeader(tmpFile, now, _alertId);
                while(file.available()) {
                    String line = _readLine(file);
                    if (line.length() == 0) {
                        continue;
                    }
                    // get id
                    IdType id;
                    if (Json::getValue(F("i"), line, id)) {
                        __LDBG_printf("line=%s id=%u copy=%u", line.c_str(), id, Item::canCopy(id, items));
                        if (Item::canCopy(id, items)) {
                            // copy this alert
                            String_rtrim_P(line, PSTR(",]\n"));
                            tmpFile.print(F(",\n"));
                            tmpFile.print(line);
                        }
                    }
                }
                tmpFile.print(F("]\n"));

                __LDBG_printf("old_size=%u new_size=%u rename=%s to %s", file.size(), tmpFile.size(), tmpFileName.c_str(), FSPGM(alerts_storage_filename));
                tmpFile.close();
                file.close();

                if (config.isSafeMode()) {
                    BSFS.remove(FSPGM(alerts_storage_filename));
                    file = KFCFS.open(tmpFileName, fs::FileOpenMode::read);
                    File file2 = BSFS.open(FSPGM(alerts_storage_filename), fs::FileOpenMode::write);
                    if (file && file2) {
                        while(file.available()) {
                            file2.write(file.read());
                        }
                    }
                    file.close();
                    file2.close();
                    KFCFS.remove(tmpFileName);
                }
                else {
                    if (!KFCFS.remove(FSPGM(alerts_storage_filename))) {
                        __LDBG_assert_printf(F("failed to remove webui-alerts.json") == nullptr, "failed to remove %s", FSPGM(alerts_storage_filename));
                    }
                    if (!KFCFS.rename(tmpFileName, FSPGM(alerts_storage_filename))) {
                        __LDBG_assert_printf(F("failed to rename webui-alerts.json") == nullptr, "failed to rename %s to %s", tmpFileName.c_str(), FSPGM(alerts_storage_filename));
                        KFCFS.remove(tmpFileName);
                    }

                }
            }
        }
        else {
            file.close();
            __LDBG_assert_printf(F("removing webui-alerts.json after seek to 0 failure") == nullptr, "seek to 0 failed, removing %s", FSPGM(alerts_storage_filename));
            // SPIFFS might be corrupted
            // since its not any vital information, try to delete the file
            KFCFS.remove(FSPGM(alerts_storage_filename));
        }
    }
}

#endif
