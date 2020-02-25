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
#include "progmem_data.h"
#include "plugins.h"

#if DEBUG_HTTP2SERIAL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


Http2Serial *Http2Serial::_instance = nullptr;
WsClientAsyncWebSocket *wsSerialConsole = nullptr;

Http2Serial::Http2Serial() : _outputBufferMaxSize(SERIAL_BUFFER_MAX_LEN), _outputBufferDelay(SERIAL_BUFFER_FLUSH_DELAY)
{
    _locked = false;
#if defined(HTTP2SERIAL_BAUD) && HTTP2SERIAL_BAUD != KFC_SERIAL_RATE
    _debug_printf_P(PSTR("Reconfiguring serial port to %d baud\n"), HTTP2SERIAL_BAUD);
    Serial.flush();
    Serial.begin(HTTP2SERIAL_BAUD);
#endif
    _serialHandler = &SerialHandler::getInstance();
#if AT_MODE_SUPPORTED && HTTP2SERIAL_DISABLE_AT_MODE
    disable_at_mode(MySerial);
#endif
    resetOutputBufferTimer();
    _outputBufferEnabled = true;

    LoopFunctions::add(Http2Serial::outputLoop);
    _serialHandler->addHandler(onData, SerialHandler::RECEIVE|SerialHandler::LOCAL_TX); // RECEIVE=data received by Serial, LOCAL_TX=data sent to Serial
}


Http2Serial::~Http2Serial()
{
    LoopFunctions::remove(Http2Serial::outputLoop);
    _serialHandler->removeHandler(onData);
#if defined(HTTP2SERIAL_BAUD) && HTTP2SERIAL_BAUD != KFC_SERIAL_RATE
    Serial.flush();
    Serial.begin(KFC_SERIAL_RATE);
#endif
#if AT_MODE_SUPPORTED && HTTP2SERIAL_DISABLE_AT_MODE
    enable_at_mode(MySerial);
#endif
}

/**
 * The output buffer has a small time delay to combine multiple characters from the console into a single web socket message
 */
void Http2Serial::broadcastOutputBuffer()
{
    // os_printf("Http2Serial::broadcastOutputBuffer(%d)\n", _outputBuffer.length());

    if (_outputBuffer.length()) {
        WsClient::broadcast(wsSerialConsole, nullptr, _outputBuffer.getConstChar(), _outputBuffer.length());
        _outputBuffer.clear();
    }
    resetOutputBufferTimer();
}

void Http2Serial::writeOutputBuffer(const uint8_t *buffer, size_t len)
{
    if (!_outputBufferEnabled) {
        return;
    }
    if (_outputBuffer.length() == 0) {
        resetOutputBufferTimer(); // reset timer if it is empty
    }

    auto ptr = buffer;
    for(;;) {
        int space = _outputBufferMaxSize - _outputBuffer.length();
        if (space > 0) {
            if ((size_t)space >= len) {
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

bool Http2Serial::isTimeToSend()
{
    return (_outputBuffer.length() != 0) && (millis() > _outputBufferFlushDelay);
}

void Http2Serial::resetOutputBufferTimer()
{
    _outputBufferFlushDelay = millis() + _outputBufferDelay;
}

void Http2Serial::clearOutputBuffer()
{
    _outputBuffer.clear();
    resetOutputBufferTimer();
}

void Http2Serial::_outputLoop()
{
    if (isTimeToSend()) {
        broadcastOutputBuffer();
    }
    auto handler = getSerialHandler();
    if (handler != &SerialHandler::getInstance()) {
        handler->serialLoop();
    }
}

void Http2Serial::outputLoop()
{
    Http2Serial::_instance->_outputLoop();
}

void Http2Serial::onData(uint8_t type, const uint8_t *buffer, size_t len)
{
#if 0
    os_printf("onData(%u, %*.*s)\n", type, len, len, buffer);
#endif
    // Serial.printf_P(PSTR("Http2Serial::onData(%d, %p, %d): instance %p, locked %d\n"), type, buffer, len, Http2Serial::_instance, Http2Serial::_instance ? Http2Serial::_instance->_locked : -1);
    if (Http2Serial::_instance && !Http2Serial::_instance->_locked) {
        Http2Serial::_instance->_locked = true;
        Http2Serial::_instance->writeOutputBuffer(buffer, len);  // store data before sending
        Http2Serial::_instance->_locked = false;
    }
}

Http2Serial *Http2Serial::getInstance()
{
    return _instance;
}

void Http2Serial::createInstance()
{
    destroyInstance();
    if (!_instance) {
        _instance = new Http2Serial();
    }
}

void Http2Serial::destroyInstance()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

SerialHandler *Http2Serial::getSerialHandler() const
{
    return _serialHandler;
}

AsyncWebSocket *Http2Serial::getConsoleServer()
{
    return wsSerialConsole;
}

void http2serial_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WsClient::onWsEvent(server, client, type, data, len, arg, WsConsoleClient::getInstance);
}

class Http2SerialPlugin : public PluginComponent {
public:
    Http2SerialPlugin() {
        REGISTER_PLUGIN(this);
    }
    virtual PGM_P getName() const {
        return PSTR("http2ser");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Http2Serial");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)10;
    }
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override;
    virtual void restart() override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override {
        bootstrapMenu.addSubMenu(F("Serial Console"), F("serial_console.html"), navMenu.util);
    }

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

static Http2SerialPlugin plugin;

void Http2SerialPlugin::setup(PluginSetupMode_t mode)
{
    auto server = get_web_server_object();
    if (server) {
        wsSerialConsole = new WsClientAsyncWebSocket(F("/serial_console"));
        wsSerialConsole->onEvent(http2serial_event_handler);
        server->addHandler(wsSerialConsole);
        _debug_printf_P(PSTR("Web socket for http2serial running on port %u\n"), config._H_GET(Config().http_port));
    }
}

void Http2SerialPlugin::reconfigure(PGM_P source)
{
    setup(PLUGIN_SETUP_DEFAULT);
}

bool Http2SerialPlugin::hasReconfigureDependecy(PluginComponent *plugin) const
{
    return plugin->nameEquals(FSPGM(http));
}

void Http2SerialPlugin::restart()
{
    wsSerialConsole->restart();
    Http2Serial::destroyInstance();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(H2SBD, "H2SBD", "<baud>", "Set serial port rate");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(H2SBUFSZ, "H2SBUFSZ", "<size=512>", "Set output buffer size");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(H2SBUFDLY, "H2SBUFDLY", "<delay=60>", "Set output buffer delay");

void Http2SerialPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(H2SBD), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(H2SBUFSZ), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(H2SBUFDLY), getName());
}

bool Http2SerialPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(H2SBD))) {
        if (args.requireArgs(1, 1)) {
            uint32_t rate = args.toIntMinMax(0, 300, 2500000, KFC_SERIAL_RATE);
            if (rate) {
                Serial.flush();
                Serial.begin(rate);
                args.printf_P(PSTR("Set serial rate to %d"), (unsigned)rate);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(H2SBUFSZ))) {
        if (args.requireArgs(1, 1)) {
            auto http2Serial = Http2Serial::getInstance();
            if (http2Serial) {
                uint32_t size = args.toIntMinMax(0, 128, 2048, SERIAL_BUFFER_MAX_LEN);
                http2Serial->setOutputBufferMaxSize(size);
                args.printf_P(PSTR("Set size to %u ms"), size);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(H2SBUFDLY))) {
        if (args.requireArgs(1, 1)) {
            auto http2Serial = Http2Serial::getInstance();
            if (http2Serial) {
                uint32_t delay = args.toIntMinMax(0, 10, 500, SERIAL_BUFFER_FLUSH_DELAY);
                http2Serial->setOutputBufferDelay(delay);
                args.printf_P(PSTR("Set delay to %u ms"), delay);
            }
        }
        return true;
    }
    return false;
}

#endif

#endif
