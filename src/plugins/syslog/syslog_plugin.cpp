/**
 * Author: sascha_lammers@gmx.de
 */

#if SYSLOG

#include "kfc_fw_config.h"
#include <Buffer.h>
#include <KFCSyslog.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include "progmem_data.h"
#include "templates.h"
#include "plugins.h"


void syslog_process_queue();

#if DEBUG_USE_SYSLOG

SyslogStream *debugSyslog = nullptr;

void syslog_setup_debug_logger() {

    SyslogParameter parameter;
    parameter.setHostname(config.getString(_H(Config().device_name)));
    parameter.setAppName(SPGM(kfcfw));
    parameter.setProcessId(F("DEBUG"));
	parameter.setSeverity(SYSLOG_DEBUG);

    SyslogFilter *filter = new SyslogFilter(parameter);
    filter->addFilter(F("*.*"), F(DEBUG_USE_SYSLOG_TARGET));

	SyslogQueue *queue = new SyslogMemoryQueue(2048);
    debugSyslog = new SyslogStream(filter, queue);

    DEBUG_OUTPUT = *debugSyslog;

    debug_printf_P(PSTR("Debug Syslog enabled, target " DEBUG_USE_SYSLOG_TARGET "\n"));

    LoopFunctions::add(syslog_process_queue);
}

#endif

SyslogStream *syslog = nullptr;

void syslog_setup_logger() {

    if (syslog) {
        _logger.setSyslog(nullptr);
        delete syslog;
        syslog = nullptr;
#if DEBUG_USE_SYSLOG == 0
        LoopFunctions::remove(syslog_process_queue);
#endif
    }

    if (config._H_GET(Config().flags).syslogProtocol != SYSLOG_PROTOCOL_NONE) {

        SyslogParameter parameter;
        parameter.setHostname(config.getString(_H(Config().device_name)));
        parameter.setAppName(SPGM(kfcfw));
        parameter.setFacility(SYSLOG_FACILITY_KERN);
        parameter.setSeverity(SYSLOG_NOTICE);

        SyslogFilter *filter = new SyslogFilter(parameter);
        filter->addFilter(F("*.*"), SyslogFactory::create(filter->getParameter(), (SyslogProtocol)config._H_GET(Config().flags).syslogProtocol, config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port)));

        SyslogQueue *queue = debugSyslog ? debugSyslog->getQueue() : new SyslogMemoryQueue(512);
        syslog = new SyslogStream(filter, queue);

        _logger.setSyslog(syslog);
        LoopFunctions::add(syslog_process_queue);
    }
}

void syslog_setup() {
    syslog_setup_debug_logger();
    syslog_setup_logger();
}

void syslog_process_queue() {
    static MillisTimer timer(1000UL);
    if (timer.reached()) {
        if (debugSyslog) {
            if (debugSyslog->hasQueuedMessages()) {
                debugSyslog->deliverQueue();
            }
        }
        if (syslog) {
            if (syslog->hasQueuedMessages()) {
                syslog->deliverQueue();
            }
        }
        timer.restart();
    }
}

#endif

const String syslog_get_status() {
#if SYSLOG
    PrintHtmlEntitiesString out;
    switch(config._H_GET(Config().flags).syslogProtocol) {
        case SYSLOG_PROTOCOL_UDP:
            out.printf_P(PSTR("UDP @ %s:%u"), config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port));
            break;
        case SYSLOG_PROTOCOL_TCP:
            out.printf_P(PSTR("TCP @ %s:%u"), config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port));
            break;
        case SYSLOG_PROTOCOL_TCP_TLS:
            out.printf_P(PSTR("TCP TLS @ %s:%u"), config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port));
            break;
        default:
            out += SPGM(Disabled);
            break;
    }
    // #if SYSLOG_SPIFF_QUEUE_SIZE
    // if (!_Config.getOptions().isSyslogProtocol(SYSLOG_PROTOCOL_NONE) && syslogQueue && syslogQueue->hasSyslog()) {
    //     SyslogQueueInfo *info = syslogQueue->getQueueInfo();
    //     size_t currentSize = /*Syslog::*/syslogQueue->getSize();
    //     out.printf_P(PSTR(HTML_S(br) "Queue buffer on SPIFF %s/%s (%.2f%%)"), formatBytes(currentSize).c_str(), formatBytes(SYSLOG_SPIFF_QUEUE_SIZE).c_str(), currentSize * 100.0 / (float)SYSLOG_SPIFF_QUEUE_SIZE);
    //     if (info->transmitted || info->dropped) {
    //         out.printf_P(PSTR(HTML_S(br) "Transmitted %u, dropped %u"), info->transmitted, info->dropped);
    //     }
    // } else {
    //     return F("Syslog disabled");
    // }
    // #endif
#if DEBUG_USE_SYSLOG
    out.print(F(HTML_S(br) "Debugging enabled, target " DEBUG_USE_SYSLOG_TARGET));
#endif
    return out;
#else
    return SPGM(Not_supported);
#endif
}

void syslog_create_settings_form(AsyncWebServerRequest *request, Form &form) {

    form.add<uint8_t>(F("syslog_enabled"), config._H_GET(Config().flags).syslogProtocol, [](uint8_t value, FormField *) {
        auto &flags = config._H_W_GET(Config().flags);
        flags.syslogProtocol = value;
    });
    form.addValidator(new FormRangeValidator(0, (long)SYSLOG_PROTOCOL_FILE));

    form.add<sizeof Config().syslog_host>(F("syslog_host"), config._H_W_STR(Config().syslog_host));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<uint16_t>(F("syslog_port"), config._H_GET(Config().syslog_port), [&](uint16_t port, FormField *field) {
        if (port == 0) {
            port = 514;
            field->setValue(String(port));
        }
        config._H_SET(Config().syslog_port, port);
    });
    form.addValidator(new FormRangeValidator(F("Invalid port"), 0, 65535));

    form.finalize();
}

bool syslog_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {

    if (command.length() == 0) {
        serial.print(F(
            " AT+SQC\n"
            "    Clear syslog queue\n"
            " AT+SQI\n"
            "    Display syslog queue info\n"
            " AT+SQD\n"
            "    Display syslog queue\n"
        ));
    } else if (command.equalsIgnoreCase(F("SQC"))) {
        if (syslog) {
            syslog->getQueue()->cleanUp();
            serial.println(F("Syslog queue cleared"));
        }
        return true;
    } else if (command.equalsIgnoreCase(F("SQI"))) {
        if (syslog) {
            serial.printf_P(PSTR("Syslog queue size: %d\n"), syslog->getQueue()->size());
        }
        return true;
    } else if (command.equalsIgnoreCase(F("SQD"))) {
        if (syslog) {
            auto *queue = syslog->getQueue();
            for(auto it = queue->begin(); it != queue->end(); ++it) {
                auto item = *it;
                if (item->getId() != 0) {
                    serial.printf_P(PSTR("Syslog queue id %d (failures %d): %s\n"), item->getId(), item->getFailureCount(), item->getMessage().c_str());
                }
            }
        }
        return true;
    }
    return false;
}

void syslog_prepare_deep_sleep(uint32_t time, RFMode mode) {

    //TODO the send timeout could be reduces for short sleep intervals

    if (WiFi.isConnected()) {
        if (debugSyslog) {
            ulong timeout = millis() + 2000;    // long timeout in debug mode
            while(debugSyslog->hasQueuedMessages() && millis() < timeout) {
                debugSyslog->deliverQueue();
                delay(1);
            }
        }
        if (syslog) {
            ulong timeout = millis() + 150;
            while(syslog->hasQueuedMessages() && millis() < timeout) {
                syslog->deliverQueue();
                delay(1);
            }
        }
    }
}

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ syslog,
/* setupPriority            */ 9,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ true,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ syslog_setup,
/* statusTemplate           */ syslog_get_status,
/* configureForm            */ syslog_create_settings_form,
/* reconfigurePlugin        */ syslog_setup_logger,
/* prepareDeepSleep         */ syslog_prepare_deep_sleep,
/* atModeCommandHandler     */ syslog_at_mode_command_handler
);


#if 0
size_t SyslogMemoryQueue::store(const String &message) {
    if_debug_printf_P(PSTR("store, queue size %d, msg %d\n"), _queueSize, message.length());
    _queue.push_back(message);
    _queueSize += sizeof(String) + message.length();

    static int maxSize = ESP.getFreeHeap() / 2;

    while((int)_queueSize > maxSize && !_queue.empty()) {
        if_debug_printf(PSTR("erase queue size %d\n"), _queueSize);
        _queueSize -= sizeof(String) +_queue.front().length();
        _queue.erase(_queue.begin());
    }
    if (_queue.empty()) {
        _queueSize = 0;
    }
    return _queue.size();
}


#if defined(ESP32)

bool SyslogTCP::_connect() {
    if_debug_println(F("_connect"));
    if (_client != nullptr) {
        delete _client;
    }
    _client = new SyncClient();
    int status = _client->connect(_host, _port, _useTLS); // connect does not use setTimeout()
    _client->setTimeout(5);
    if_debug_printf_P(PSTR("Connect status %d\n"), status);
    if (status == 0) {
        delete _client;
        _client = nullptr;
    }
    return status != 0;
}

void SyslogTCP::transmit(const String &message) {
    if_debug_printf_P(PSTR("transmit %s\n"), message.c_str());
    if (!canSend()) {
        _syslogQueue->transmitError();
    } else {
        ESP.wdtDisable();
        if (_client == nullptr || !_client->connected()) {
            if (!_connect()) {
                if_debug_printf_P(PSTR("Cannot connect to %s:%d\n"), _host, _port);
                _syslogQueue->transmitError();
                ESP.wdtEnable(1000);
                return;
            }
        }

        while(_client->available() && _client->read() != -1) {
            // clear receive buffer and discard data
        }

        String msg = message + '\n';
        size_t result = _client->write((const uint8_t *)msg.c_str(), msg.length());
        ESP.wdtEnable(1000);

        if_debug_printf_P(PSTR("transmit() = %d / %d\n"), result, msg.length());
        if (result != msg.length()) {
            _syslogQueue->transmitError();
        } else {
            if_debug_println(F("flush"));
            _client->flush();
            _syslogQueue->loadNext();
        }
    }
}

#endif

#if !defined(ESP32)
void SyslogAsyncTCP::_onConnect(void *arg, AsyncClient *client) {

    if_debug_printf_P(PSTR("onConnect event, state %d, %s\n"), client->canSend() ? 1 : 0, client->stateToString());
#if DEBUG
    client->onData([](void*, AsyncClient*, void *data, size_t len) {
        if_debug_printf_P(PSTR("received data, length %d\n"), len); // should not happen, ignore data
    });
#endif
    reinterpret_cast<SyslogAsyncTCP *>(arg)->_send();
}

void SyslogAsyncTCP::_onAck(void *arg, AsyncClient *, size_t len, uint32_t time) {
    if_debug_printf_P(PSTR("onAck event %d, %lu\n"), len, time);
    reinterpret_cast<SyslogAsyncTCP *>(arg)->_ack(len);
}

void SyslogAsyncTCP::_onTimeout(void *arg, AsyncClient *client, uint32_t time) {
    if_debug_printf_P(PSTR("onTimeout event %lu\n"), time);
    reinterpret_cast<SyslogAsyncTCP *>(arg)->_disconnect();
}

void SyslogAsyncTCP::_onError(void *arg, AsyncClient *client, int8_t error) {
    if_debug_printf_P(PSTR("onError event %s\n"), client->errorToString(error));
    reinterpret_cast<SyslogAsyncTCP *>(arg)->_disconnect();
}

void SyslogAsyncTCP::_onDisconnect(void *arg, AsyncClient *client) {
    if_debug_println(F("onDisconnect: onPoll event"));
    reinterpret_cast<SyslogAsyncTCP *>(arg)->_disconnect();
}

void SyslogAsyncTCP::_onPoll(void *arg, AsyncClient *client) {
    if_debug_println(F("SyslogAsyncTCP: onPoll event"));
    reinterpret_cast<SyslogAsyncTCP *>(arg)->onPoll(client);
}

void SyslogAsyncTCP::_send() {
    _ackLen = -1;
    if_debug_printf_P(PSTR("_send %s"), _message.c_str());
    _ackLen = _client->write(_message.c_str(), _message.length());
    if_debug_printf_P(PSTR("transmit() = %d / %d\n"), _ackLen, _message.length());
    if (_ackLen == 0) {
        if_debug_println(F("acklen = 0"));
        _disconnect();
    }
}

void SyslogAsyncTCP::_ack(size_t len) {
    if (_ackLen == -1) {
        if_debug_printf_P(PSTR("ack len = -1, len %d\n"), len);
        return;
    } else if (len != (size_t)_ackLen) {
        if_debug_printf_P(PSTR("ack len %d / %d: failure\n"), len, _ackLen);
        _disconnect();
        return;
    }
    _ackLen = -1;
    _message.remove(0, len); // remove already acknowledged part

    if (_message.length() == 0) {
        if_debug_printf_P(PSTR("message sent %d\n"), _message.length());
        _syslogQueue->loadNext(25); // load a bit faster then the blocking version, max. 40 messags per second
        return;
    }

    _ackLen = _client->write(_message.c_str(), _message.length());
    if_debug_printf_P(PSTR("transmit() = %d / %d\n"), _ackLen, _message.length());
    if (_ackLen == 0) {
        _disconnect();
    }
}

bool SyslogAsyncTCP::canSend() {
    if (!WiFi.isConnected()) {
        if_debug_println(F("canSend() = false: WiFi not connected"));
    }
    if (_ackLen != -1) {
        if_debug_println(F("canSend() = false: waiting for acknowledgement"));
    }
    if (_message.length() != 0) {
        if_debug_printf_P(PSTR("canSend() = false: message not 0, %d\n"), _message.length());
    }
    return (WiFi.isConnected() && _ackLen == -1 && _message.length() == 0);
}

// not in use
void SyslogAsyncTCP::onPoll(AsyncClient *client) {
    if_debug_printf_P(PSTR("onPoll() = ack len %d, message len %d\n"), _ackLen, _message.length());
}

void SyslogAsyncTCP::_disconnect() {
    if_debug_println(F("_disconnect"));
    if (_ackLen != -1 || _message.length()) { // if this is false it might have been closed cause of being idle
        _close();
        _syslogQueue->transmitError();
    }
    _close();
}

void SyslogAsyncTCP::_close() {
    if_debug_println(F("_close"));
    if (_client) {
        delete _client;
        _client = nullptr;
    }
    _ackLen = -1;
    _message = String();
}

void SyslogAsyncTCP::transmit(const String &message) {
    if_debug_print("transmit: " + message);
    if (!canSend()) {
        _syslogQueue->transmitError();
    } else {
        _ackLen = -1;
        _message = message + '\n';
        if (_client != nullptr && !_client->canSend()) {
            if_debug_println(F("TCP socket does not accept data, closing connection first"));
            _close();
        }
        if (_client == nullptr || !_client->connected()) {
            if_debug_println(F("TCP connection open"));
            _client = new AsyncClient(); // open connection and wait for ack
            _client->setAckTimeout(ASYNC_MAX_ACK_TIME);
            _client->setRxTimeout(15000); // close idle connection after 15 seconds
            _client->onConnect(SyslogAsyncTCP::_onConnect, this);
            _client->onAck(SyslogAsyncTCP::_onAck, this);
            _client->onDisconnect(SyslogAsyncTCP::_onDisconnect, this);
            _client->onTimeout(SyslogAsyncTCP::_onTimeout, this);
            _client->onError(SyslogAsyncTCP::_onError, this);
            // client->onPoll(SyslogAsyncTCP::_onPoll, this);
#if ASYNC_TCP_SSL_ENABLED
            _client->connect(_host, _port, _useTLS);
#else
            _client->connect(_host, _port);
#endif
            return;
        } else {
            _send(); // send message and wait for ack
        }
    }
}
#endif

#endif
