/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "web_server.h"

#ifndef DEBUG_WEB_SERVER_ACTION
#define DEBUG_WEB_SERVER_ACTION                     1
#endif

#if DEBUG_WEB_SERVER_ACTION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace WebServer {

    namespace Action {

        enum class StateType : uint8_t {
            NONE = 0,
            UNAUTHORIZED,
            NEW,
            EXECUTED,
            RELOAD,
        };

        struct Id {
            uint32_t _id;
            StateType _state;

            Id(StateType state) : _id(0), _state(state) {}
            Id(uint32_t id, StateType state) : _id(id), _state(state) {}
        };

        using IdVector = std::vector<Id>;

        class Session {
        public:
            Session(StateType state) : _state(state) {}

            operator bool() const {
                return false;
            }

            operator StateType() const {
                return _state;
            }

        protected:
            StateType _state;
        };

        class Handler {
        public:
            Handler() {}

            Handler &getInstance() {
                if (_instance == nullptr) {
                    _instance = new Handler();
                }
                return *_instance;
            }

            //void setActionState(const ActionId &id, ActionStateType state);

            Session initSession(AsyncWebServerRequest *request, const String &url, const String &title);


        protected:


        private:
            static Handler *_instance;
        };


    }

}

#include <debug_helper_disable.h>
