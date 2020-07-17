/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <StreamString.h>
#include <PrintHtmlEntitiesString.h>
#include "Serial2TcpServer.h"
#include "serial_handler.h"
#include "misc.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


Serial2TcpServer::Serial2TcpServer(Stream &serial, const Serial2TCP::Serial2Tcp_t &config) : Serial2TcpBase(serial, config), _server(nullptr)
{
}

Serial2TcpServer::~Serial2TcpServer()
{
    end();
}

void Serial2TcpServer::getStatus(Print &output)
{
    if (hasAuthentication()) {
        output.println(F(", Authentication enabled"));
    }
    output.printf_P(PSTR(HTML_S(br) "%u client(s) connected"), _connections.size());
}

void Serial2TcpServer::begin()
{
    Serial2TcpBase::begin();

    end();
    _server = new AsyncServer(getPort());
    _server->onClient(&Serial2TcpServer::handleNewClient, this);

#if ASYNC_TCP_SSL_ENABLED
    if (config._H_GET(Config().flags).serial2TCPMode == SERIAL2TCP_MODE_SECURE_SERVER) {
        //TODO
        // _server->beginSecure(_Config.getServerCert(F("serial2tcp")).c_str(), _Config.getServerKey(F("serial2tcp")).c_str(), _Config.getCertPassphrase());
    } else
#endif
    {
        _server->begin();
    }

}

void Serial2TcpServer::end()
{
    if (_server) {
        for(auto &&conn: _connections) {
            _onDisconnect(conn->getClient(), F("server shutdown"));
        }
        delete _server;
        _server = nullptr;
    }
    _onEnd();
    Serial2TcpBase::end();
}

void Serial2TcpServer::handleNewClient(void *arg, AsyncClient *client)
{
    reinterpret_cast<Serial2TcpServer *>(arg)->_handleNewClient(client);
}

void Serial2TcpServer::_onSerialData(uint8_t type, const uint8_t *buffer, size_t len)
{
    if (!_connections.empty()) {
        // _debug_printf_P(PSTR("Serial2TcpServer::_onData(): type %d, length %u\n"), type, len);
        for(auto &&conn: _connections) {
            conn->getClient()->write(reinterpret_cast<const char *>(buffer), len);
        }
    }
}

void Serial2TcpServer::_onData(AsyncClient *client, void *data, size_t len)
{
    // _debug_printf_P(PSTR("Serial2TcpServer::_onData(): length %u\n"), len);
    if (_debugOutput) {
        for(size_t i = 0; i < len; i++) {
            char byte = reinterpret_cast<char *>(data)[i];
            _serialPrintf_P(nullptr, PSTR("%02x "), byte);
            // _getSerial().printf_P(PSTR("%02x "), byte);
        }
        // _getSerial().println();
        _serialWrite(nullptr, "\n", 1);
    }

    auto conn = _getConn(client);
    if (conn) {
        _processData(conn, reinterpret_cast<const char *>(data), len);
    }
}

void Serial2TcpServer::_onDisconnect(AsyncClient *client, const String &reason)
{
    _debug_printf_P(PSTR("client=%p reason=%s\n"), client, reason.c_str());
    _removeClient(client);
}


String Serial2TcpServer::_getClientInfo(Serial2TcpConnection &conn) const
{
    String output;
    if (hasAuthentication()) {
        output += getUsername() + '@';
    }
    output += conn.getClient()->remoteIP().toString().c_str();
    return output;
}

size_t Serial2TcpServer::_serialWrite(Serial2TcpConnection *conn, const char *data, size_t len)
{
    size_t written = Serial2TcpBase::_serialWrite(conn, data, len);
    for(auto &&conn2: _connections) {
        if (conn != conn2.get()) {
            conn2->getClient()->write(data, len); // TODO add buffering / implement send() method to connection
        }
    }
    return written;
}

Serial2TcpConnection *Serial2TcpServer::_getConn(AsyncClient *client)
{
    auto iterator = std::find_if(_connections.begin(), _connections.end(), [&client](const Serial2TcpConnectionPtr &conn) {
        return conn->getClient() == client;
    });
    if (iterator == _connections.end()) {
        return nullptr;
    }
    return iterator->get();
}

void Serial2TcpServer::_handleNewClient(AsyncClient *client)
{
    _debug_printf_P(PSTR("%p: client connected from %s\n"), client, client->remoteIP().toString().c_str());

// #if DEBUG
//     if (_getSerialPort() == SERIAL2TCP_HARDWARE_SERIAL) { // disable any debug output
//         DEBUG_HELPER_SILENT();
//     }
// #endif

    auto &conn = _addClient(client);
	Logger_notice(F("[%s] Client has been connected to server"), _getClientInfo(conn).c_str());
}


Serial2TcpConnection &Serial2TcpServer::_addClient(AsyncClient *client)
{
    _debug_printf_P(PSTR("Serial2TcpServer::_addClient(%p) IP address %s\n"), client, client->remoteIP().toString().c_str());
    _connections.emplace_back(new Serial2TcpConnection(client, false));
    if (_connections.size() == 1) {
        _onStart();
    }
    _debug_printf_P(PSTR("client %p conn.client %p\n"), client, _connections.back()->getClient());
	client->onData(&handleData, this);
	client->onError(&handleError, this);
	client->onDisconnect(&handleDisconnect, this);
	client->onTimeout(&handleTimeOut, this);
    client->onAck(&handleAck, this);
    client->onPoll(&handlePoll, this);

    return *_connections.back();
}

void Serial2TcpServer::_removeClient(AsyncClient *client)
{
    _debug_printf_P(PSTR("Serial2TcpServer::_removeClient(%p) IP address %s\n"), client, client->remoteIP().toString().c_str());

    client->abort();
    _connections.erase(std::remove_if(_connections.begin(), _connections.end(), [&client](const Serial2TcpConnectionPtr &conn) {
        return conn->getClient() == client;
    }), _connections.end());

    if (_connections.size() == 0) {
        _onEnd();
    }
}

void Serial2TcpServer::_onStart()
{
    _debug_println();
//     if (_getSerialPort() == SERIAL2TCP_HARDWARE_SERIAL) {
// #if DEBUG
//         DEBUG_HELPER_SILENT();
// #endif
//         StreamString nul;
//         disable_at_mode(nul);
//     }
}

void Serial2TcpServer::_onEnd()
{
    _debug_println();
//     if (_getSerialPort() == SERIAL2TCP_HARDWARE_SERIAL) {
//         Serial.begin(KFC_SERIAL_RATE);
// #if DEBUG
//         DEBUG_HELPER_INIT();
// #endif
//         enable_at_mode(Serial);
//     }
}
