/**
 * Author: sascha_lammers@gmx.de
 */

#include "syslog_plugin.h"
#include <SyslogMemoryQueue.h>
#include <SyslogStream.h>
#include <SyslogFactory.h>

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable.h>
#include <debug_helper_disable_mem.h>
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
}

void SyslogPlugin::timerCallback(Event::CallbackTimerPtr timer)
{
    plugin._timerCallback(timer);
}

void SyslogPlugin::queueSize(uint32_t size, bool isAvailable)
{
    __LDBG_printf("size=%u available=%u timer=%u stream=%p", size, isAvailable, (bool)_timer, _stream);
    if (!_stream) {
        return;
    }
    if (size == 0) {
        __LDBG_print("remove timer");
        _timer.remove();
    }
    else if (isAvailable && !_timer) {
        __LDBG_print("add timer");
        _Timer(_timer).add(Event::milliseconds(125), true, timerCallback);
    }
}

SyslogPlugin::NameType SyslogPlugin::protocolToString(SyslogProtocol proto)
{
    switch(proto) {
        case SyslogProtocol::TCP:
            return F("TCP");
        case SyslogProtocol::TCP_TLS:
            return F("Secure TCP");
        case SyslogProtocol::UDP:
            return F("UDP");
        case SyslogProtocol::FILE:
            return F("File");
        default:
            break;
    }
    return nullptr;
}

void SyslogPlugin::_timerCallback(Event::CallbackTimerPtr timer)
{
    __LDBG_assert(_stream != nullptr);
    if (_stream->hasQueuedMessages()) {
        _stream->deliverQueue();
    }
}

void SyslogPlugin::_begin()
{
    __LDBG_assert(_stream == nullptr);
    if (SyslogClient::isEnabled()) {

        auto cfg = SyslogClient::getConfig();
        String hostname = SyslogClient::getHostname();
        uint16_t port = cfg.getPort();
        hostname.trim();
        if (hostname.length() && port) {
            bool zeroconf = config.hasZeroConf(hostname);

            auto queue = __LDBG_new(SyslogMemoryQueue, *this, SYSLOG_PLUGIN_QUEUE_SIZE);
            auto syslog = SyslogFactory::create(SyslogParameter(System::Device::getName(), FSPGM(kfcfw)), queue, cfg.protocol_enum, zeroconf ? emptyString : hostname, static_cast<uint16_t>(zeroconf ? SyslogFactory::kZeroconfPort : port));
            _stream = __LDBG_new(SyslogStream, syslog, _timer);
            _logger.setSyslog(_stream);

            __LDBG_printf("zeroconf=%u port=%u", zeroconf, port);
            if (zeroconf) {
                auto stream = _stream;
                config.resolveZeroConf(getFriendlyName(), hostname, port, [stream](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) {
                    plugin._zeroConfCallback(stream, hostname, address, port, type);
                });
            }
        }
        else {
            __LDBG_printf("hostname=%s port=%u", hostname.c_str(), port);
        }
    }
}

void SyslogPlugin::_zeroConfCallback(const SyslogStream *stream, const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type)
{
    __LDBG_printf("zeroconf callback host=%s address=%s port=%u type=%u _stream=%p stream=%p", hostname.c_str(), address.toString().c_str(), port, type, _stream, stream);
    // make sure that the callback belongs to the same stream
    if (_stream == stream) {
        _stream->_syslog.setupZeroConf(hostname, address, port);
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
        //stream->_syslog._queue.setManager(nullptr);
        auto stream = _stream;
        _stream = nullptr;
        _timer.remove();

        stream->setTimeout(timeout);
        timeout += millis();
        while(millis() < timeout && stream->hasQueuedMessages()) {
            stream->deliverQueue();
            delay(10); // let system process tcp buffers etc...
        }
        stream->clearQueue();

        // __LDBG_delete(stream);
    }
}

void SyslogPlugin::getStatus(Print &output)
{
#if SYSLOG_SUPPORT
    if (_stream) {
        auto proto = protocolToString(SyslogClient::getConfig().protocol_enum);
        if (!proto) {
            output.print(FSPGM(Disabled));
            return;
        }
        output.print(proto);

        auto &syslog = _stream->_syslog;
        if (syslog.getPort() == SyslogFactory::kZeroconfPort) {
            output.print(F(" @ resolving zeroconf"));
        }
        else {
            output.printf_P(PSTR(" @ %s:%u"), syslog.getHostname().c_str(), syslog.getPort());
        }
        auto &queue = _stream->_syslog._queue;
        if (queue.empty()) {
            output.print(F(" - queue empty"));
        }
        else {
            output.printf_P(PSTR(" - %u message(s) queued, state %s"), queue.size(), syslog.isSending() ? PSTR("sending") : PSTR("waiting"));
        }
        if (queue.getDropped()) {
            output.printf_P(PSTR(", %u dropped"), queue.getDropped());
        }
    }
    else {
        output.print(FSPGM(Disabled));
    }

#else
    output.print(FSPGM(Not_supported));
#endif
}

#if ENABLE_DEEP_SLEEP

void SyslogPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    _kill((sleepTimeMillis > 60000) ? 1000 : 250);
}

#endif

#if AT_MODE_SUPPORTED

#include <at_mode.h>
#include <logger.h>

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SQ, "SQ", "<clear|info|queue|pause>", "Syslog queue command");
#if LOGGER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LOG, "LOG", "[<error|security|warning|notice|debug>,]<message>", "Send message to the logger component");
#endif

ATModeCommandHelpArrayPtr SyslogPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(SQ),
#if LOGGER
        PROGMEM_AT_MODE_HELP_COMMAND(LOG),
#endif
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool SyslogPlugin::atModeHasStream(AtModeArgs &args) const
{
    if (!_stream) {
        args.print(F("Syslog is disabled"));
        return false;
    }
    return true;
}

bool SyslogPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SQ))) {
        if (atModeHasStream(args)) {
            int cmd = args.size() >= 1 ? stringlist_find_P_P(PSTR("queue|info|clear|pause"), args.get(0), '|') : -1;
            if (cmd == 3) {
                // cmd == pause
                SyslogMemoryQueue &queue = reinterpret_cast<SyslogMemoryQueue &>(_stream->_syslog._queue);
                if (queue._timer == ~1U) {
                    queue._timer = 0;
                    queueSize(queue._items.size(), queue._isAvailable());
                }
                else {
                    queue._timer = ~1U;
                }
                args.printf_P(PSTR("Queue paused=%u"), queue._timer == ~1U);
            }
            else if (cmd == 2) {
                // cmd == clear
                _stream->clearQueue();
                args.print(F("Queue cleared"));
            }
            else { // cmd == queue|info|<none/invalid>
                auto cfg = SyslogClient::getConfig();
                args.printf_P(PSTR("hostname=%s port=%u resolved=%s protocol=%s"), SyslogClient::getHostname(), cfg.getPort(), _stream->_syslog.getHostname().c_str(), protocolToString(cfg.protocol_enum));
                args.print();
                _stream->dumpQueue(args.getStream(), cmd != 1);         // show queue if cmd != info
            }
        }
        return true;
    }
#if LOGGER
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LOG))) {
        if (atModeHasStream(args)) {
            if (args.size() == 2) {
                _logger.log(_logger.getLevelFromString(args.get(0)), PSTR("%s"), args.get(1));
            }
            else if (args.requireArgs(1, 1)) {
                Logger_error(F("%s"), args.get(0));
            }
        }
        return true;
    }
#endif

    return false;
}

#endif
