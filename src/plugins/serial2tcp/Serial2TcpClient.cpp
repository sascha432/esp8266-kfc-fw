/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <StreamString.h>
#include "Serial2TcpClient.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Serial2TcpClient::Serial2TcpClient(Stream &serial, uint8_t serialPort) : Serial2TcpBase(serial, serialPort), _autoConnect(false), _autoReconnect(0), _conn(nullptr), _timer(nullptr) {
}

Serial2TcpClient::~Serial2TcpClient() {
    if (_timer) {
        Scheduler.removeTimer(_timer);
        _timer = nullptr;
    }
    end();
}

void Serial2TcpClient::getStatus(PrintHtmlEntitiesString &output) {
    output.print(F(HTML_S(br)));
    if (getAutoConnect()) {
        output.print(F("Auto connect enabled, "));
    }
    if (_conn) {
        output.printf_P(PSTR("%s %s:%u"), _conn->getClient()->connected() ? PSTR("Connected to ") : PSTR("Disconnected from "), getHost().c_str(), getPort());
    }
    else {
        output.printf_P(PSTR("Idle %s:%u"), getHost().c_str(), getPort());
    }
}

void Serial2TcpClient::begin() {
    _debug_printf_P(PSTR("Serial2TcpClient::begin()\n"));
    if (getAutoConnect()) {
        _connect();
    }
}

void Serial2TcpClient::end() {
    _debug_printf_P(PSTR("Serial2TcpClient::end()\n"));

   _disconnect();
    if (_getSerialPort() == SERIAL2TCP_HARDWARE_SERIAL) {
        Serial.begin(KFC_SERIAL_RATE);
#if DEBUG
        DEBUG_HELPER_INIT();
#endif
        enable_at_mode(MySerial);
    }
}


void Serial2TcpClient::_onSerialData(uint8_t type, const uint8_t *buffer, size_t len) {
    if (_conn) {
        _debug_printf_P(PSTR("Serial2TcpClient::_onData(): type %d, length %u\n"), type, len);
        _conn->getClient()->write(reinterpret_cast<const char *>(buffer), len);
    }
}

void Serial2TcpClient::_onData(AsyncClient *client, void *data, size_t len) {
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

void Serial2TcpClient::_onDisconnect(AsyncClient *client, const __FlashStringHelper *reason) {
    _debug_printf_P(PSTR("Serial2TcpClient::_onDisconnect(): reason: %s\n"), RFPSTR(reason));
    end();

    auto time = getAutoReconnect();
    if (time) {
        Scheduler.removeTimer(_timer);
        Scheduler.addTimer(&_timer, time * 1000, false, [this](EventScheduler::TimerPtr timer) {
            _timer = nullptr; // connect removes the timer but it already has triggered
            _connect();
        });
    }
}

void Serial2TcpClient::_connect() {

    if (_timer) {
        Scheduler.removeTimer(_timer);
        _timer = nullptr;
    }

    if (_getSerialPort() == SERIAL2TCP_HARDWARE_SERIAL) {
#if DEBUG
        // DEBUG_HELPER_SILENT();
#endif
        StreamString nul;
        disable_at_mode(nul);
    }

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

    _debug_printf_P(PSTR("Serial2TcpClient::_connect(): %s:%u\n"), getHost().c_str(), getPort());
    if (!client->connect(getHost().c_str(), getPort())) {
        _disconnect();
    }
}

void Serial2TcpClient::_disconnect() {
    _debug_printf_P(PSTR("Serial2TcpClient::_disconnect()\n"));
    if (_conn) {
        _conn->close();
        delete _conn;
        _conn = nullptr;
    }
}
