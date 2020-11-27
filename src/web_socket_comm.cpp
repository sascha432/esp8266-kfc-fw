/**
 * Author: sascha_lammers@gmx.de
 */

#if WEBSERVER_SUPPORT

#include "web_server.h"

#if WEBSERVER_WS_COMM

// EXPERIMENTAL
// web socket replacing REST calls
// TODO: check if a permanently connected websocket uses less resources than frequent REST api calls

// var auth = false;
// var url = location.protocol.replace(/^http/g, 'ws') + '//' + location.hostname;
// if (location.port) {
// 	url = url + ':' + location.port;
// }
// url = url + '/mws';
// var socket = new WebSocket(url);
// socket.onmessage = function(e) {
// 	console.log(e)
// 	if (e.type == 'message') {
// 		if (e.data == '+REQ_AUTH') {
// 			var sid = Cookies.get('SID');
// 			auth = false;
// 			socket.send('+SID ' + sid);
// 		}
// 		else if (e.data == '+AUTH_OK') {
// 			auth = true;
// 			socket.send('+STIME');
// 		}

// 	}
// }
// socket.onopen = function(e) {
// 	console.log(e)
// }
// socket.onclose = function(e) {
// 	console.log(e)
// 	auth = false;
// }
// socket.onerror = function(e) {
// 	console.log(e)
// 	auth = false;
// 	socket.close();
// }


#include <Arduino_compat.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
// #include <StreamString.h>
// #include <BufferStream.h>
// #include <JsonConfigReader.h>
//#include <ESPAsyncWebServer.h>
#include <FileOpenMode.h>
// #include "build.h"
// #include "rest_api.h"
// #include "async_web_response.h"
// #include "async_web_handler.h"
#include "kfc_fw_config.h"
// #include "blink_led_timer.h"
// #include "fs_mapping.h"
// #include "session.h"
#include "../include/templates.h"
 #include "web_socket.h"
// #include "WebUISocket.h"
#include "WebUIAlerts.h"
// #if STK500V1
// #include "../src/plugins/stk500v1/STK500v1Programmer.h"
// #endif
// #include "./plugins/mdns/mdns_sd.h"
// #if IOT_REMOTE_CONTROL
// #include "./plugins/remote/remote.h"
// #endif

#if DEBUG_WEB_SERVER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif
#include <debug_helper_disable_mem.h>

using KFCConfigurationClasses::System;

static inline void __WebServerPlugin_wsEventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WebServerPlugin::getInstance().wsEventHandler(server, client, type, arg, data, len);
}

class WsMainSocket : public WsClient {
public:
    using WsClient::WsClient;

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onAuthenticated(uint8_t *data, size_t len) override {
        __DBG_printf("onAuthenticated=%s len=%d", printable_string(data, len, 32).c_str(), len);
    }

    virtual void onConnect(uint8_t *data, size_t len) {
        __DBG_printf("onConnect=%s len=%d", printable_string(data, len, 32).c_str(), len);
    }

    virtual void onDisconnect(uint8_t *data, size_t len) {
        __DBG_printf("onDisconnect=%s len=%d", printable_string(data, len, 32).c_str(), len);
    }

    virtual void onText(uint8_t *data, size_t len) {
        __DBG_printf("onText=%s len=%d", printable_string(data, len, 32).c_str(), len);

        String str;
        str.concat(reinterpret_cast<char *>(data), len);
        auto pos = str.indexOf('=') + 1;

        if (String_equals(str, F("+STIME"))) {
            PrintHtmlEntitiesString str = F("+STIME=");
            WebTemplate::printSystemTime(time(nullptr), str);
            getClient()->text(str);
        }
#if WEBUI_ALERTS_ENABLED
        else if (String_startsWith(str, F("+WUIPA="))) {
            size_t size = 0;
            WebAlerts::IdType pollId = atoi(str.c_str() + pos);
            if (pollId <= WebAlerts::FileStorage::getMaxAlertId()) {
                auto file = KFCFS.open(FSPGM(alerts_storage_filename), fs::FileOpenMode::read);
                if (file) {
                    size = file.size();
                    file.close();
                }
            }
            auto str = PrintString(F("+WUIPA=%u"), size);
            if (size) {
                str.printf_P(PSTR(",/alerts?poll_id=%u"), pollId);
            }
            getClient()->text(str);
        }
        else if (String_startsWith(str, F("+WUIDA="))) {
            str.remove(0, pos);
            if (str.length()) {
                WebAlerts::Alert::dismissAlerts(str);
            }
            getClient()->text(F("+WUIDA=OK"));
        }
#endif
        else {
            getClient()->text(F("+ERR"));
        }
    }

    virtual void onBinary(uint8_t *data, size_t len) {
        __DBG_printf("onBinary=%s len=%d", printable_string(data, len, 32).c_str(), len);
    }

    virtual void onPong(uint8_t *data, size_t len) {
        __DBG_printf("onPong=%s len=%d", printable_string(data, len, 32).c_str(), len);
    }

    virtual void onStart() override {
        __DBG_printf("onStart");
    }

    virtual void onEnd() override {
        __DBG_printf("onEnd");
    }

    virtual void onError(WsErrorType type, uint8_t *data, size_t len) override {
        __DBG_printf("type=%d data=%s len=%d", type, printable_string(data, len, 32).c_str(), len);
    }
};

WsClient *WsMainSocket::getInstance(AsyncWebSocketClient *socket)
{
    return __LDBG_new(WsMainSocket, socket);
}

void WebServerPlugin::wsEventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WsClient::onWsEvent(server, client, type, data, len, arg, WsMainSocket::getInstance);
}

void WebServerPlugin::wsSetup()
{
    auto ws = __LDBG_new(WsClientAsyncWebSocket, F("/mws"), &wsMainSocket);
    ws->onEvent(__WebServerPlugin_wsEventHandler);
    addHandler(ws);
    __DBG_printf("Main web socket running on port %u", System::WebServer::getConfig().getPort());
}

#endif

#endif
