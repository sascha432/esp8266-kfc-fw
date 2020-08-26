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
    _client(nullptr),
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
}

SyslogTCP::~SyslogTCP()
{
    _queueId = 0;
    _freeClient();
    if (_host) {
        free(_host);
    }
}

void SyslogTCP::transmit(const SyslogQueueItem &item)
{
    auto &message = item.getMessage();
#if DEBUG_SYSLOG
    if (!String_startsWith(message, F("::transmit '"))) {
        __LDBG_printf("::transmit id=%u msg=%s%s", item.getId(), _getHeader(item.getMillis()).c_str(), message.c_str());
    }
#endif
    __LDBG_assert(_ack == 0);
    __LDBG_assert(_queueId == 0);
#if DEBUG_SYSLOG
    if (_ack) {
        __LDBG_printf("ack=%u id=%u buffer=%u", _ack, _queueId, _buffer.length());
        clear();
    }
#endif

    _queueId = item.getId();
    _buffer = _getHeader(item.getMillis());
    _buffer.write(message);
    _buffer.write('\n');
    _ack = _buffer.length();

    _connect();
}

void SyslogTCP::clear()
{
    __LDBG_print("clear");
    if (_client) {
        _disconnect();
    }
    _queueId = 0;
    _buffer.clear();
    _ack = 0;
    _queue.clear();
}

bool SyslogTCP::setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port)
{
    if (_host) {
        free(_host);
        _host = nullptr;
    }
    if (address.isSet()) {
        _address = address;
    }
    else  if (hostname.length()) {
        _host = strdup(hostname.c_str());
    }
    _port = port;
    if (_client) {
        __LDBG_printf("client=%p queue=%u state=%s", _client, _queueId, _client->stateToString());
        if (_client->connected()) {
            _disconnect();
        }
        if (_queueId) {
            _connect();
        }
    }
    else {
        __LDBG_printf("client=%p", _client);
    }
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
    return emptyString;
}

uint16_t SyslogTCP::getPort() const
{
    return _port;
}

bool SyslogTCP::canSend() const
{
    return (_port != 0) && (_host || _address.isSet()) && WiFi.isConnected();
}

bool SyslogTCP::isSending()
{
	 return _queueId;
}

void SyslogTCP::__onDisconnect()
{
    __LDBG_assert_panic(_client != nullptr, "_client==nullptr");
    bool hasNoQueue = (_queueId == 0);
    __LDBG_printf("has_no_queue=%u state=%s state=%u", hasNoQueue, _client->stateToString(), _client->state());
    if (_client->state() == 0 && hasNoQueue) {
        _freeClient();
    }
    else {
        _status(false __LDBG_IF(, PSTR("disconnect")));
    }
}

void SyslogTCP::__onTimeout(uint32_t time)
{
    __LDBG_assert_panic(_client != nullptr, "_client==nullptr");
    bool hasNoQueue = (_queueId == 0); // store queue state
    __LDBG_printf("timeout=%u has_no_queue=%u connected=%u state=%s", time, hasNoQueue, _client->connected(), _client->stateToString());
    _status(false __LDBG_IF(, PSTR("timeout")));
    if (hasNoQueue) {
        _freeClient(); // disconnect and free client
    }
}

void SyslogTCP::__onPoll()
{
    __sendQueue();
}

// connect to remote host if connection is not established
void SyslogTCP::_connect()
{
    if (!_client) {
        _allocClient();
    }
    __LDBG_printf("connected=%u port=%u connecting=%u can_send=%u state=%s", _client->connected(), _port, _client->connecting(), _client->canSend(), _client->stateToString());
    if (_port == 0) {
        return;
    }
    if (!_client->connected() && !_client->connecting()) {
#if ASYNC_TCP_SSL_ENABLED
#   define USE_TLS              , _useTLS
#else
#   define USE_TLS
#endif
        bool result;
        if (_host) {
            result = _client->connect(_host, _port USE_TLS);
        }
        else {
            result = _client->connect(_address, _port USE_TLS);
        }
        if (!result) {
            _status(false __LDBG_IF(, PSTR("connect")));
        }
    }
}

void SyslogTCP::_disconnect()
{
    __LDBG_assert_panic(_client != nullptr, "_client==nullptr");
    __LDBG_printf("disconnect id=%u connected=%d state=%s", _queueId, _client->connected(), _client->stateToString());
    _client->close(true);
}

void SyslogTCP::_allocClient()
{
    __LDBG_printf("client=%p port=%u", _client, _port);
    if (!_client) {
        _client = __LDBG_new(AsyncClient);
        _client->onConnect(_onPoll, this);
        _client->onPoll(_onPoll, this);
        _client->onDisconnect(_onDisconnect, this);
        _client->onAck(_onAck, this);
        _client->onError(_onError, this);
        _client->onTimeout(_onTimeout, this);
        _client->setRxTimeout(kMaxIdleSeconds);
    }
}

void SyslogTCP::_freeClient()
{
    __LDBG_printf("client=%p", _client);
    if (_client) {
        __LDBG_assert(_queueId == 0);
        _client->onConnect(nullptr);
        _client->onPoll(nullptr);
        _client->onDisconnect(nullptr);
        _client->onAck(nullptr);
        _client->onError(nullptr);
        _client->onTimeout(nullptr);
        // _client->abort();
        __LDBG_printf("freeable=%u state=%s", _client->freeable(), _client->stateToString());
        __LDBG_delete(_client);
        _client = nullptr;
    }
}


void SyslogTCP::_status(bool success __LDBG_IF(, const char *reason))
{
    __LDBG_assert_panic(_client != nullptr, "_client==nullptr");
    __LDBG_printf("success=%u id=%u ack=%u reason=%s queue=%s state=%s error=%s", success, _queueId, _ack, __S(reason), _queueId ? PSTR("busy") : PSTR("idle"), _client->stateToString(), success ? PSTR("null") : _client->errorToString(_client->getCloseError()));
    if (_queueId) {
        if (!success) {
            __DBG_printf("failed to send syslog message %u", _queueId);
        }
        _queue.remove(_queueId, success);
        _queueId = 0;
        _ack = 0;
        _buffer.clear();
    }
}

void SyslogTCP::__sendQueue()
{
    __LDBG_assert_panic(_client != nullptr, "_client==nullptr");
    if (_queueId && _buffer.length() && _client->connected() && _client->canSend()) {
        auto toSend = std::min(_client->space(), _buffer.length());
        auto written = _client->write(_buffer.cstr_begin(), toSend);
        __LDBG_printf("buffer=%u send=%u written=%u remove=%u", _buffer.length(), toSend, written, (written == toSend));
        if (written == toSend) {
            // remove what has been written to the socket
            if (written == _buffer.length()) {
                _buffer.clear();
            }
            else {
                _buffer.removeAndShrink(0, written);
            }
        }
        else {
            _status(false __LDBG_IF(, PSTR("write")));
            _disconnect();
        }
    }
}

void SyslogTCP::__onAck(size_t len, uint32_t time)
{
    __LDBG_assert_panic(_client != nullptr, "_client==nullptr");
    __LDBG_printf("len=%u time=%u ack=%u id=%u buffer=%u state=%s", len, time, _ack, _queueId, _buffer.length(), _client->stateToString());
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
             else if (_buffer.length()) {
                 // try to send more data
                 __sendQueue();
             }
        }
    }
    else {
        __LDBG_printf("ack while not sending");
        _disconnect();
    }
}

void SyslogTCP::__onError(int8_t error)
{
    __LDBG_assert_panic(_client != nullptr, "_client==nullptr");
    __LDBG_printf("err=%d error=%s state=%s", error, _client->errorToString(error), _client->stateToString());
    _status(false __LDBG_IF(, PSTR("error")));
}

void SyslogTCP::_onDisconnect(void *arg, AsyncClient *client)
{
    __LDBG_assert(reinterpret_cast<SyslogTCP *>(arg)->_client == client);
    reinterpret_cast<SyslogTCP *>(arg)->__onDisconnect();
}

void SyslogTCP::_onError(void *arg, AsyncClient *client, int8_t error)
{
    __LDBG_assert(reinterpret_cast<SyslogTCP *>(arg)->_client == client);
    reinterpret_cast<SyslogTCP *>(arg)->__onError(error);
}

// default ack timeout is 5000ms for sending
// rx timeout is set to kMaxIdleSeconds
void SyslogTCP::_onTimeout(void *arg, AsyncClient *client, uint32_t time)
{
    __LDBG_assert(reinterpret_cast<SyslogTCP *>(arg)->_client == client);
    reinterpret_cast<SyslogTCP *>(arg)->__onTimeout(time);
}

void SyslogTCP::_onAck(void *arg, AsyncClient *client, size_t len, uint32_t time)
{
    __LDBG_assert(reinterpret_cast<SyslogTCP *>(arg)->_client == client);
    reinterpret_cast<SyslogTCP *>(arg)->__onAck(len, time);
}

// gets called once per second or every 125ms while connected
void SyslogTCP::_onPoll(void *arg, AsyncClient *client)
{
    __LDBG_assert(reinterpret_cast<SyslogTCP *>(arg)->_client == client);
    reinterpret_cast<SyslogTCP *>(arg)->__onPoll();
}
