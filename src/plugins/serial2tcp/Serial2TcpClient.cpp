/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <StreamString.h>
#include <PrintHtmlEntitiesString.h>
#include "Serial2TcpClient.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Serial2TcpClient::Serial2TcpClient(Stream &serial, const char *hostname, const Serial2TCP::Serial2Tcp_t &config) : Serial2TcpBase(serial, config), _conn(nullptr)
{
    setHostname(hostname);
}

Serial2TcpClient::~Serial2TcpClient()
{
    _timer.remove();
    end();
}

void Serial2TcpClient::getStatus(Print &output)
{
    output.print(F(HTML_S(br)));
    if (hasAutoConnect()) {
        output.print(F("Auto connect enabled, "));
    }
    if (_conn) {
        output.printf_P(PSTR("%s %s:%u"), _conn->getClient()->connected() ? PSTR("Connected to ") : PSTR("Disconnected from "), getHostname().c_str(), getPort());
    }
    else {
        output.printf_P(PSTR("Idle %s:%u"), getHostname().c_str(), getPort());
    }
}

void Serial2TcpClient::begin()
{
    _debug_println();
    Serial2TcpBase::begin();
    if (hasAutoConnect()) {
        _connect();
    }
}

void Serial2TcpClient::end()
{
    _debug_println();

   _disconnect();
   Serial2TcpBase::end();
}


void Serial2TcpClient::_onSerialData(uint8_t type, const uint8_t *buffer, size_t len)
{
    if (_conn) {
        _debug_printf_P(PSTR("Serial2TcpClient::_onData(): type %d, length %u\n"), type, len);
        _conn->getClient()->write(reinterpret_cast<const char *>(buffer), len);
    }
}

void Serial2TcpClient::_onData(AsyncClient *client, void *data, size_t len)
{
    // _debug_printf_P(PSTR("Serial2TcpClient::_onData(): length %u\n"), len);
#if 0
    for(size_t i = 0; i < len; i++) {
        char byte = reinterpret_cast<char *>(data)[i];
        _getSerial().printf("%02x ", byte);
    }
    _getSerial().println();
#endif

    if (_conn) {
        _processData(_conn, reinterpret_cast<const char *>(data), len);
    }
}

// size_t Serial2TcpClient::_serialWrite(Serial2TcpConnection *conn, const char *data, size_t len)  {
//     size_t written = Serial2TcpBase::_serialWrite(conn, data, len);
//     // no echo
//     // if (_conn) {
//     //     _conn->getClient()->write(data, len); // TODO add buffering / implement send() method to connection
//     // }
//     return written;
// }

void Serial2TcpClient::_onDisconnect(AsyncClient *client, const __FlashStringHelper *reason)
{
    _debug_printf_P(PSTR("Serial2TcpClient::_onDisconnect(): reason: %s\n"), RFPSTR(reason));
    end();

    auto time = getAutoReconnect();
    if (time) {
        _timer.add(time * 1000UL, false, [this](EventScheduler::TimerPtr timer) {
            _connect();
        });
    }
}

void Serial2TcpClient::_connect()
{
    _timer.remove();

    _disconnect();
    _conn = new Serial2TcpConnection(new AsyncClient(), hasAuthentication());
    auto client = _conn->getClient();
    client->onConnect(&handleConnect, this);
	client->onData(&handleData, this);
	client->onError(&handleError, this);
	client->onDisconnect(&handleDisconnect, this);
	client->onTimeout(&handleTimeOut, this);
    client->onAck(&handleAck, this);
    client->onPoll(&handlePoll, this);

    _debug_printf_P(PSTR("Serial2TcpClient::_connect(): %s:%u\n"), getHostname().c_str(), getPort());
    if (!client->connect(getHostname().c_str(), getPort())) {
        _disconnect();
    }
}

void Serial2TcpClient::_disconnect()
{
    _debug_printf_P(PSTR("Serial2TcpClient::_disconnect()\n"));
    if (_conn) {
        _conn->close();
        delete _conn;
        _conn = nullptr;
    }
}
