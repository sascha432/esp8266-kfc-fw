/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogTCP.h"
#include "SyslogQueue.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogTCP::SyslogTCP(SyslogParameter &&parameter, SyslogQueue &queue, const String &host, uint16_t port, bool useTLS) :
    Syslog(std::move(parameter), queue),
    _host(nullptr),
    _queueId(0),
    _port(port),
    _ack(0),
    _useTLS(useTLS)
{
    __LDBG_printf("%s:%d TLS %d", host.c_str(), _port, _useTLS);

    if (host.length()) {
        if (!_address.fromString(host)) {
            _host = strdup(host.c_str());
        }
    }

    _client.onConnect(_onPoll, this);
    _client.onPoll(_onPoll, this);
    _client.onDisconnect(_onDisconnect, this);
    _client.onAck(_onAck, this);
    _client.onError(_onError, this);
    _client.onTimeout(_onTimeout, this);
    _client.setRxTimeout(kMaxIdleSeconds);
}

SyslogTCP::~SyslogTCP()
{
    _queueId = 0;
    _client.onConnect(nullptr, nullptr);
    _client.onPoll(nullptr, nullptr);
    _client.onDisconnect(nullptr, nullptr);
    _client.onAck(nullptr, nullptr);
    _client.onError(nullptr, nullptr);
    _client.onTimeout(nullptr, nullptr);
    _client.abort();
    if (_host) {
        free(_host);
    }
}

void SyslogTCP::transmit(const SyslogQueueItem &item)
{
    auto &message = item.getMessage();
#if DEBUG_SYSLOG
    if (!String_startsWith(message, F("::transmit '"))) {
        __LDBG_printf("::transmit id=%u msg=%s%s", item.getId(), _getHeader().c_str(), message.c_str());
    }
    if (_queueId) {
        __DBG_panic("FATAL: Transmit called while sending");
    }
#endif

    _queueId = item.getId();
    _buffer = _getHeader();
    _buffer.write(message);
    _buffer.write('\n');
    _ack = _buffer.length();

    // canSend() means that the connection is established and tcp buffers have free space
    if (_client.canSend()) {
        __LDBG_print("already connected");
        __onPoll(&_client);
    }
    else {
        // check if we are disconnected
        _connect();
    }
}

void SyslogTCP::clear()
{
    __LDBG_print("clear");
    _disconnect();
    _queueId = 0;
    _buffer.clear();
    _ack = 0;
    _queue.clear();
}

bool SyslogTCP::setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port)
{
    _client.close();
    if (_host) {
        free(_host);
        _host = nullptr;
    }
    if (address.isSet()) {
        _address = address;
    }
    else {
        _host = strdup(hostname.c_str());
    }
    _port = port;
    return true;
}

String SyslogTCP::getHostname() const
{
    if (_host) {
        return _host;
    }
    if (_address.isSet()) {
        return _address.toString();
    }
    return F("N/A");
}

uint16_t SyslogTCP::getPort() const
{
    return _port;
}

bool SyslogTCP::canSend() const
{
    return (_host || _address.isSet()) && WiFi.isConnected();
}

bool SyslogTCP::isSending()
{
	 return _queueId;
}

// connect to remote host if connection is not established
void SyslogTCP::_connect()
{
    __LDBG_printf("connected=%u connecting=%u can_send=%u", _client.connected(), _client.connecting(), _client.canSend());
    if (!_client.connected() && !_client.connecting()) {
#if ASYNC_TCP_SSL_ENABLED
#   define USE_TLS              , _useTLS
#else
#   define USE_TLS
#endif
        bool result;
        if (_host) {
            result = _client.connect(_host, _port USE_TLS);
        }
        else {
            result = _client.connect(_address, _port USE_TLS);
        }
        if (!result) {
            _status(false __LDBG_IF(, PSTR("connect")));
        }
    }
}

void SyslogTCP::_disconnect()
{
    __LDBG_printf("id=%u connected=%d connecting=%d disconnecting=%d", _queueId, _client.connected(), _client.connecting(), _client.disconnecting());
    _client.close(true);
}

void SyslogTCP::_status(bool success __LDBG_IF(, const char *reason))
{
    __LDBG_printf("success=%u id=%u ack=%u reason=%s state=%s", success, _queueId, _ack, __S(reason), _queueId ? PSTR("busy") : PSTR("idle"));
    if (_queueId) {
        _queue.remove(_queueId, success);
        _queueId = 0;
        _ack = 0;
        _buffer.clear();
    }
}

void SyslogTCP::__onPoll(AsyncClient *client)
{
    if (_queueId && _buffer.length() && client->canSend()) {
        auto toSend = std::min(client->space(), _buffer.length());
        auto written = client->write(_buffer.cstr_begin(), toSend);
        __LDBG_printf("buffer=%u send=%u written=%u", _buffer.length(), toSend, written);
        if (written == toSend) {
            _buffer.removeAndShrink(0, toSend); // remove what has been written already
        }
        else {
            _status(false __LDBG_IF(, PSTR("write")));
            _disconnect();
        }
    }
}

void SyslogTCP::__onAck(AsyncClient *client, size_t len, uint32_t time)
{
    __LDBG_printf("len=%u time=%u ack=%u id=%u buffer=%u", len, time, _ack, _queueId, _buffer.length());
    if (_queueId) {
        if (len > _ack) {
            _status(false __LDBG_IF(, PSTR("ack > message")));
            _disconnect();
        }
        else {
            _ack -= len;
            if (_ack == 0 && _buffer.length() == 0) {
                // report success and remove queueId
                _status(true);
            }
        }
    }
    else {
        __LDBG_printf("ack while not sending");
        _disconnect();
    }
}

void SyslogTCP::_onDisconnect(void *arg, AsyncClient *client)
{
    // not an error, calling _queueWriteError just verfies the state
    reinterpret_cast<SyslogTCP *>(arg)->_status(false __LDBG_IF(, PSTR("disconnect")));
}

void SyslogTCP::_onError(void *arg, AsyncClient *client, int8_t error)
{
    reinterpret_cast<SyslogTCP *>(arg)->_status(false __LDBG_IF(, PSTR("error")));
}

// default ack timeout is 5s for sending
// rx timeout is set to kMaxIdleSeconds
void SyslogTCP::_onTimeout(void *arg, AsyncClient *client, uint32_t time)
{
    reinterpret_cast<SyslogTCP *>(arg)->_status(false __LDBG_IF(, PSTR("timeout")));
}

void SyslogTCP::_onAck(void *arg, AsyncClient *client, size_t len, uint32_t time)
{
    reinterpret_cast<SyslogTCP *>(arg)->__onAck(client, len, time);
}

// gets called once per second
void SyslogTCP::_onPoll(void *arg, AsyncClient *client)
{
    reinterpret_cast<SyslogTCP *>(arg)->__onPoll(client);
}
