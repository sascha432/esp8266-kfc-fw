/**
 * Author: sascha_lammers@gmx.de
 */

#include "syslog_plugin.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using SyslogClient = KFCConfigurationClasses::Plugins::SyslogClient;
using KFCConfigurationClasses::System;

static SyslogPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    SyslogPlugin,
    "syslog",           // name
    "Syslog Client",    // friendly name
    "",                 // web_templates
    "syslog",           // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::SYSLOG,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

SyslogPlugin::SyslogPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SyslogPlugin)), _stream(nullptr)
{
    REGISTER_PLUGIN(this, "SyslogPlugin");
}

void SyslogPlugin::setup(SetupModeType mode)
{
    _begin();
}

void SyslogPlugin::reconfigure(const String &source)
{
    _end();
    _begin();
}

void SyslogPlugin::shutdown()
{
    _kill(500);
    _end();
}

void SyslogPlugin::timerCallback(Event::CallbackTimerPtr timer)
{
    plugin._timerCallback(timer);
}

void SyslogPlugin::_timerCallback(Event::CallbackTimerPtr timer)
{
#if DEBUG
    assert(_stream != nullptr);
#endif
    if (_stream->hasQueuedMessages()) {
        _stream->deliverQueue();
    }
    else {
        if (!_stream->getSyslog().isSending()) {
            timer->updateInterval(Event::seconds(10));
        }
    }
}

void SyslogPlugin::_begin()
{
#if DEBUG
    assert(_stream == nullptr);
#endif

    if (SyslogClient::isEnabled()) {

        auto cfg = SyslogClient::getConfig();
        String hostname = SyslogClient::getHostname();
        auto port = cfg.getPort();

        String tmp;
        if (config.hasZeroConf(hostname)) {
            tmp = String(); // remove hostname, the class will initialize and wait for it
        }
        else {
            tmp = hostname;
        }

        _Timer(_timer).add(Event::seconds(1), true, timerCallback);

        _stream = __LDBG_new(SyslogStream, SyslogFactory::create(
            SyslogParameter(System::Device::getName(), FSPGM(kfcfw)),
            __LDBG_new(SyslogMemoryQueue, SYSLOG_PLUGIN_QUEUE_SIZE), cfg.protocol_enum, tmp, port),
            _timer
        );
        _logger.setSyslog(_stream);

        if (tmp.length() == 0) {
            config.resolveZeroConf(getFriendlyName(), hostname, port, [](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) {
                plugin._zeroConfCallback(hostname, address, port, type);
            });
        }
    }
}

void SyslogPlugin::_zeroConfCallback(const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type)
{
    __LDBG_printf("zeroconf callback host=%s address=%s port=%u stream=%p", hostname.c_str(), address.toString().c_str(), port, _stream);
    if (_stream) {
        auto &syslog = _stream->getSyslog();
        syslog.setupZeroConf(hostname, address, port);
    }
}

void SyslogPlugin::_end()
{
    if (_stream) {
        _timer.remove();
        _logger.setSyslog(nullptr);
        __LDBG_delete(_stream);
        _stream = nullptr;
    }
}

void SyslogPlugin::_kill(uint32_t timeout)
{
    if (_stream) {
        _timer.remove();

        _stream->setTimeout(timeout);
        timeout += millis();
        while(millis() < timeout && _stream->hasQueuedMessages()) {
            _stream->deliverQueue();
            delay(10); // let system process tcp buffers etc...
        }
        _stream->clearQueue();
    }
}

void SyslogPlugin::getStatus(Print &output)
{
#if SYSLOG_SUPPORT
    if (_stream) {
        auto &syslog = _stream->getSyslog();
        switch(SyslogClient::getConfig().protocol_enum) {
            case SyslogClient::SyslogProtocolType::UDP:
                output.printf_P(PSTR("UDP @ %s:%u"), syslog.getHostname().c_str(), syslog.getPort());
                return;
            case SyslogClient::SyslogProtocolType::TCP:
                output.printf_P(PSTR("TCP @ %s:%u"), syslog.getHostname().c_str(), syslog.getPort());
                return;
            case SyslogClient::SyslogProtocolType::TCP_TLS:
                output.printf_P(PSTR("TCP TLS @ %s:%u"), syslog.getHostname().c_str(), syslog.getPort());
                return;
            default:
                break;
        }
    }
    output.print(FSPGM(Disabled));
#else
    output.print(FSPGM(Not_supported));
#endif
}

#if ENABLE_DEEP_SLEEP

void SyslogPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    kill((sleepTimeMillis > 60000) ? 1000 : 250);
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SQ, "SQ", "<clear|info|queue>", "Syslog queue command");

void SyslogPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(SQ), getName_P());
}

bool SyslogPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SQ))) {
        if (_stream && args.size() >= 1) {
            int cmd = stringlist_find_P_P(PSTR("clear|info|queue"), args.get(0), '|');
            if (cmd == 0) { // clear
                _stream->clearQueue();
                args.print(F("Queue cleared"));
            }
            else if (cmd == 2) { // queue
                args.printf_P(PSTR("Messages in queue %d"), _stream->queueSize());
                _stream->dumpQueue(args.getStream());
            }
            else { // info or invalid command
                args.printf_P(PSTR("%d"), _stream->queueSize());
            }
        }
        else {
            args.print(F("Syslog is disabled"));
        }
        return true;
    }
    return false;
}

#endif
