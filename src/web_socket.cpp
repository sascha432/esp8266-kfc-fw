/**
 * Author: sascha_lammers@gmx.de
 */

//#include <sha512.h>
// #include <base64.hpp>
#include <Arduino_compat.h>
#include <memory>
#include <map>
#include <vector>
#include <algorithm>
#include <functional>
#include <PrintString.h>
#include <Buffer.h>
#include "session.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "progmem_data.h"
#include "web_socket.h"
#if HTTP2SERIAL
#include "./plugins/http2serial/http2serial.h"
#endif

#if DEBUG_WEB_SOCKETS
#include "debug_local_def.h"
#else
#include "debug_local_undef.h"
#endif

WsClientManager *WsClientManager::wsClientManager = nullptr;

String web_socket_getStatus() {
    PrintString out;
    return out;
}

WsClientManager *WsClientManager::getWsClientManager() {
    if (!wsClientManager) {
        wsClientManager = _debug_new WsClientManager();
    }
    return wsClientManager;
}

void WsClientManager::removeWsClient(WsClient *wsClient) {
    wsClientManager->remove(wsClient);
    if (wsClientManager->getClientCount() == 0) {
        delete wsClientManager;
        wsClientManager = nullptr;
    }
}

int WsClientManager::getWsClientCount(bool isAuthenticated) {
    if (!wsClientManager) {
        return 0;
    }
    return wsClientManager->getClientCount(isAuthenticated);
}

WsClient *WsClientManager::getWsClient(AsyncWebSocketClient *socket) {
    if (!wsClientManager) {
        return nullptr;
    }
    return wsClientManager->getClient(socket);
}



WsClientManager::WsClientManager() {
}

WsClientManager::~WsClientManager() {
    for(auto &client: _list) {
        setClientAuthenticated(client.wsClient, false);
        delete client.wsClient;
    }
    _list.clear();
#if DEBUG
    if (_authenticated.size() != 0) {
        debug_println(F("WsClientManager::~WsClientManager() manager still has authenticated clients attached"));
        panic();
    }
#endif
}

#if DEBUG
void WsClientManager::_displayStats() {
#if DEBUG_WEB_SOCKETS

    debug_printf_P(PSTR("WsClientManager::_displayStats() Clients connected %d, authenticated %d\n"), getClientCount(false), getClientCount(true));
#if HTTP2SERIAL
    debug_printf_P(PSTR("WsClientManager::_displayStats() Http2Serial enabled, object %p\n"), Http2Serial::_instance);
#endif

#endif
}
#endif


void WsClientManager::add(WsClient *wsClient, AsyncWebSocketClient *socket) {
    _list.push_back({ wsClient, socket });
#if DEBUG
    _displayStats();
#endif
}

void WsClientManager::remove(WsClient *wsClient) {
    _list.erase(std::remove_if(_list.begin(), _list.end(), [wsClient](const WsClientPair &pair) {
        return (pair.wsClient == wsClient);
    }));
    if (wsClient->isAuthenticated()) {
        setClientAuthenticated(wsClient, false);
    }
    delete wsClient;
#if DEBUG
    _displayStats();
#endif
}

void WsClientManager::remove(AsyncWebSocketClient *socket) {
    _list.erase(std::remove_if(_list.begin(), _list.end(), [&socket, this](const WsClientPair &pair) {
        if (pair.socket == socket) {
            if (pair.wsClient->isAuthenticated()) {
                this->setClientAuthenticated(pair.wsClient, false);
            }
            delete pair.wsClient;
            return true;
        } else {
            return false;
        }
    }), _list.end());
#if DEBUG
    _displayStats();
#endif
}

WsClient *WsClientManager::getClient(AsyncWebSocketClient *socket) {
    for(const auto &pair: _list) {
        if (pair.socket == socket) {
            return pair.wsClient;
        }
    }
    return nullptr;
}

int WsClientManager::getClientCount(bool isAuthenticated) {
    if (isAuthenticated) {
#if DEBUG
        size_t count = 0;
        for(const auto &pair: _list) {
            if (pair.wsClient->isAuthenticated()) {
                count++;
            }
        }
        if (count != _authenticated.size()) {
            debug_printf_P(PSTR("WsClientManager::getClientCount(%d): Authenticated count out of sync\n"), isAuthenticated);
        }
#endif
        return _authenticated.size();
    }
    return _list.size();
}

WsClientManagerVector &WsClientManager::getClients() {
    return _list;
}


void WsClientManager::setClientAuthenticated(WsClient *wsClient, bool isAuthenticated) {

    // debug_printf_P(PSTR("WsClientManager::setClientAuthenticated(%p, %d)\n"), wsClient, isAuthenticated);

    auto it = std::find_if(_authenticated.begin(), _authenticated.end(), [wsClient](WsClient *client) {
        return client == wsClient;
    });
    if (isAuthenticated) {
        if (it == _authenticated.end()) {
            _authenticated.push_back(wsClient);
            if (_authenticated.size() == 1) {
                wsClient->onStart();
            }
        }
#if DEBUG
        else {
            debug_println(F("WsClientManager::setClientAuthenticated() client already exists in authentication list"));
        }
#endif
    } else {
        if (it != _authenticated.end()) {
            _authenticated.erase(it);
        }
        if (_authenticated.size() == 0) {
            wsClient->onEnd();
        }
#if DEBUG
        else {
            debug_println(F("client not found in authenticated list"));
        }
#endif
    }
// #if DEBUG
//     _displayStats();
// #endif
}

/**
 * Example of the callback function
 *
 * It tries to find a match for the socket and if the maanger returns a
 * nullptr, iot creates a new object.
 */
// WsClient *WsClient::getInstance(AsyncWebSocketClient *socket) {

//     WsClient *wsClient = WsClientManager::getWsClientManager()->getWsClient(socket);
//     if (!wsClient) {
//         wsClient = _debug_new WsClient(socket);
//     }
//     return wsClient;
// }

#if DEBUG
void WsClient::_displayData(WsClient *wsClient, AwsFrameInfo *info, uint8_t *data, size_t len) {

    AsyncWebSocketClient *client = wsClient->getClient();
    AsyncWebSocket *server = client->server();

    debug_printf_P(PSTR(WS_PREFIX "WS_EVT_DATA %s[%u], wsClient %p\n"), WS_PREFIX_ARGS, info->opcode == WS_TEXT ? F("Text") : F("Binary"), (unsigned int)info->len, wsClient);
    if (info->final && info->index == 0 && info->len == len) {
        String msg;
        if (info->opcode == WS_TEXT) {
            for (size_t i = 0; i < info->len; i++) {
                msg += (char)data[i];
            }
        } else {
            char buff[4];
            for (size_t i = 0; i < info->len; i++) {
                snprintf(buff, sizeof(buff), PSTR("%02x "), (uint8_t)data[i]);
                msg += buff;
            }
        }
        debug_printf_P(PSTR(WS_PREFIX "%s-message[%u] %s\n"), WS_PREFIX_ARGS, (info->opcode == WS_TEXT) ? F("text") : F("binary"), (unsigned)info->len, msg.c_str());
    }
}
#endif

 WsClient::WsClient(AsyncWebSocketClient * client) {
     _authenticated = false;
     _isEncryped = false;
     _client = client;
#if WEB_SOCKET_ENCRYPTION
     _iv = nullptr;
     _salt = nullptr;
#endif
 }

 WsClient::~WsClient() {
#if WEB_SOCKET_ENCRYPTION
     if (_iv != nullptr) {
         free(_iv);
         free(_salt);
     }
#endif
 }

 void WsClient::setAuthenticated(bool authenticated) {
     _authenticated = authenticated;
 }

 bool WsClient::isAuthenticated() {
     return _authenticated;
 }

#if WEB_SOCKET_ENCRYPTION

 bool WsClient::isEncrypted() {
     return _iv != nullptr;
 }

 void WsClient::initEncryption(uint8_t * iv, uint8_t * salt) {
     _iv = iv;
     _salt = salt;
 }

#endif

 void WsClient::setClient(AsyncWebSocketClient * client) {
     _client = client;
 }

 AsyncWebSocketClient * WsClient::getClient() {
     return _client;
 }

 void WsClient::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, int type, uint8_t *data, size_t len, void *arg, WsGetInstance getInstance) {

    WsClient *wsClient = getInstance ? getInstance(client) : nullptr;
    WsClientManager *manager;

    if_debug_printf_P(PSTR("WsClient::onWsEvent(event %d, wsClient %p)\n"), onWsEvent, wsClient);
    if (!wsClient) {
        if_debug_printf_P(PSTR("WsClient::onWsEvent(): getInstance() returned nullptr\n"));
        panic();
    }

    if (type == WS_EVT_CONNECT) {
        manager = WsClientManager::getWsClientManager();
        manager->add(wsClient, client);
        client->ping();

        Logger_notice(F(WS_PREFIX "%s: Client connected"), WS_PREFIX_ARGS, client->remoteIP().toString().c_str());
        client->text(F("+REQ_AUTH"));

        wsClient->onConnect(data, len);

    } else if (type == WS_EVT_DISCONNECT) {

        Logger_notice(F(WS_PREFIX "%s: Client disconnected"), WS_PREFIX_ARGS, client->remoteIP().toString().c_str());
        wsClient->onDisconnect(data, len);
        WsClientManager::removeWsClient(wsClient);

    } else if (type == WS_EVT_ERROR) {

        if_debug_printf_P(PSTR("WsClient::onWsEvent(): WS_EVT_ERROR wsClient %p\n"), wsClient);
        Logger_notice(F(WS_PREFIX "Error(%u): %s"), WS_PREFIX_ARGS, (unsigned)*((uint16_t*)arg), (char *)data, wsClient);
        wsClient->onError(WsClient::ERROR_FROM_SERVER, data, len);

    } else if (type == WS_EVT_PONG) {

        if_debug_printf_P(PSTR("WsClient::onWsEvent(): WS_EVT_PONG wsClient %p\n"), wsClient);
        wsClient->onPong(data, len);

    } else if (type == WS_EVT_DATA) {

        #if DEBUG
            wsClient->_displayData(wsClient, (AwsFrameInfo*)arg, data, len);
        #endif

        if (wsClient->isAuthenticated()) {

            wsClient->onData((AwsFrameType)reinterpret_cast<AwsFrameInfo *>(arg)->opcode, data, len);

        } else if (len == 22 && strncmp_P((const char *)data, PSTR("+REQ_ENCRYPTION "), 16) == 0) {     // client requests an encrypted connection

#if WEB_SOCKET_ENCRYPTION
            char cipher[7];
            strncpy(cipher, (char *)data[16], sizeof(cipher))[sizeof(cipher)] = 0;
            ws_request_encryption(*wsClient, client, cipher);
#else
            client->text(F("+ENCRYPTION_NOT_SUPPORTED"));
#endif

        } else if (len > 10 && strncmp_P((const char *)data, PSTR("+SID "), 5) == 0) {          // client sent authentication

            Buffer buffer = Buffer();
            const char *dataPtr = (const char *)data;
            dataPtr += 5;
            len -= 5;
            if (!buffer.reserve(len + 1)) {
                if_debug_printf_P(PSTR("WsClient::onWsEvent(): WebSocket buffer allocation failed: %d\n"), len + 1);
                return;
            }
            char *ptr = buffer.getChar();
            strncpy(ptr, dataPtr, len)[len] = 0;
#if WEB_SOCKET_ENCRYPTION
            unsigned char buf[256];
            if ((*wsClient)->isEncrypted()) {
                buf[0] = 0;
                if (decode_base64_length((unsigned char *)ptr) < 256) {
                        buf[decode_base64((unsigned char *)ptr, buf)] = 0;
                }
                    //TODO
                    // encrypt buf to get the SID
            }
#endif
            manager = WsClientManager::getWsClientManager();
            if (verify_session_id(ptr, config.getString(_H(Config().device_name)), config.getString(_H(Config().device_pass)))) {

                wsClient->setAuthenticated(true);
                manager->setClientAuthenticated(wsClient, true);
                client->text(F("+AUTH_OK"));
                wsClient->onAuthenticated(data, len);

            } else {

                wsClient->setAuthenticated(false);
                client->text(F("+AUTH_ERROR"));
                wsClient->onError(ERROR_AUTHENTTICATION_FAILED, data, len);
                client->close();

            }
        }
    }
}

/**
 *  gets called when the first client has been authenticated.
 **/
void WsClient::onStart() {
    if_debug_println(F("WebSocket::onConnect()"));
}
/**
 * gets called once all authenticated clients have been disconnected. onStart() will be called
 * again when authenticated clients become available.
 **/
void WsClient::onEnd() {
    if_debug_println(F("WebSocket::onEnd()"));
}

void WsClient::onData(AwsFrameType type, uint8_t *data, size_t len) {
    if (type == WS_TEXT) {
        onText(data, len);
    } else if (type == WS_BINARY) {
        onBinary(data, len);
    } else {
        if_debug_printf_P(PSTR("WsClient::onData(): WebSocket Data received with type %d\n"), (int)type);
    }
}

/*
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }*/


#if WEB_SOCKET_ENCRYPTION

#define HMAC_DIGEST_SIZE  	20
#define PBKDF2_ITERATIONS	100

/*
 * Password-Based Key Derivation Function 2 (PKCS #5 v2.0)
 * Source Code From PolarSSL
 */
void PBKDF2( uint8_t *pass, uint32_t pass_len, uint8_t *salt, uint32_t salt_len,uint8_t *output, uint32_t key_len, uint32_t rounds )
{
	register int ret,j;
	register uint32_t i;
	register uint8_t md1[HMAC_DIGEST_SIZE],work[HMAC_DIGEST_SIZE];
	register size_t use_len;
	register uint8_t *out_p = output;
	register uint8_t counter[4];

	for ( i = 0 ; i < sizeof ( counter ) ; i++ )
		counter[i] = 0;
	counter[3] = 1;

	while (key_len)
	{
        uint8_t *hmac;

        Sha1.initHmac(pass,pass_len);
		Sha1.write(salt,salt_len);
		Sha1.write(counter,4);
		hmac = Sha1.resultHmac();

		for ( i = 0 ; i < HMAC_DIGEST_SIZE ; i++ )
			work[i] = md1[i] = hmac[i];

		for ( i = 1 ; i < rounds ; i++ )
		{
			Sha1.initHmac(pass, pass_len);
			Sha1.write(md1, HMAC_DIGEST_SIZE);
			hmac = Sha1.resultHmac();

			for ( j = 0 ; j < HMAC_DIGEST_SIZE ; j++ )
			{
				md1[j] = hmac[j];
				work[j] ^= md1[j];
			}
		}

		use_len = (key_len < HMAC_DIGEST_SIZE ) ? key_len : HMAC_DIGEST_SIZE;
		for ( i = 0 ; i < use_len ; i++ )
			out_p[i] = work[i];

		key_len -= use_len;
		out_p += use_len;

		for ( i = 4 ; i > 0 ; i-- )
			if ( ++counter[i-1] != 0 )
				break;
	}
}

void ws_request_encryption(WsClient *wsClient, AsyncWebSocketClient *client, String cipher) {
    uint8_t *iv, *salt;

    if (cipher != F("AES128") && cipher != F("AES192") && cipher != F("AES256") {
        client->text(F("Encryption cipher not supported"));
        client->close();
        return;
    }

    if (rng.available(16)) {
        iv = (uint8_t *)malloc(16);
        rng.rand(iv, 16);
        if (rng.available(16)) {
            salt = (uint8_t *)malloc(16);
            rng.rand(salt, 16);
            wsClient->initEncryption(iv, salt);
        } else {
            free(iv);
        }
    }
    if (wsClient->isEncrypted()) {
        unsigned char iv_str[25], salt_str[25];
        encode_base64(iv, 16, iv_str);
        encode_base64(iv, 16, salt_str);
        client->printf_P(PSTR("+REQ_ENCRYPTION %s %s %s"), cipher.c_str(), iv_str, salt_str);
    } else {
        client->text(F("+ERROR Failed to initialize encryption, try later again..."));
        client->close();
    }
}

#endif
