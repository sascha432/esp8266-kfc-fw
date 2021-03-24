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

        Session Handler::initSession(AsyncWebServerRequest *request, const String &url, const String &title)
        {
            auto &plugin = Plugin::getInstance();
            if (!plugin.isAuthenticated(request)) {
                request->send(403);
                return Session(StateType::UNAUTHORIZED);
            }
            return Session(StateType::UNAUTHORIZED);
        }


    }

}


// Session Handler::initSession(AsyncWebServerRequest *request, const String &url, const String &title)
// {
//     auto &plugin = Plugin::getInstance();

//     // if (!plugin.isAuthenticated(request)) {
//     //     request->send(403);
//     //     return ActionId(ActionStateType::UNAUTHORIZED);
//     // }

//     //     uint32_t now = time(nullptr);
//     //     uint32_t actionId = request->arg(F("action")).toInt();
//     //     bool run = false;
//     //     if (actionId == 0) {
//     //         auto url = PrintString(F("%s?action=%u"), PSTR("/mqtt-publish-ad.html"), now - 5);
//     //         httpHeaders.addNoCache(true);
//     //         httpHeaders.add<HttpLocationHeader>(url);
//     //         response = request->beginResponse(302);
//     //         httpHeaders.setResponseHeaders(response);
//     //         request->send(response);
//     //         return;
//     //     }
//     //     else {
//     //         String lastActionId;
//     //         HttpCookieHeader::parseCookie(request, F("last_action_id"), lastActionId);
//     //         auto cookieActionId = static_cast<uint32_t>(lastActionId.toInt());
//     //         if ((cookieActionId != actionId) && (actionId <= now)) {
//     //             httpHeaders.add<HttpCookieHeader>(F("last_action_id"), String(actionId));
//     //             __DBG_printf("action_id=%u cookie_action_id=%u now=%u", actionId, cookieActionId, now);
//     //             run = true;
//     //         }
//     //         else {
//     //             __DBG_printf("cookie or expired: action_id=%u cookie_action_id=%u now=%u cookie_len=%u", actionId, cookieActionId, now, lastActionId.length());
//     //         }
//     //     }



// }
