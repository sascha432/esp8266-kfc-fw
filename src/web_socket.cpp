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
#include <DumpBinary.h>
#include "session.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "progmem_data.h"
#include "web_socket.h"
#if HTTP2SERIAL
#include "./plugins/http2serial/http2serial.h"
#endif

#if DEBUG_WEB_SOCKETS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

 WsClient::ClientCallbackVector_t  WsClient::_clientCallback;

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

 bool WsClient::isAuthenticated() const {
     return _authenticated;
 }

#if WEB_SOCKET_ENCRYPTION

 bool WsClient::isEncrypted() const {
     return _iv != nullptr;
 }

 void WsClient::initEncryption(uint8_t * iv, uint8_t * salt) {
     _iv = iv;
     _salt = salt;
 }

#endif

 void WsClient::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, int type, uint8_t *data, size_t len, void *arg, WsGetInstance getInstance) {

    WsClient *wsClient;
     if (client->_tempObject == nullptr) {
         // wsClient will be linked to AsyncWebSocketClient and deleted with WS_EVT_DISCONNECT when the socket gets destroyed
         client->_tempObject = wsClient = getInstance(client);
     }
     else {
         wsClient = reinterpret_cast<WsClient *>(client->_tempObject);
     }

    _debug_printf_P(PSTR("WsClient::onWsEvent(event %d, wsClient %p)\n"), type, wsClient);
    if (!wsClient) {
        __debugbreak_and_panic_printf_P(PSTR("WsClient::onWsEvent(): getInstance() returned nullptr\n"));
    }

    if (type == WS_EVT_CONNECT) {
        client->ping();

        Logger_notice(F(WS_PREFIX "%s: Client connected"), WS_PREFIX_ARGS, client->remoteIP().toString().c_str());
        client->text(F("+REQ_AUTH"));

        wsClient->onConnect(data, len);

        for(const auto &callback: _clientCallback) {
            callback(ClientCallbackTypeEnum_t::CONNECT, wsClient);
        }

    } else if (type == WS_EVT_DISCONNECT) {

        Logger_notice(F(WS_PREFIX "%s: Client disconnected"), WS_PREFIX_ARGS, client->remoteIP().toString().c_str());
        wsClient->onDisconnect(data, len);

        WsClient::invokeStartOrEndCallback(wsClient, false);

        for(const auto &callback: _clientCallback) {
            callback(ClientCallbackTypeEnum_t::DISCONNECT, wsClient);
        }

        // WS_EVT_DISCONNECT is called in the destructor of AsyncWebSocketClient
        delete wsClient;
        client->_tempObject = nullptr;

    } else if (type == WS_EVT_ERROR) {

        _debug_printf_P(PSTR("WsClient::onWsEvent(): WS_EVT_ERROR wsClient %p\n"), wsClient);

        PrintString str;
        DumpBinary dumper(str);
        dumper.dump(data, len);

        str.replace(F("\n"), F(" "));

        Logger_notice(F(WS_PREFIX "Error(%u): data='%s', length=%d"), WS_PREFIX_ARGS, *reinterpret_cast<uint16_t *>(arg), str.c_str(), len);
        wsClient->onError(WsClient::ERROR_FROM_SERVER, data, len);

    } else if (type == WS_EVT_PONG) {

        _debug_printf_P(PSTR("WsClient::onWsEvent(): WS_EVT_PONG wsClient %p\n"), wsClient);
        wsClient->onPong(data, len);

    } else if (type == WS_EVT_DATA) {

        // #if DEBUG_WEB_SOCKETS
        //     wsClient->_displayData(wsClient, (AwsFrameInfo*)arg, data, len);
        // #endif

        if (wsClient->isAuthenticated()) {

            wsClient->onData(static_cast<AwsFrameType>(reinterpret_cast<AwsFrameInfo *>(arg)->opcode), data, len);

        } else if (len == 22 && strncmp_P(reinterpret_cast<const char *>(data), PSTR("+REQ_ENCRYPTION "), 16) == 0) {     // client requests an encrypted connection

#if WEB_SOCKET_ENCRYPTION
            char cipher[7];
            strncpy(cipher, (char *)data[16], sizeof(cipher))[sizeof(cipher)] = 0;
            ws_request_encryption(*wsClient, client, cipher);
#else
            client->text(F("+ENCRYPTION_NOT_SUPPORTED"));
#endif

        } else if (len > 10 && strncmp_P((const char *)data, PSTR("+SID "), 5) == 0) {          // client sent authentication

            Buffer buffer = Buffer();
            auto dataPtr = reinterpret_cast<const char *>(data);
            dataPtr += 5;
            len -= 5;
            if (!buffer.reserve(len + 1)) {
                _debug_printf_P(PSTR("WsClient::onWsEvent(): WebSocket buffer allocation failed: %d\n"), len + 1);
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
            if (verify_session_id(ptr, config.getString(_H(Config().device_name)), config.getString(_H(Config().device_pass)))) {

                wsClient->setAuthenticated(true);
                WsClient::invokeStartOrEndCallback(wsClient, true);
                client->text(F("+AUTH_OK"));
                wsClient->onAuthenticated(data, len);

                for(const auto &callback: _clientCallback) {
                    callback(ClientCallbackTypeEnum_t::AUTHENTICATED, wsClient);
                }

            }
            else {
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
    _debug_println(F("WebSocket::onConnect()"));
}
/**
 * gets called once all authenticated clients have been disconnected. onStart() will be called
 * again when authenticated clients become available.
 **/
void WsClient::onEnd() {
    _debug_println(F("WebSocket::onEnd()"));
}

void WsClient::onData(AwsFrameType type, uint8_t *data, size_t len) {
    if (type == WS_TEXT) {
        onText(data, len);
    } else if (type == WS_BINARY) {
        onBinary(data, len);
    } else {
        _debug_printf_P(PSTR("WsClient::onData(): WebSocket Data received with type %d\n"), (int)type);
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

void WsClient::addClientCallback(ClientCallback_t callback) {
    _clientCallback.push_back(callback);
}

void WsClient::invokeStartOrEndCallback(WsClient *wsClient, bool isStart) {
    uint16_t authenticatedClients = 0;
    auto client = wsClient->getClient();
    for(auto socket: client->server()->getClients()) {
        if (socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
            authenticatedClients++;
        }
    }
    _debug_printf_P(PSTR("WsServer::invokeStartOrEndCallback(): client=%p, isStart=%u, authenticatedClients=%u\n"), wsClient, isStart, authenticatedClients);
    if (isStart) {
        if (authenticatedClients == 1) { // first client has been authenticated
            _debug_println(F("WsServer::invokeStartOrEndCallback(): invoking onStart()"));
            wsClient->onStart();
        }
    }
    else {
        if (authenticatedClients == 0) { // last client disconnected
            _debug_println(F("WsServer::invokeStartOrEndCallback(): invoking onEnd()"));
            wsClient->onEnd();
        }
    }
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer) {
    if (server == nullptr) {
        server = sender->getClient()->server();
    }
    // _debug_printf_P(PSTR("WsClient::broadcast(): sender=%p, clients=%u, message=%s\n"), sender, server->getClients().length(), buffer->get());
    buffer->lock();
    for(auto socket: server->getClients()) {
        if (socket->status() == WS_CONNECTED && socket->_tempObject && socket->_tempObject != sender && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
            socket->text(buffer);
        }
    }
    buffer->unlock();
    server->_cleanBuffers();
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const char *str, size_t length) {
    if (server == nullptr) {
        server = sender->getClient()->server();
    }
    auto buffer = server->makeBuffer(length);
    memcpy(buffer->get(), str, length);
    broadcast(server, sender, buffer);
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const __FlashStringHelper *str, size_t length) {
    if (server == nullptr) {
        server = sender->getClient()->server();
    }
    auto buffer = server->makeBuffer(length);
    memcpy_P(buffer->get(), str, length);
    broadcast(server, sender, buffer);
}

void WsClient::safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const String &message) {
    for(auto socket: server->getClients()) {
        if (client == socket && socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
            _debug_printf_P(PSTR("WsClient::safeSend(): server=%p, client=%p, message=%s\n"), server, client, message.c_str());
            client->text(message);
            return;
        }
    }
    _debug_printf_P(PSTR("WsClient::safeSend(): server=%p, client NOT found=%p, message=%s\n"), server, client, message.c_str());
}

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
