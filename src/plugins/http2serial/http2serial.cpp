/*
 * Author: sascha_lammers@gmx.de
 */

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
#include <PluginComponent.h>
#include "plugins_menu.h"

#if DEBUG_HTTP2SERIAL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AUTO_STRING_DEF(serial_console_html, "serial-console.html")
AUTO_STRING_DEF(_serial_console, "/serial-console")
AUTO_STRING_DEF(Serial_Console, "Serial Console")

using KFCConfigurationClasses::System;

Http2Serial *Http2Serial::_instance = nullptr;
WsClientAsyncWebSocket *wsSerialConsole = nullptr;

Http2Serial::Http2Serial() :
    _client(serialHandler.addClient(onData, SerialHandler::EventType::RW)),
    _locked(false),
    _outputBufferEnabled(true),
    _clientLocked(false)

{
    #if defined(HTTP2SERIAL_BAUD) && HTTP2SERIAL_BAUD != KFC_SERIAL_RATE
        __LDBG_printf("Reconfiguring serial port to %d baud", HTTP2SERIAL_BAUD);
        Serial.flush();
        Serial.begin(HTTP2SERIAL_BAUD);
    #endif
    #if AT_MODE_SUPPORTED && HTTP2SERIAL_DISABLE_AT_MODE
        disable_at_mode(Serial);
    #endif
    resetOutputBufferTimer();

    LOOP_FUNCTION_ADD(Http2Serial::outputLoop);
}

Http2Serial::~Http2Serial()
{
    LoopFunctions::remove(Http2Serial::outputLoop);
    serialHandler.removeClient(_client);
    #if defined(HTTP2SERIAL_BAUD) && HTTP2SERIAL_BAUD != KFC_SERIAL_RATE
        Serial.flush();
        Serial.begin(KFC_SERIAL_RATE);
    #endif
    #if AT_MODE_SUPPORTED && HTTP2SERIAL_DISABLE_AT_MODE
        enable_at_mode(Serial);
    #endif
}

/**
 * The output buffer has a small time delay to combine multiple characters from the console into a single web socket message
 */
void Http2Serial::broadcastOutputBuffer()
{
    if (_outputBuffer.length()) {
        WsClient::broadcast(wsSerialConsole, nullptr, _outputBuffer.getConstChar(), _outputBuffer.length());
        _outputBuffer.clear();
    }
    resetOutputBufferTimer();
}

void Http2Serial::writeOutputBuffer(Stream &client)
{
    if (!_outputBufferEnabled) {
        static_cast<SerialHandler::Client &>(client).flush();
        return;
    }
    if (_outputBuffer.length() < kOutputBufferMinSize) {
        resetOutputBufferTimer(); // reset timer
    }

    while(client.available()) {
        if (_outputBuffer.length() < kOutputBufferMaxSize) {
            _outputBuffer.write(client.read());
        }
        else {
            broadcastOutputBuffer();
        }
    }

    if (kOutputBufferFlushDelay == 0 || ((_outputBuffer.length() >= kOutputBufferMinSize) && (millis() > _outputBufferFlushDelay))) {
        broadcastOutputBuffer();
    }
}

void Http2Serial::onData(Stream &client)
{
    // Serial.printf_P(PSTR("Http2Serial::onData(%d, %p, %d): instance %p, locked %d\n"), type, buffer, len, Http2Serial::_instance, Http2Serial::_instance ? Http2Serial::_instance->_locked : -1);
    if (Http2Serial::_instance && !Http2Serial::_instance->_locked) {
        Http2Serial::_instance->_locked = true;
        _instance->writeOutputBuffer(client);
        //Http2Serial::_instance->writeOutputBuffer(reinterpret_cast<const uint8_t *>(buffer), len);  // store data before sending
        Http2Serial::_instance->_locked = false;
    }
}

AsyncWebSocketClient *Http2Serial::getClientById(const void *clientId)
{
    if (wsSerialConsole) {
        for(auto client: wsSerialConsole->getClients()) {
            if ((clientId == nullptr || reinterpret_cast<const void *>(client) == clientId) && client->status() && client->_tempObject && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
                return client;
            }
        }
    }
    return nullptr;
}

class Http2SerialPlugin : public PluginComponent
{
public:
    Http2SerialPlugin();

    virtual void setup(SetupModeType mode, const DependenciesPtr &dependencies) override;
    // virtual void reconfigure(const String &source) override;
    virtual void shutdown() override {
        Http2Serial::destroyInstance();
    }

    virtual void createMenu() override {
        bootstrapMenu.addMenuItem(FSPGM(Serial_Console), FSPGM(serial_console_html), navMenu.util);
        bootstrapMenu.addMenuItem(FSPGM(Serial_Console), FSPGM(serial_console_html), navMenu.home, bootstrapMenu.findMenuByURI(FSPGM(password_html), navMenu.home)->getId());
    }

    #if AT_MODE_SUPPORTED
        #if AT_MODE_HELP_SUPPORTED
            virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
        #endif
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif
};

static Http2SerialPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Http2SerialPlugin,
    "http2serial",      // name
    "Http2Serial",      // friendly name
    "",                 // web_templates
    "",                 // config_forms
    "http",             // reconfigure_dependencies
    PluginComponent::PriorityType::HTTP2SERIAL,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    true,               // allow_safe_mode
    false,              // setup_after_deep_sleep
    false,              // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

Http2SerialPlugin::Http2SerialPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(Http2SerialPlugin))
{
    REGISTER_PLUGIN(this, "Http2SerialPlugin");
}

void Http2SerialPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    auto server = WebServer::Plugin::getWebServerObject();
    if (server) {
        // __LDBG_printf("server=%p console=%p", server, wsSerialConsole);
        auto ws = new WsClientAsyncWebSocket(FSPGM(_serial_console), &wsSerialConsole);
        ws->onEvent(Http2Serial::eventHandler);
        server->addHandler(ws);
        __LDBG_printf("Web socket for http2serial running on port %u", System::WebServer::getConfig().port);
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(H2SBD, "H2SBD", "<baud>", "Set serial port rate");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr Http2SerialPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(H2SBD),
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool Http2SerialPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(H2SBD))) {
        if (args.requireArgs(1, 1)) {
            uint32_t rate = args.toIntMinMax(0, 300, 2500000, KFC_SERIAL_RATE);
            if (rate) {
                KFC_SAFE_MODE_SERIAL_PORT.end();
                KFC_SAFE_MODE_SERIAL_PORT.begin(rate);
                args.printf_P(PSTR("Set serial rate to %d"), (unsigned)rate);
            }
        }
        return true;
    }
    return false;
}

#endif
