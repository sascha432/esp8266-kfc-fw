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

Http2Serial::Http2Serial() {
#ifdef HTTP2SERIAL_BAUD
    _debug_printf_P(PSTR("Reconfiguring serial port to %d baud\n"), HTTP2SERIAL_BAUD);
    Serial.flush();
    Serial.begin(HTTP2SERIAL_BAUD);
#endif
    _serialWrapper = &serialHandler.getWrapper();
    _serialHandler = &serialHandler;
#if AT_MODE_SUPPORTED && HTTP2SERIAL_DISABLE_AT_MODE
    disable_at_mode();
#endif
    resetOutputBufferTimer();
    _outputBufferEnabled = true;

#if HTTP2SERIAL_SERIAL_HANDLER
    LoopFunctions::add(Http2Serial::inputLoop);
#endif
    LoopFunctions::add([this]() {
        this->outputLoop();
    }, reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
    _serialHandler->addHandler(onData, SerialHandler::RECEIVE|SerialHandler::LOCAL_TX);
}


Http2Serial::~Http2Serial() {
#if HTTP2SERIAL_SERIAL_HANDLER
    LoopFunctions::remove(Http2Serial::inputLoop);
#endif
    LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
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
            if (pair.socket->status() == WS_CONNECTED && pair.wsClient != sender && pair.wsClient->isAuthenticated()) {
                pair.socket->text((uint8_t *)message, len);
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

void Http2Serial::outputLoop() {
    if (isTimeToSend()) {
        broadcastOutputBuffer();
    }
    auto *handler = getSerialHandler();
    if (handler != &serialHandler) {
        handler->serialLoop();
    }
}

void Http2Serial::onData(uint8_t type, const uint8_t *buffer, size_t len) {
    //os_printf("Http2Serial::onData(%d, %p, %d)\n", type, buffer, len);
    if (Http2Serial::_instance) {
        static bool locked = false;
        if (!locked) {
            locked = true;
            Http2Serial::_instance->writeOutputBuffer(buffer, len);  // store data before sending
            locked = false;
        }
    }
}

SerialHandler *Http2Serial::getSerialHandler() {
    return _serialHandler;
}

void http2serial_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WsClient::onWsEvent(server, client, type, data, len, arg, WsConsoleClient::getInstance);
}

void http2serial_install_web_server_hook() {
    if (get_web_server_object()) {
        AsyncWebSocket *ws_console = _debug_new AsyncWebSocket(F("/serial_console"));
        ws_console->onEvent(http2serial_event_handler);
        web_server_add_handler(ws_console);
        _debug_printf_P(PSTR("Web socket for http2serial running on port %u\n"), config.get<uint16_t>(_H(Config().http_port)));
    }
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
/* reconfigurePlugin        */ http2serial_install_web_server_hook,
/* reconfigure Dependencies */ SPGM(plugin_config_name_http),
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ http2_serial_at_mode_command_handler
);

#endif
