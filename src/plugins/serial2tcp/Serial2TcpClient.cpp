/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <StreamString.h>
#include "serial_handler.h"
#include <PrintHtmlEntitiesString.h>
#include "Serial2TcpClient.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Serial2TcpClient::Serial2TcpClient(Stream &serial, const char *hostname, const Serial2TCP::Serial2Tcp_t &config) : Serial2TcpBase(serial, config), _connection(nullptr)
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
    if (_connected()) {
        output.printf_P(PSTR("%s %s:%u"), _client().connected() ? PSTR("Connected to ") : PSTR("Disconnected from "), getHostname().c_str(), getPort());
    }
    else {
        output.printf_P(PSTR("Idle %s:%u"), getHostname().c_str(), getPort());
    }
}

bool Serial2TcpClient::isConnected() const
{
    return _connection && _connection->isConnected();
}

void Serial2TcpClient::begin()
{
    __DBGS2T("begin\n");
    Serial2TcpBase::begin();
    if (hasAutoConnect()) {
        _connect();
    }
}

void Serial2TcpClient::end()
{
    __DBGS2T("end\n");
   _disconnect();
   Serial2TcpBase::end();
}


void Serial2TcpClient::_onSerialData(Stream &client)
{
    //__DBGS2T("connected=%u conn=%p client=%p\n", _connected(), _connection, _connection ? _connection->getClient() : nullptr);
    if (_connected()) {
        auto endTime = millis() + 100;
        while (client.available() && millis() < endTime) {
            char data = client.peek();
            if (_client().write(&data, sizeof(data)) != sizeof(data)) {
                delay(1);
            }
            else {
                client.read();
            }
        }
    }
}

void Serial2TcpClient::_onData(AsyncClient *client, void *data, size_t len)
{
    _processTcpData(_connection, reinterpret_cast<const char *>(data), len);
}

void Serial2TcpClient::_onConnect(AsyncClient *client)
{
    __DBGS2T("client=%p conn=%p client=%p\n", client, _connection, _connection ? _connection->getClient() : nullptr);
}

void Serial2TcpClient::_onDisconnect(AsyncClient *client, const String &reason)
{
    __DBGS2T("client=%p reason=%s\n", client, reason.c_str());
    end();

    auto time = getAutoReconnect();
    // __LDBG_printf("auto reconnect=%u", time);
    __DBGS2T("auto reconnect=%u\n", time);
    if (time) {
        _Timer(_timer).add(Event::seconds(time), false, [this](Event::CallbackTimerPtr timer) {
            _connect();
        });
    }
}

void Serial2TcpClient::_connect()
{
    _disconnect();

    _connection = __LDBG_new(Serial2TcpConnection, __LDBG_new(AsyncClient), hasAuthentication());
    auto &client = _client();
    __DBGS2T("connect this=%p conn=%p client=%p\n", this, _connection, _connection->getClient());
    client.onConnect(handleConnect, this);
	client.onData(handleData, this);
	client.onError(handleError, this);
	client.onDisconnect(handleDisconnect, this);
	client.onTimeout(handleTimeOut, this);
    client.onAck(handleAck, this);
    client.onPoll(handlePoll, this);

    bool result;
    IPAddress address;
    if (address.fromString(getHostname())) {
        __DBGS2T("IP=%s:%u\n", address.toString().c_str(), getPort());
        result = client.connect(address, getPort());
    }
    else {
        __DBGS2T("host=%s:%u\n", getHostname().c_str(), getPort());
        result = client.connect(getHostname().c_str(), getPort());
    }
    __DBGS2T("result=%u\n", result);

    if (result) {
        _startClient();
    } else {
        _onDisconnect(&client, WiFi.isConnected() ? F("Connection failed") : F("Connection failed: WiFi offline"));
    }
    __DBGS2T("connect conn=%p client=%p\n", _connection, _connection ? _connection->getClient() : nullptr);
}

void Serial2TcpClient::_disconnect()
{
    __DBGS2T("disconnect conn=%p client=%p\n", _connection, _connection ? _connection->getClient() : nullptr);
    _timer.remove();
    if (_connection) {
        _stopClient();
        _connection->close();
        __LDBG_delete(_connection);
        _connection = nullptr;
    }
}
