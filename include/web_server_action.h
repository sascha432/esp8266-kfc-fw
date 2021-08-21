/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "web_server.h"
#include <EventScheduler.h>
#if ESP8266
#    include <coredecls.h>
#endif

#ifndef DEBUG_WEB_SERVER_ACTION
#    define DEBUG_WEB_SERVER_ACTION (0 || defined(DEBUG_ALL))
#endif

#if DEBUG_WEB_SERVER_ACTION
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WebServer {

    namespace Action {

        enum class AuthType : uint8_t {
            NONE = 0,
            AUTH,
            NO_AUTH
        };

        enum class StateType : uint8_t {
            UNAUTHORIZED,           // session cannot be created since the client is not authenticated
            NEW,                    // session started
            EXECUTING,              // session executing
            FINISHED,               // session finished
            EXPIRED,                // session not found
            MAX,
        };
        static_assert(static_cast<int>(StateType::MAX) <= 8, "overflow");

        class Session {
        public:

            static constexpr uint32_t kUnixtimeStartOffset = 1616500000;        // Tue Mar 23 2021
            static constexpr uint16_t kMaxLifetime = 300;                       // max. lifetime in seconds

        public:
            Session(const Session &) = delete;
            Session &operator=(const Session &) = delete;

            Session(StateType state);
            Session(uint32_t id, StateType state);
            Session(time_t time, StateType state);
            ~Session();

            Session(Session &&session) noexcept;
            Session &operator=(Session &&session) noexcept;

            bool operator==(uint32_t id) const;
            uint32_t getId() const;

            time_t getTime() const;

            operator StateType() const;
            Session &operator=(StateType state);
            StateType getState() const;
            bool isNew() const;
            bool isFinished() const;
            bool isExecuting() const;
            bool isError() const;
            bool isExpired() const;
            bool isUnauthorized() const;

            const char *getUrl() const;
            const char *getTitle() const;

            void setDetails(const String &url, const String &title);
            void setDetails(const __FlashStringHelper *url, const __FlashStringHelper *title);
            void setDetails(PGM_P url, PGM_P title);

            const __FlashStringHelper *getStatusFPStr() const;
            PGM_P getStatus() const;
            MessageType getStatusType() const;

            void setStatus(const __FlashStringHelper *str, MessageType type = MessageType::PRIMARY);
            void removeStatus();

        protected:
            uint32_t _id;
            char *_details;
            PGM_P _status;
            uint32_t _state: 3;
            uint32_t _type: 3;
            // uint32_t _freeStatus: 1; //not implemented
        };

        using SessionVector = std::vector<Session>;

        class Handler {
        public:
            Handler() : _error(StateType::EXPIRED) {
                _Timer(_timer).add(Event::seconds(60), true, [](Event::CallbackTimerPtr timer) {
                    _instance->timerCallback();
                });
            }

            static Handler &getInstance() {
                if (_instance == nullptr) {
                    _instance = new Handler();
                }
                return *_instance;
            }

            //void setActionState(const ActionId &id, ActionStateType state);

            Session &initSession(AsyncWebServerRequest *request, const char *url, const char *title, AuthType authenticated = AuthType::NONE) {
                return initSession(request, reinterpret_cast<const __FlashStringHelper *>(url), reinterpret_cast<const __FlashStringHelper *>(title));
            }
            Session &initSession(AsyncWebServerRequest *request, const String &url, const String &title, AuthType authenticated = AuthType::NONE) {
                return initSession(request, reinterpret_cast<const __FlashStringHelper *>(url.c_str()), reinterpret_cast<const __FlashStringHelper *>(title.c_str()));
            }
            Session &initSession(AsyncWebServerRequest *request, const __FlashStringHelper *url, const __FlashStringHelper *title, AuthType authenticated = AuthType::NONE);
            Session &addSession();

            Session *getSession(uint32_t id);
            // String getCookieName(const __FlashStringHelper *url) const;

            void timerCallback();

        protected:
            SessionVector _sessions;
            Session _error;
            Event::Timer _timer;

        private:
            static Handler *_instance;
        };

        inline Session::Session(StateType state) :
            _id(0),
            _details(nullptr),
            _status(nullptr),
            _state(static_cast<uint32_t>(state)),
            _type(static_cast<uint32_t>(MessageType::WARNING))
        {
        }

        inline Session::Session(uint32_t id, StateType state) :
            _id(id),
            _details(nullptr),
            _status(nullptr),
            _state(static_cast<uint32_t>(state)),
            _type(static_cast<uint32_t>(MessageType::WARNING))
        {
        }

        inline Session::Session(time_t time, StateType state) :
            _id(time - kUnixtimeStartOffset),
            _details(nullptr),
            _status(nullptr),
            _state(static_cast<uint32_t>(state)),
            _type(static_cast<uint32_t>(MessageType::WARNING))
        {
        }

        inline Session::~Session()
        {
            if (_details) {
                delete[] _details;
            }
        }

        inline Session::Session(Session &&session) noexcept :
            _id(std::exchange(session._id, 0)),
            _details(std::exchange(session._details, nullptr)),
            _status(std::exchange(session._status, nullptr)),
            _state(session._state),
            _type(session._type)
        {
            session._state = 0;
            session._type = 0;
        }

#pragma push_macro("new")
#undef new

        inline Session &Session::operator=(Session &&session) noexcept
        {
            this->~Session();
            ::new(static_cast<void *>(this)) Session(std::move(session));
            return *this;
        }

#pragma pop_macro("new")

        inline bool Session::operator==(uint32_t id) const
        {
            return _id == id;
        }

        inline Session::operator StateType() const
        {
            return getState();
        }

        inline Session &Session::operator=(StateType state)
        {
            _state = static_cast<uint32_t>(state);
            return *this;
        }

        inline uint32_t Session::getId() const
        {
            return _id;
        }

        inline StateType Session::getState() const
        {
            return static_cast<StateType>(_state);
        }

        inline time_t Session::getTime() const
        {
            return _id + kUnixtimeStartOffset;
        }

        inline bool Session::isNew() const
        {
            return _state == static_cast<int>(StateType::NEW);
        }

        inline bool Session::isFinished() const
        {
            return _state == static_cast<int>(StateType::FINISHED);
        }

        inline bool Session::isExecuting() const
        {
            return _state == static_cast<int>(StateType::EXECUTING);
        }

        inline bool Session::isError() const
        {
            return _state == static_cast<int>(StateType::UNAUTHORIZED) || _state == static_cast<int>(StateType::EXPIRED);
        }

        inline bool Session::isExpired() const
        {
            return _state == static_cast<int>(StateType::EXPIRED);
        }

        inline bool Session::isUnauthorized() const
        {
            return _state == static_cast<int>(StateType::UNAUTHORIZED);
        }

        inline const char *Session::getUrl() const
        {
            return _details;
        }

        inline const char *Session::getTitle() const
        {
            return _details + strlen(_details) + 1;
        }

        inline const __FlashStringHelper *Session::getStatusFPStr() const
        {
            return FPSTR(_status);
        }

        inline PGM_P Session::getStatus() const
        {
            return _status;
        }

        inline MessageType Session::getStatusType() const
        {
            return static_cast<MessageType>(_type);
        }

        inline void Session::setDetails(const String &url, const String &title)
        {
            setDetails(url.c_str(), title.c_str());
        }

        inline void Session::setDetails(const __FlashStringHelper *url, const __FlashStringHelper *title)
        {
            setDetails(reinterpret_cast<PGM_P>(url), reinterpret_cast<PGM_P>(title));
        }

        inline void Session::setDetails(PGM_P url, PGM_P title)
        {
            if (_details) {
                delete[] _details;
            }
            _details = new char[strlen_P(url) + strlen_P(title) + 2];
            if (_details) {
                strcpy_P(_details, url);
                strcpy_P(_details + strlen(_details) + 1, title);
            }
        }

        inline void Session::setStatus(const __FlashStringHelper *str, MessageType type)
        {
            _status = reinterpret_cast<PGM_P>(str);
            _type = static_cast<uint32_t>(type);
        }

        inline void Session::removeStatus()
        {
            _status = nullptr;
            _type = static_cast<uint32_t>(MessageType::WARNING);
        }

    }

}

#if DEBUG_WEB_SERVER_ACTION
#    include <debug_helper_disable.h>
#endif
