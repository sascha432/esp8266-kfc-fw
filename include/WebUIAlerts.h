/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <queue>
#include <EnumHelper.h>
#include <MicrosTimer.h>

#if WEBUI_ALERTS_ENABLED

#ifndef DEBUG_WEBUI_ALERTS
#define DEBUG_WEBUI_ALERTS                              1
#endif

#ifndef WEBUI_ALERTS_USE_MQTT
#define WEBUI_ALERTS_USE_MQTT                           0
#warning EXPERIMENTIAL
// MQTT credentials are exposed over the unencrypted web interface
// The web browser accessing the WebUI must be able to access the MQTT server directly
// Messages that cannot be delivered to MQTT before a reboot/crash, or if the queue memory limit has been exceeded, are lost
// If Safe Mode is enabled, the local storage is being used
#endif

#if !MQTT_SUPPORT && WEBUI_ALERTS_USE_MQTT
#error MQTT support required
#endif

#if WEBUI_ALERTS_USE_MQTT
#include "../src/plugins/mqtt/mqtt_client.h"
#endif


#ifndef WEBUI_ALERTS_SPIFF_STORAGE
#define WEBUI_ALERTS_SPIFF_STORAGE                      "/.pvt/alerts.json"
#endif

// webui_alerts
#ifndef WEBUI_ALERTS_MQTT_TOPIC_NAME
#define WEBUI_ALERTS_MQTT_TOPIC_NAME                    "wua"
#endif

// %08x = unique id
// %08x = expiration time
// %02x = type
#ifndef WEBUI_ALERTS_MQTT_TOPIC
#define WEBUI_ALERTS_MQTT_TOPIC                         WEBUI_ALERTS_MQTT_TOPIC_NAME "/m/%08x/%08x_%02x"
#endif

// start of the 8 digit hex id from end of the topic
#ifndef WEBUI_ALERTS_MQTT_TOPIC_RPOS
#define WEBUI_ALERTS_MQTT_TOPIC_RPOS                    -20
#endif

// wildcard topic for messages, %s = '#'
#ifndef WEBUI_ALERTS_MQTT_MSG_TOPIC
#define WEBUI_ALERTS_MQTT_MSG_TOPIC                     WEBUI_ALERTS_MQTT_TOPIC_NAME "/m/%s"
#endif

// wildcard for removals, %s = '+'
// persistent messages will be removed upon reconnect
#ifndef WEBUI_ALERTS_MQTT_REMOVE_TOPIC
#define WEBUI_ALERTS_MQTT_REMOVE_TOPIC                  WEBUI_ALERTS_MQTT_TOPIC_NAME "/r/%s"
#endif


PROGMEM_STRING_DECL(alerts_storage_filename);

#ifndef WEBUI_ALERTS_POLL_INTERVAL
#define WEBUI_ALERTS_POLL_INTERVAL                      10000
#endif

#ifndef WEBUI_ALERTS_REWRITE_SIZE
#define WEBUI_ALERTS_REWRITE_SIZE                       3500
#endif

#ifndef WEBUI_ALERTS_MAX_HEIGHT
#define WEBUI_ALERTS_MAX_HEIGHT                         "200px"
#endif

#endif

namespace WebAlerts {

    class Message;
    class FileStorage;
    using Vector = std::vector<Message>;
    using IdType = uint32_t;

    enum class AddAlertActionType {
        ADD,
        REMOVE,             // id stored in message
        REMOVE_LIST,        // ids stored as CSV in message
    };

    enum class RewriteType {
        READ_ONLY,                  // reads max. alert id
        REMOVE_NON_PERSISTENT,      // removes all non persistent messages
        KEEP_NON_PERSISTENT         // remove all deleted messages
    };

    // maximum number of alerts to keep when rewriting the file
    static constexpr size_t kMaxAlerts = 100;

    // rewrite file if filesize exceeds this number in bytes
    static constexpr size_t kRewriteFilesize = 1024 * 2;

    // wait at least n milliseconds before rewriting the file
    static constexpr int kMinRewriteInterval = 60 * 1000;

    enum class OptionsType {
        NONE               = 0x00,
        ENABLED            = 0x01,
        GET_ALERTS         = 0x02,
        PRINT_ALERTS_JSON  = 0x04,
        PERSISTANT_STORAGE = 0x08,
        IS_LOGGER          = 0x10,
        IS_MQTT            = 0x20
    };

    enum class ExpiresType : uint8_t {
        REBOOT,
        PERSISTENT
    };

    enum class Type : uint8_t {
        MIN = 0,
        NONE = MIN,
        SUCCESS,
        DANGER,
        ERROR = DANGER,
        WARNING,
        SECURITY = WARNING,
        INFO,
        NOTICE = INFO,
        MAX
    };

    inline static const __FlashStringHelper *getTypeStr(Type type) {
        switch(type) {
            case Type::SUCCESS:
                return F("success");
            case Type::WARNING:
                return F("warning");
            case Type::INFO:
                return F("info");
            case Type::DANGER:
            default:
                return F("danger");
        }
    }

    class AbstractBase {
    public:

        static IdType addAlert(const String &message, Type type, ExpiresType expires) = delete;
        static bool hasOption(OptionsType option) = delete;
        static void dismissAlert(IdType id) {}

        static void readStorage() {}
        static void writeStorage() {}
    };


#if WEBUI_ALERTS_ENABLED

    class AbstractStorage {
    public:
        enum class StorageType {
            FILE,
#if WEBUI_ALERTS_USE_MQTT
            MQTT,
#endif
        };
    public:
        AbstractStorage() : _alertId(1) {
        }
#if WEBUI_ALERTS_USE_MQTT
        virtual ~AbstractStorage() {}
#endif

        static AbstractStorage *create(StorageType type);
#if WEBUI_ALERTS_USE_MQTT
        static void changeStorageToMQTT();
#endif

        virtual IdType addAlert(const String &message, Type type, ExpiresType expires, AddAlertActionType action = AddAlertActionType::ADD) = 0;
        virtual void dismissAlert(IdType id) = 0;
        virtual void dismissAlerts(const String &id) = 0;
        // virtual void printAlertsAsJson(PrintHtmlEntitiesString &output, IdType minAlertId, bool separator = false) {}

        inline static AbstractStorage &getInstance() {
            return *_storage;
        }

        static IdType &getMaxAlertId() {
            return getInstance()._alertId;
        }

        virtual bool isMqtt() {
            return false;
        }

    protected:
        IdType _alertId;
        static AbstractStorage *_storage;
    };

#if WEBUI_ALERTS_USE_MQTT

    class MQTTStorage : public AbstractStorage, public MQTTComponent {
    public:
        class MQTTMessage {
        public:
            MQTTMessage(const String &topic, const String &message) : _topic(topic), _message(message) {}

            const String &getTopic() const {
                return _topic;
            }
            const String &getMessage() const {
                return _message;
            }

        private:
            String _topic;
            String _message;
        };

        using MessageVector = std::queue<MQTTMessage>;
        // using MessageVector = std::vector<MQTTMessage>;
        using RemoveIdsVector = std::vector<IdType>;
    public:
        MQTTStorage();

#if MQTT_AUTO_DISCOVERY
        virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) {
            return nullptr;
        }
        virtual uint8_t getAutoDiscoveryCount() const {
            return 0;
        }
#endif

        virtual void onConnect(MQTTClient *client);
        virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
        virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

        virtual IdType addAlert(const String &message, Type type, ExpiresType expires, AddAlertActionType action = AddAlertActionType::ADD);
        virtual void dismissAlert(IdType id);
        virtual void dismissAlerts(const String &id);

        // void _readAlertStorage(IdType minAlertId = 0);
        // File _openAlertStorage(bool readOnly = false);
        // void _closeAlertStorage(File &file);
        // void _rewriteAlertStorage(File &file, RewriteType type);
        // IdType _removeAlert(IdType id);

        inline static MQTTStorage &getInstance() {
            return reinterpret_cast<MQTTStorage &>(AbstractStorage::getInstance());
        }

        virtual bool isMqtt() override {
            return true;
        }

    private:
        String _getTopic(IdType id, Type type, ExpiresType expires) const;
        String _getMessageTopic(bool wildcard) const;
        String _getRemoveTopic(const char *suffix = nullptr) const;

        bool _canSend() const;
        void _startQueue();
        void _stopQueue();

        MessageVector _messages;
        RemoveIdsVector _remove;
        Event::Timer _queueTimer;
    };

#endif

    class FileStorage : public AbstractStorage {
    public:
        FileStorage() : AbstractStorage(), _lastRewrite(0) {}

        virtual IdType addAlert(const String &message, Type type, ExpiresType expires, AddAlertActionType action = AddAlertActionType::ADD);
        virtual void dismissAlert(IdType id);
        virtual void dismissAlerts(const String &id);
        // virtual void printAlertsAsJson(PrintHtmlEntitiesString &output, IdType minAlertId, bool separator = false);

        void _readAlertStorage(IdType minAlertId = 0);
        File _openAlertStorage(bool readOnly = false);
        void _closeAlertStorage(File &file);
        void _rewriteAlertStorage(File &file, RewriteType type);
        IdType _removeAlert(IdType id);

        static String _readLine(File &file);

        inline static FileStorage &getInstance() {
            return reinterpret_cast<FileStorage &>(AbstractStorage::getInstance());
        }

        uint32_t _getTimeSinceLastRewrite() const {
            return get_time_diff(_lastRewrite, millis());
        }

        void _resetLastWriteTime() {
            _lastRewrite = 0;
        }

    private:
        uint32_t _lastRewrite;
    };

    class Base : public AbstractBase {
    public:
        using StorageClass = FileStorage;

        static IdType addAlert(const String &message, Type type, ExpiresType expires) {
            return StorageClass::getInstance().addAlert(message, type, expires);
        }

        static void dismissAlert(IdType id) {
            StorageClass::getInstance().dismissAlert(id);
        }

        // comma separated list
        static void dismissAlerts(const String &ids) {
            StorageClass::getInstance().dismissAlerts(ids);
        }

        static bool hasOption(OptionsType option);

        static void readStorage(RewriteType type = RewriteType::READ_ONLY) {
            auto &fs = StorageClass::getInstance();
            File file = fs._openAlertStorage(type == RewriteType::READ_ONLY);
            if (file) {
                fs._rewriteAlertStorage(file, type);
                file.close();
            }
        }

    };

#else

    struct Message {
    };

    class Base : public AbstractBase {
    public:
        static IdType addAlert(const String &message, Type type, ExpiresType expires) {
            logger(message, type);
        }

        static bool hasOption(OptionsType option) {
            switch(option) {
                case OptionsType::IS_LOGGER:
                    return true;
                default:
                case OptionsType::ENABLED:
                    break;
            }
            return false;
        }

    private:
        static void logger(const String &message, Type type) {
            auto str = String(F("WebUI Alert: "));
            //TODO remove html
            str += message;
            str.replace(F("<br>"), String('\n'));
            str.replace(F("<small>"), emptyString);
            str.replace(F("</small>"), emptyString);
            switch(type) {
                case Type::DANGER:
                    Logger_error(str);
                    break;
                case Type::WARNING:
                    Logger_warning(str);
                    break;
                case Type::SUCCESS:
                case Type::INFO:
                default:
                    Logger_notice(str);
                    break;
            }
        }

    };

#endif

    class Alert : public Base {
    public:

        static IdType add(const String &message, Type type, ExpiresType expires = ExpiresType::PERSISTENT) {
            return addAlert(message, type, expires);
        }

        static void dimiss(IdType id) {
           dismissAlert(id);
       }

        static IdType danger(const String &message, ExpiresType type = ExpiresType::PERSISTENT) {
            return add(message, Type::DANGER);
        }

        static IdType error(const String &message, ExpiresType type = ExpiresType::PERSISTENT) {
            return add(message, Type::ERROR);
        }

        static IdType warning(const String &message, ExpiresType type = ExpiresType::PERSISTENT) {
            return add(message, Type::WARNING);
        }

        static IdType notice(const String &message, ExpiresType type = ExpiresType::PERSISTENT) {
            return add(message, Type::NOTICE);
        }

        static IdType success(const String &message, ExpiresType type = ExpiresType::PERSISTENT) {
            return add(message, Type::SUCCESS);
        }
    };

}
