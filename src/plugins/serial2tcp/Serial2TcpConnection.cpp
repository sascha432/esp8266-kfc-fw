/**
 * Author: sascha_lammers@gmx.de
 */

#include "Serial2TcpConnection.h"
#include "Serial2TcpBase.h"

#if DEBUG_SERIAL2TCP
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Serial2TcpConnection::Serial2TcpConnection(AsyncClient *client, bool isAuthenticated) : _client(client), _isAuthenticated(isAuthenticated)
{
    // __DBGS2T("CONST this=%p client=%p\n", this, _client);
}

Serial2TcpConnection::~Serial2TcpConnection()
{
    // __DBGS2T("DEST this=%p client=%p\n", this, _client);
    close();
}

void Serial2TcpConnection::close()
{
    // __DBGS2T("CLOSE this=%p client=%p\n", this, _client);
    if (_client) {
        _client->abort();
        __LDBG_delete(_client);
        _client = nullptr;
    }
}

bool Serial2TcpConnection::isAuthenticated() const
{
    return _isAuthenticated;
}

bool Serial2TcpConnection::isConnected() const
{
    return _client && _client->connected();
}

// void Serial2TcpConnection::setClient(AsyncClient *client)
// {
//     _client = client;
//     __DBGS2T("SETCLIENT=%p this=%p\n", _client, this);
// }

AsyncClient *Serial2TcpConnection::getClient() const
{
    // __DBGS2T("GETCLIENT=%p this=%p\n", _client, this);
    return _client;
}

Buffer &Serial2TcpConnection::getTxBuffer()
{
    return _txBuffer;
}

Buffer &Serial2TcpConnection::getRxBuffer()
{
    return _rxBuffer;
}

Buffer &Serial2TcpConnection::getNvtBuffer()
{
    return _nvt;
}
