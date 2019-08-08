/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogTCP.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogTCP::SyslogTCP(SyslogParameter& parameter, const String &host, uint16_t port, bool useTLS) : Syslog(parameter) {
    _debug_printf_P(PSTR("SyslogTCP::SyslogTCP %s:%d TLS %d\n"), host.c_str(), port, useTLS);

    _host = host;
    _port = port;
    _useTLS = useTLS;

    _queue.isSending = false;

    _client.onConnect(_onPoll, this);
    _client.onPoll(_onPoll, this);
    _client.onDisconnect(_onDisconnect, this);
    _client.onAck(_onAck, this);
    _client.onError(_onError, this);
    _client.onTimeout(_onTimeout, this);
}

void SyslogTCP::__onPoll(AsyncClient *client) {
    if (_queue.isSending) {
        if (client->canSend()) {
            auto space = client->space(); // there is at least one byte space
            auto send = _queue.message.length();
            if (send > space) {
                send = space;
            }
            auto written = client->write(_queue.message.getConstChar(), send);
            _debug_printf_P(PSTR("SyslogTCP::__onPoll(): space=%u,length=%u,send=%u,written=%u\n"), space, _queue.message.length(), send, written);
            if (written != send) {
                _queueWriteError(IF_DEBUG("write() failed"));
            } else {
                _queue.written += written;
                if (_queue.message.length() == written) {   // all data written, wait for ack
                    _queue.message.clear();
                    _queueWritten(false);
                } else {
                    _queue.message.removeAndShrink(0, written); // remove what has been written already
                }
            }
        }
    } else {
        if (!_queue.isDisconnecting && millis() > _queue.idleTimeout) {
            _debug_printf_P(PSTR("SyslogTCP::__onPoll(): disconnecting idle connection\n"));
            _queue.isDisconnecting = true;
            client->close(false);
        }
#if DEBUG
        if (!_queue.idleTimeout) {
            _queueInvalidState();
        }
#endif
    }
}

void SyslogTCP::__onAck(AsyncClient *client, size_t len, uint32_t time) {
    _debug_printf_P(PSTR("SyslogTCP::__onAck(): len=%u,time=%u,queue:sending=%d,written=%u,sentAck=%u,left=%u\n"), len, time, _queue.isSending, _queue.written, _queue.sentAck, _queue.message.length());
    if (_queue.isSending) {
        _queue.sentAck += len;
        if (_queue.sentAck == _queue.written && _queue.message.length() == 0) {
            _queueWritten(true);
        } 
#if DEBUG
        else if (_queue.sentAck > _queue.written) {
            _queueInvalidState();
        }
    } else {
        _queueInvalidState();
#endif
    }
}

void SyslogTCP::_onDisconnect(void *arg, AsyncClient *client) {
    // not an error, calling _queueWriteError just verfies the state
    reinterpret_cast<SyslogTCP *>(arg)->_queueWriteError(IF_DEBUG("onDisconnect"));
}

void SyslogTCP::_onError(void *arg, AsyncClient *client, int8_t error) {
    reinterpret_cast<SyslogTCP *>(arg)->_queueWriteError(IF_DEBUG("onError"));
}

// default timeout is 5s for sending
void SyslogTCP::_onTimeout(void *arg, AsyncClient *client, uint32_t time) {
    reinterpret_cast<SyslogTCP *>(arg)->_queueWriteError(IF_DEBUG("onTimeout"));
}

void SyslogTCP::_onAck(void *arg, AsyncClient *client, size_t len, uint32_t time) {
    reinterpret_cast<SyslogTCP *>(arg)->__onAck(client, len, time);
}

// gets called once per second
void SyslogTCP::_onPoll(void *arg, AsyncClient *client) {
    reinterpret_cast<SyslogTCP *>(arg)->__onPoll(client);
}

void SyslogTCP::transmit(const String &message, Callback_t callback) {
    if (_queue.isSending) {
        _debug_printf_P(PSTR("FATAL: Transmit called while sending\n"));
        panic();
    }
    _queue.isSending = true;
    _queue.idleTimeout = 0;
    _queue.isDisconnecting = false;
    _queue.written = 0;
    _queue.sentAck = 0;
    _queue.message.clear();
    _queue.message.write(message.c_str(), message.length());
    _queue.message.write('\n');
    _queue.callback = callback;
    _connect();
}

bool SyslogTCP::isSending() {
	 return _queue.isSending;
}

// connect to remote host if connection is not established
void SyslogTCP::_connect() {
    _debug_printf_P(PSTR("SyslogTCP::_connect(): connected=%d,connecting=%d,disconnecting=%d\n"), _client.connected(), _client.connecting(), _client.disconnecting());
    if (!_client.connected() && !_client.connecting()) {
#if ASYNC_TCP_SSL_ENABLED
        if (!_client.connect(_host.c_str(), _port, _useTLS)) {
#else
        if (!_client.connect(_host.c_str(), _port)) {
#endif
            _debug_printf_P(PSTR("SyslogTCP::_connect(): _client.connect() failed\n"));
            _queueWriteError(IF_DEBUG("connect() failed"));
        }
    }
}

// disconnect or terminate connection
void SyslogTCP::_disconnect(bool now) {
    _debug_printf_P(PSTR("SyslogTCP::_disconnect(now=%d): connected=%d,connecting=%d,disconnecting=%d\n"), now, _client.connected(), _client.connecting(), _client.disconnecting());
    _client.close(now);
}

void SyslogTCP::_queueWritten(bool ack) {
    _debug_printf_P(PSTR("SyslogTCP::_queueWritten(%d): written=%u,left=0,sentAck=%u\n"), ack, _queue.written, _queue.sentAck);
    if (ack) {
        _queue.isSending = false;
        _queue.message.clear();
        _queue.isDisconnecting = false;
        _queue.idleTimeout = millis() + (SYSLOG_TCP_MAX_IDLE * 1000UL);
        _queue.callback(true);
    }
}

void SyslogTCP::_queueWriteError(IF_DEBUG(const char *reason)) {
    if (_queue.isSending) {
        _debug_printf_P(PSTR("SyslogTCP::_queueWriteError(%s): isSending=%u,written=%u,left=%u,sentAck=%u\n"), reason, _queue.isSending, _queue.written, _queue.message.length(), _queue.sentAck);
        _queue.isSending = false;
        _queue.isDisconnecting = true;
        _queue.idleTimeout = 0;
        _queue.message.clear();
        _disconnect(true);
        _queue.callback(false);
    }
}

void SyslogTCP::_queueInvalidState() {
    _debug_printf_P(PSTR("SyslogTCP::_queueInvalidState()\n"));
    _queueWriteError(IF_DEBUG("Invalid state"));
}
