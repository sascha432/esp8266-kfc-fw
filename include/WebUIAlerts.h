/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <EnumHelper.h>

#if WEBUI_ALERTS_ENABLED

#ifndef DEBUG_WEBUI_ALERTS
#define DEBUG_WEBUI_ALERTS                              1
#endif

#ifndef WEBUI_ALERTS_SPIFF_STORAGE
#define WEBUI_ALERTS_SPIFF_STORAGE                      "/.pvt/alerts_storage"
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
    using Vector = std::vector<Message>;

    using IdType = uint32_t;

    enum class OptionsType {
        NONE               = 0x00,
        ENABLED            = 0x01,
        GET_ALERTS         = 0x02,
        PRINT_ALERTS_JSON  = 0x04,
        PERSISTANT_STORAGE = 0x08,
        IS_LOGGER          = 0x010,
    };

    enum class Type : uint8_t {
        MIN = 0,
        SUCCESS = MIN,
        DANGER,
        ERROR = DANGER,
        WARNING,
        SECURITY = WARNING,
        INFO,
        NOTICE = INFO,
        MAX
    };

    enum class ExpiresType : int32_t {
        MIN = -2,
        EXPIRED = MIN,
        REBOOT,
        PERSISTENT = 0,
        NEVER = PERSISTENT,
        MAX = INT32_MAX
    };

    class AbstractBase {
    public:

        static IdType addAlert(const String &message, Type type, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) = delete;
        static bool hasOption(OptionsType option) = delete;

        static void dismissAlert(IdType id) {
        }

        static Vector getAlerts() {
            return Vector();
        }

        static void printAlertsAsJson(PrintHtmlEntitiesString &, IdType, bool = false) {
        }

        static void readStorage() {
        }

        static void writeStorage() {
        }

    };


#if WEBUI_ALERTS_ENABLED

    class Message {
    public:
        using Type = WebAlerts::Type;
        using IdType = WebAlerts::IdType;
        using ExpiresType = WebAlerts::ExpiresType;

        Message(IdType id = 0) :
            _id(id),
            _counter(0),
            _expires(ExpiresType::EXPIRED),
            _time(0),
            _type(Type::MIN),
            _dismissable(false)
        {
        }

        Message(IdType id, const String &message, Type type, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) :
            _message(message),
            _id(id),
            _counter(1),
            _expires(expires),
            _time(time(nullptr)),
            _type(type),
            _dismissable(dismissable)
        {
        }

        IdType getId() const {
            return _id;
        }

        uint32_t getCount() const {
            return _counter;
        }

        void setCount(uint32_t counter) {
            _counter = counter;
            auto now = time(nullptr);
            if (IS_TIME_VALID(now)) {
                _time = now;
            }
        }

        const String &getMessage() const {
            return _message;
        }

        bool isDismissable() const {
            return _dismissable;
        }

        Type getType() const {
            return _type;
        }

        const __FlashStringHelper *getTypeStr() const {
            switch(_type) {
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

        void setTime(time_t time) {
            _time = time;
        }

        time_t getTime() const {
            if (IS_TIME_VALID(_time)) {
                return _time;
            }
            return 0;
        }

        void setExpires(ExpiresType expires = ExpiresType::EXPIRED) {
            _expires = expires;
        }

        ExpiresType getExpires() const {
            return _expires;
        }

        bool isExpired() const {
            switch(_expires) {
                case ExpiresType::EXPIRED:
                    return true;
                case ExpiresType::NEVER:
                case ExpiresType::REBOOT:
                    return false;
                default:
                    break;
            }
            if (!IS_TIME_VALID(_time)) {
                return false;
            }
            time_t now = time(nullptr);
            return (IS_TIME_VALID(now) && (now > _time + static_cast<int32_t>(_expires)));
        }

        bool isPersistent() const {
            return _expires >= ExpiresType::NEVER;
        }

        void remove();

        // format: 0:id,1:expires[,2:counter,3:type,4:dismissable,5:time,6:message]
        //
        //  on success:
        //      returns pointer to the third item or the end of the line
        //      id and type are set
        //
        //  on failure:
        //     returns nullptr
        static const char *getFromString(const String &line, IdType &id, ExpiresType &type);

        // returns false on error and alert is not modified
        static bool fromString(Message &alert, const String &line);

        static void toString(String &line, const Message &alert);

        // creates a record that the alert has been deleted
        static void toString(String &line, IdType alertId);

    private:
        String _message;
        IdType _id;
        uint32_t _counter;
        ExpiresType _expires;
        time_t _time;
        Type _type;
        bool _dismissable;
    };

    class FileStorage {
    public:
        static constexpr size_t kReadwriteFilesize = 1024 * 3;

        IdType addAlert(const String &message, Type type, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true);
        void dismissAlert(IdType id);
        void printAlertsAsJson(PrintHtmlEntitiesString &output, IdType minAlertId, bool separator = false);

        void _readAlertStorage();
        File _openAlertStorage(bool append = false);
        void _closeAlertStorage(File &file);
        void _rewriteAlertStorage();
        IdType _removeAlert(IdType id);

        inline Vector &getAlerts() {
            return _alerts;
        }

        inline static FileStorage &getInstance() {
            return _webAlerts;
        }

    private:
        Vector _alerts;
        IdType _alertId;
        static FileStorage _webAlerts;
    };

    class Base : public AbstractBase {
    public:

        static IdType addAlert(const String &message, Type type, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) {
            return FileStorage::getInstance().addAlert(message, type, expires, dismissable);
        }

        static void dismissAlert(IdType id) {
            FileStorage::getInstance().dismissAlert(id);
        }

        static Vector &getAlerts() {
            return FileStorage::getInstance().getAlerts();
        }

        static void printAlertsAsJson(PrintHtmlEntitiesString &output, IdType minAlertId, bool separator = false){
            FileStorage::getInstance().printAlertsAsJson(output, minAlertId, separator);
        }

        static bool hasOption(OptionsType option);

        static void readStorage() {
            FileStorage::getInstance()._readAlertStorage();
        }

    };

#else

    struct Message {
    };

    class Base : public AbstractBase {
    public:
        static IdType addAlert(const String &message, Type type, ExpiresType = ExpiresType::NEVER, bool = true) {
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

        static IdType add(const String &message, Type type, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) {
            return addAlert(message, type, expires, dismissable);
        }

        static void dimiss(IdType id) {
           dismissAlert(id);
       }

        static IdType danger(const String &message, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) {
            return add(message, Type::DANGER, expires, dismissable);
        }

        static IdType danger(const String &message, bool dismissable, ExpiresType expires = ExpiresType::NEVER) {
            return add(message, Type::DANGER, expires, dismissable);
        }

        static IdType error(const String &message, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) {
            return add(message, Type::ERROR, expires, dismissable);
        }

        static IdType error(const String &message, bool dismissable, ExpiresType expires = ExpiresType::NEVER) {
            return add(message, Type::ERROR, expires, dismissable);
        }

        static IdType warning(const String &message, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) {
            return add(message, Type::WARNING, expires, dismissable);
        }

        static IdType warning(const String &message, bool dismissable, ExpiresType expires = ExpiresType::NEVER) {
            return add(message, Type::WARNING, expires, dismissable);
        }

        static IdType notice(const String &message, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) {
            return add(message, Type::NOTICE, expires, dismissable);
        }

        static IdType notice(const String &message, bool dismissable, ExpiresType expires = ExpiresType::NEVER) {
            return add(message, Type::NOTICE, expires, dismissable);
        }

        static IdType success(const String &message, ExpiresType expires = ExpiresType::NEVER, bool dismissable = true) {
            return add(message, Type::SUCCESS, expires, dismissable);
        }

        static IdType success(const String &message, bool dismissable, ExpiresType expires = ExpiresType::NEVER) {
            return add(message, Type::SUCCESS, expires, dismissable);
        }
    };

}
