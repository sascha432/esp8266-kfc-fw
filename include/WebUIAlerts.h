/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class AlertMessage {
public:
    typedef enum {
        SUCCESS,
        DANGER,
        WARNING,
        INFO,
    } TypeEnum_t;

#if WEBUI_ALERTS_ENABLED

        typedef enum {
            EXPIRED = -2,
            REBOOT = -1,
            PERSISTENT = 0,
            NEVER = PERSISTENT,
        } ExpiresEnum_t;
        AlertMessage() : _id(0), _expires(ExpiresEnum_t::EXPIRED) {
        }
        AlertMessage(uint32_t id) : _id(id), _expires(ExpiresEnum_t::EXPIRED) {
        }
        AlertMessage(uint32_t id, const String &message, TypeEnum_t type, int32_t expires = ExpiresEnum_t::NEVER, bool dismissable = true) : _id(id), _counter(1), _message(message), _type(type), _dismissable(dismissable), _expires(expires), _time(time(nullptr)) {
        }
        uint32_t getId() const {
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
        TypeEnum_t getType() const {
            return _type;
        }
        const __FlashStringHelper *getTypeStr() const {
            switch(_type) {
                case TypeEnum_t::SUCCESS:
                    return F("success");
                case TypeEnum_t::WARNING:
                    return F("warning");
                case TypeEnum_t::INFO:
                    return F("info");
                case TypeEnum_t::DANGER:
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
        void setExpires(int32_t expires = ExpiresEnum_t::EXPIRED) {
            _expires = expires;
        }
        int32_t getExpires() const {
            return _expires;
        }
        bool isExpired() const {
            switch(_expires) {
                case ExpiresEnum_t::EXPIRED:
                    return true;
                case ExpiresEnum_t::NEVER:
                case ExpiresEnum_t::REBOOT:
                    return false;
                default:
                    break;
            }
            if (!IS_TIME_VALID(_time)) {
                return false;
            }
            time_t now = time(nullptr);
            return (IS_TIME_VALID(now) && (now > _time + _expires));
        }
        bool isPersistent() const {
            return _expires >= ExpiresEnum_t::NEVER;
        }
        void remove();
        static bool fromString(AlertMessage &alert, const String &line);
        static void toString(String &line, const AlertMessage &alert);
        static void toString(String &line, uint32_t alertId); // creates a record that the alert has been deleted
    private:
        uint32_t _id;
        uint32_t _counter;
        String _message;
        TypeEnum_t _type;
        bool _dismissable;
        int32_t _expires;
        time_t _time;
    };
    typedef std::vector<AlertMessage> AlertVector;

    uint32_t addAlert(const String &message, AlertMessage::TypeEnum_t type, int32_t expires = AlertMessage::ExpiresEnum_t::NEVER, bool dismissable = true);
    void dismissAlert(uint32_t id);
    AlertVector &getAlerts() {
        return _alerts;
    }
    void printAlertsAsJson(Print &output, bool sep, uint32_t minAlertId);

private:
    friend class WebServerPlugin;

    void _readAlertStorage();
    File _openAlertStorage(bool append = false);
    void _closeAlertStorage(File &file);
    void _rewriteAlertStorage();
    uint32_t _removeAlert(uint32_t id);

    AlertVector _alerts;
    uint32_t _alertId;

#else

    static void logger(const String &message, AlertMessage::TypeEnum_t type) {
        auto str = String(F("WebUI Alert: "));
        str += message;
        switch(type) {
            case TypeEnum_t::DANGER:
                Logger_error(str);
                break;
            case TypeEnum_t::WARNING:
                Logger_warning(str);
            case TypeEnum_t::SUCCESS:
            case TypeEnum_t::INFO:
            default:
                Logger_notice(str);
                break;
        }
    }

#endif

};

#if WEBUI_ALERTS_ENABLED

#ifndef DEBUG_WEBUI_ALERTS
#define DEBUG_WEBUI_ALERTS                          1
#endif

#ifndef WEBUI_ALERTS_SPIFF_STORAGE
#define WEBUI_ALERTS_SPIFF_STORAGE                  "/.alerts_storage"
#endif

PROGMEM_STRING_DECL(alerts_storage_filename);

#ifndef WEBUI_ALERTS_POLL_INTERVAL
#define WEBUI_ALERTS_POLL_INTERVAL                  10000
#endif

#ifndef WEBUI_ALERTS_REWRITE_SIZE
#define WEBUI_ALERTS_REWRITE_SIZE                   3500
#endif

#ifndef WEBUI_ALERTS_MAX_HEIGHT
#define WEBUI_ALERTS_MAX_HEIGHT                     "200px"
#endif

#define WebUIAlerts_add(message, ...)
#define WebUIAlerts_remove(alertId)
#define WebUIAlerts_printAsJson(output, minAlertId)
#define WebUIAlerts_readStorage()

#else

#define WebUIAlerts_add(message, type, ...)             AlertMessage::logger(message, type);
#define WebUIAlerts_remove(alertId)                     ;
#define WebUIAlerts_printAsJson(output, minAlertId)     ;
#define WebUIAlerts_readStorage()                       ;

#endif
