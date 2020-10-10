/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <EnumHelper.h>
#include <MicrosTimer.h>

#if WEBUI_ALERTS_ENABLED

#ifndef DEBUG_WEBUI_ALERTS
#define DEBUG_WEBUI_ALERTS                              1
#endif

#ifndef WEBUI_ALERTS_SPIFF_STORAGE
#define WEBUI_ALERTS_SPIFF_STORAGE                      "/.pvt/alerts.json"
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
        IS_LOGGER          = 0x010,
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

    class FileStorage {
    public:

        FileStorage() : _alertId(1), _lastRewrite(0) {}

        IdType addAlert(const String &message, Type type, ExpiresType expires, AddAlertActionType action = AddAlertActionType::ADD);
        void dismissAlert(IdType id);
        void dismissAlerts(const String &id);
        void printAlertsAsJson(PrintHtmlEntitiesString &output, IdType minAlertId, bool separator = false);

        void _readAlertStorage(IdType minAlertId = 0);
        File _openAlertStorage(bool readOnly = false);
        void _closeAlertStorage(File &file);
        void _rewriteAlertStorage(File &file, RewriteType type);
        IdType _removeAlert(IdType id);

        static String _readLine(File &file);

        inline static FileStorage &getInstance() {
            return _webAlerts;
        }

        static IdType getMaxAlertId() {
            return getInstance()._alertId;
        }

    private:
        uint32_t _getTimeSinceLastRewrite() const {
            return get_time_diff(_lastRewrite, millis());
        }

        void _resetLastWriteTime() {
            _lastRewrite = 0;
        }

    private:
        IdType _alertId;
        uint32_t _lastRewrite;
        static FileStorage _webAlerts;
    };

    class Base : public AbstractBase {
    public:

        static IdType addAlert(const String &message, Type type, ExpiresType expires) {
            return FileStorage::getInstance().addAlert(message, type, expires);
        }

        static void dismissAlert(IdType id) {
            FileStorage::getInstance().dismissAlert(id);
        }

        // comma separated list
        static void dismissAlerts(const String &ids) {
            FileStorage::getInstance().dismissAlerts(ids);
        }

        static bool hasOption(OptionsType option);

        static void readStorage(RewriteType type = RewriteType::READ_ONLY) {
            auto &fs = FileStorage::getInstance();
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
