/*
 * Author: sascha_lammers@gmx.de
 */

#if HTTP2SERIAL

// Allows to connect over web sockets to the serial port, similar to Serial2TCP

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <Buffer.h>
#include <LoopFunctions.h>
#include "http2serial.h"
#include "kfc_fw_config.h"
#include "serial_handler.h"
#include "web_server.h"
#include "web_socket.h"
#include "plugins.h"

#if DEBUG_HTTP2SERIAL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


Http2Serial *Http2Serial::_instance = nullptr;
AsyncWebSocket *wsSerialConsole = nullptr;

Http2Serial::Http2Serial() {
    _locked = false;
#ifdef HTTP2SERIAL_BAUD
    _debug_printf_P(PSTR("Reconfiguring serial port to %d baud\n"), HTTP2SERIAL_BAUD);
    Serial.flush();
    Serial.begin(HTTP2SERIAL_BAUD);
#endif
    //_serialWrapper = &serialHandler.getWrapper();
    _serialHandler = &serialHandler;
#if AT_MODE_SUPPORTED && HTTP2SERIAL_DISABLE_AT_MODE
    disable_at_mode();
#endif
    resetOutputBufferTimer();
    _outputBufferEnabled = true;

#if HTTP2SERIAL_SERIAL_HANDLER
    LoopFunctions::add(Http2Serial::inputLoop);
#endif
    LoopFunctions::add(Http2Serial::outputLoop);
    _serialHandler->addHandler(onData, SerialHandler::RECEIVE|SerialHandler::LOCAL_TX); // RECEIVE=data received by Serial, LOCAL_TX=data sent to Serial
}


Http2Serial::~Http2Serial() {
#if HTTP2SERIAL_SERIAL_HANDLER
    LoopFunctions::remove(Http2Serial::inputLoop);
#endif
    LoopFunctions::remove(Http2Serial::outputLoop);
    _serialHandler->removeHandler(onData);
#ifdef HTTP2SERIAL_BAUD
    Serial.flush();
    Serial.begin(115200);
#endif
#if AT_MODE_SUPPORTED && HTTP2SERIAL_DISABLE_AT_MODE
    enable_at_mode();
#endif
}

void Http2Serial::broadcast(WsConsoleClient *sender, const uint8_t *message, size_t len) {

    if (WsClientManager::getWsClientCount(true)) {
        for(const auto &pair: WsClientManager::getWsClientManager()->getClients()) {
            // Serial.printf_P(PSTR("status %d, isSender %d, auth %d\n"), pair.socket->status(), pair.wsClient != sender, pair.wsClient->isAuthenticated());
            if (pair.socket->server() == wsSerialConsole && pair.socket->status() == WS_CONNECTED && pair.wsClient != sender && pair.wsClient->isAuthenticated()) {
                //TODO esp32 message are stuck in the queue
                pair.socket->text(reinterpret_cast<const char *>(message), len);
            }
        }
    }
}

/**
 * The output buffer has a small time delay to combine multiple characters from the console into a single web socket message
 */
void Http2Serial::broadcastOutputBuffer() {

    //os_printf("Http2Serial::broadcastOutputBuffer(%d)\n", _outputBuffer.length());
    if (_outputBuffer.length()) {
        broadcast(_outputBuffer);
        _outputBuffer.clear();
    }
    resetOutputBufferTimer();
}

void Http2Serial::writeOutputBuffer(const uint8_t *buffer, size_t len) {

    if (!_outputBufferEnabled) {
        // Serial.println(F("_outputBufferEnabled=false"));
        return;
    }

    const uint8_t *ptr = buffer;
    for(;;) {
        int space = SERIAL_BUFFER_MAX_LEN - _outputBuffer.length();
        if (space > 0) {
            if (space >= (int)len) {
                space = len;
            }
            _outputBuffer.write(ptr, space);
            ptr += space;
            len -= space;
        }
        if (len == 0) {
            break;
        }
        broadcastOutputBuffer();
    }

    if (millis() > _outputBufferFlushDelay || !_outputBufferFlushDelay) {
        broadcastOutputBuffer();
    }
}

bool Http2Serial::isTimeToSend() {
    return (_outputBuffer.length() != 0) && (millis() > _outputBufferFlushDelay);
}

void Http2Serial::resetOutputBufferTimer() {
    _outputBufferFlushDelay = millis() + SERIAL_BUFFER_FLUSH_DELAY;
}

void Http2Serial::clearOutputBuffer() {
    _outputBuffer.clear();
    resetOutputBufferTimer();
}

void Http2Serial::_outputLoop() {
    if (isTimeToSend()) {
        broadcastOutputBuffer();
    }
    auto handler = getSerialHandler();
    if (handler != &serialHandler) {
        handler->serialLoop();
    }
}

void Http2Serial::outputLoop() {
    Http2Serial::_instance->_outputLoop();
}

void Http2Serial::onData(uint8_t type, const uint8_t *buffer, size_t len) {
    // Serial.printf_P(PSTR("Http2Serial::onData(%d, %p, %d): instance %p, locked %d\n"), type, buffer, len, Http2Serial::_instance, Http2Serial::_instance ? Http2Serial::_instance->_locked : -1);
    if (Http2Serial::_instance && !Http2Serial::_instance->_locked) {
        Http2Serial::_instance->_locked = true;
        Http2Serial::_instance->writeOutputBuffer(buffer, len);  // store data before sending
        Http2Serial::_instance->_locked = false;
    }
}

Http2Serial *Http2Serial::getInstance() {
    return _instance;
}

void Http2Serial::createInstance() {
    if (!_instance) {
        _instance = _debug_new Http2Serial();
    }
}

void Http2Serial::destroyInstance() {
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

SerialHandler *Http2Serial::getSerialHandler() const {
    return _serialHandler;
}

void http2serial_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WsClient::onWsEvent(server, client, type, data, len, arg, WsConsoleClient::getInstance);
}

void http2serial_install_web_server_hook() {
    if (get_web_server_object()) {
        wsSerialConsole = _debug_new AsyncWebSocket(F("/serial_console"));
        wsSerialConsole->onEvent(http2serial_event_handler);
        web_server_add_handler(wsSerialConsole);
        _debug_printf_P(PSTR("Web socket for http2serial running on port %u\n"), config.get<uint16_t>(_H(Config().http_port)));
    }
}

void http2serial_reconfigure(PGM_P source) {
    http2serial_install_web_server_hook();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(H2SBD, "H2SBD", "<baud>", "Set serial port rate");

bool http2_serial_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {

    if (command.length() == 0) {

        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(H2SBD));

    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(H2SBD))) {
        if (argc == 1) {
            uint32_t rate = atoi(argv[0]);
            if (rate) {
                Serial.flush();
                Serial.begin(rate);
                serial.printf_P(PSTR("Set serial rate to %d\n"), (unsigned)rate);
            }
            return true;
        }
    }
    return false;
}

#endif

PROGMEM_STRING_DECL(plugin_config_name_http);

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ http2ser,
/* setupPriority            */ 10,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ http2serial_install_web_server_hook,
/* statusTemplate           */ nullptr,
/* configureForm            */ nullptr,
/* reconfigurePlugin        */ http2serial_reconfigure,
/* reconfigure Dependencies */ SPGM(plugin_config_name_http),
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ http2_serial_at_mode_command_handler
);

#endif
