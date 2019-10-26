/**
 * Author: sascha_lammers@gmx.de
 */

#if SERIAL2TCP

#include "Serial2TcpConnection.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Serial2TcpConnection::Serial2TcpConnection(AsyncClient *client, bool isAuthenticated) : _isAuthenticated(isAuthenticated), _client(client) {
}

Serial2TcpConnection::~Serial2TcpConnection() {
    _debug_printf_P(PSTR("Serial2TcpConnection::~Serial2TcpConnection() client %p\n"), _client);
    close();
}

void Serial2TcpConnection::close() {
    _debug_printf_P(PSTR("Serial2TcpConnection::close() client %p\n"), _client);
    if (_client) {
        _client->abort();
        delete _client;
        _client = nullptr;
    }
}

bool Serial2TcpConnection::isAuthenticated() const {
    return _isAuthenticated;
}

void Serial2TcpConnection::setClient(AsyncClient *client) {
    _client = client;
}

AsyncClient *Serial2TcpConnection::getClient() const {
    return _client;
}

Buffer &Serial2TcpConnection::getTxBuffer() {
    return _txBuffer;
}

Buffer &Serial2TcpConnection::getRxBuffer() {
    return _rxBuffer;
}

Buffer &Serial2TcpConnection::getNvtBuffer() {
    return _nvt;
}

#endif
