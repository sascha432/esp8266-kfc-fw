/**
 * Author: sascha_lammers@gmx.de
 */

#include "web_server_action.h"

#if DEBUG_WEB_SERVER_ACTION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace WebServer {

    namespace Action {

        Handler *Handler::_instance;

        Session &Handler::initSession(AsyncWebServerRequest *request, const __FlashStringHelper *url, const __FlashStringHelper *title, AuthType authenticated)
        {
            // create new session
            auto &plugin = Plugin::getInstance();
            if (authenticated == AuthType::NONE) {
                authenticated = plugin.isAuthenticated(request) ? AuthType::AUTH : AuthType::NO_AUTH;
            }
            if (authenticated != AuthType::AUTH) {
                __DBG_printf("%s: unauthorized", url);
                request->send(403);
                _error = StateType::UNAUTHORIZED;
                return _error;
            }

            HttpHeaders headers;
            headers.addNoCache(true);
            headers.replace<HttpConnectionHeader>(HttpConnectionHeader::ConnectionType::CLOSE);

            uint32_t id = request->arg(F("action")).toInt();
            if (id) {
                auto session = getSession(id);
                if (!session) {
                    __DBG_printf("%s: no session", url);
                    Plugin::message(request, MessageType::DANGER, F("This session has expired"), title, headers);
                    _error = StateType::EXPIRED;
                    return _error;
                }

                __DBG_printf("%s: status: %s", url, session->getStatusFPStr() ? session->getStatusFPStr() : F("No status message available"));
                Plugin::message(request, session->getStatusType(), session->getStatusFPStr() ? session->getStatusFPStr() : F("No status message available"), title, headers);
                return *session;
            }

            auto &session = addSession();
            session.setDetails(url, title);

            String redirUrl = PrintString(F("%s?action=%u"), session.getUrl(), session.getId());
            __DBG_printf("redirecting to %s", redirUrl.c_str());
            request->send(HttpLocationHeader::redir(request, redirUrl, headers));
            return session;
        }

        Session &Handler::addSession()
        {
            time_t expires = time(nullptr) + Session::kMaxLifetime;
            noInterrupts();
            // find unused id
            for(;;) {
                auto iterator = std::find(_sessions.begin(), _sessions.end(), static_cast<uint32_t>(expires - Session::kUnixtimeStartOffset));
                if (iterator == _sessions.end()) {
                    break;
                }
                expires++;
            }
            _sessions.emplace_back(expires, StateType::NEW);
            interrupts();
            return _sessions.back();
        }

        Session *Handler::getSession(uint32_t id)
        {
            auto iterator = std::find(_sessions.begin(), _sessions.end(), id);
            if (iterator == _sessions.end()) {
                return nullptr;
            }
            return &(*iterator);
        }

        void Handler::timerCallback()
        {
            uint32_t minId = time(nullptr) - Session::kUnixtimeStartOffset + Session::kMaxLifetime;

            __DBG_printf("session min. id %u sessions %u", minId, _sessions.size());

            noInterrupts();
            _sessions.erase(std::remove_if(_sessions.begin(), _sessions.end(), [minId](const Session &session) {
                return (session == StateType::FINISHED) && (minId > session.getId());
            }), _sessions.end());
            interrupts();

            if (_sessions.empty()) {
                __DBG_printf("deleting action handler instance");
                delete this;
                return;
            }
        }
    }

}
