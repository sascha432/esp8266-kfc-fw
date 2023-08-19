/**
 * Author: sascha_lammers@gmx.de
 */

#include "syslog_plugin.h"
#include <SyslogFactory.h>

#if DEBUG_SYSLOG
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

SyslogPlugin syslogPlugin;

using Plugins = KFCConfigurationClasses::PluginsType;
using namespace KFCConfigurationClasses::Plugins::SyslogConfigNS;
using KFCConfigurationClasses::System;

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

SyslogPlugin::SyslogPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SyslogPlugin)),
    _syslog(nullptr)
{
    REGISTER_PLUGIN(this, "SyslogPlugin");
}

void SyslogPlugin::setup(SetupModeType mode, const DependenciesPtr &dependencies)
{
    __LDBG_printf("mode %u", mode);
    _begin();
}

void SyslogPlugin::reconfigure(const String &source)
{
    _begin();
}

void SyslogPlugin::shutdown()
{
    _kill(500);
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
    if (_syslog->deliverQueue() == 0) {
        timer->disarm();
    }
}

void SyslogPlugin::_begin()
{
    _end();

    if (SyslogClient::isEnabled()) {

        auto cfg = SyslogClient::getConfig();
        String hostname = SyslogClient::getHostname();
        uint16_t port = cfg.getPort();
        if (hostname.trim().length() && port) {
            bool zeroconf = config.hasZeroConf(hostname);

            _syslog = SyslogFactory::create(_lock, System::Device::getName(), cfg._get_enum_protocol(), zeroconf ? emptyString : hostname, static_cast<uint16_t>(zeroconf ? SyslogFactory::kZeroconfPort : port));
            #if LOGGER
                _logger.setSyslog(_syslog);
            #endif

            __LDBG_printf("zeroconf=%u port=%u", zeroconf, port);
            if (zeroconf) {
                auto syslog = _syslog; // pass a copy of the pointer in case _syslog gets destroyed before the callback
                config.resolveZeroConf(getFriendlyName(), hostname, port, [syslog](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) {
                    getInstance()._zeroConfCallback(syslog, hostname, address, port, type);
                });
            }

        }
        else {
            __LDBG_printf("hostname=%s port=%u", hostname.c_str(), port);
        }
    }
}

void SyslogPlugin::_zeroConfCallback(const Syslog *syslog, const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type)
{
    __LDBG_printf("zeroconf callback host=%s address=%s port=%u type=%u _syslog=%p syslog=%p", hostname.c_str(), address.toString().c_str(), port, type, _syslog, syslog);
    // make sure that the callback belongs to the same syslog
    MUTEX_LOCK_BLOCK(_lock) {
        if (_syslog == syslog) {
            _syslog->setupZeroConf(hostname, address, port);
        }
    }
}

void SyslogPlugin::_end()
{
    MUTEX_LOCK_BLOCK(_lock) {
        if (_syslog) {
            _Timer(_timer).remove();
            #if LOGGER
                _logger.setSyslog(nullptr);
            #endif
            delete _syslog;
            _syslog = nullptr;
        }
    }
}

void SyslogPlugin::_waitForQueue(uint32_t timeout)
{
    if (_syslog) {
        uint32_t start = millis();
        while(get_time_since(start, millis()) < timeout) {
            if (!_syslog->deliverQueue()) {
                break;
            }
            delay(100); // let system process tcp buffers etc...
        }
    }
}

void SyslogPlugin::_kill(uint32_t timeout)
{
    if (_syslog) {
        _waitForQueue(timeout);
        _end();
    }
}

void SyslogPlugin::getStatus(Print &output)
{
    #if SYSLOG_SUPPORT
        if (_syslog) {
            auto proto = protocolToString(Plugins::SyslogClient::getConfig()._get_enum_protocol());
            if (!proto) {
                output.print(FSPGM(Disabled));
                return;
            }
            output.print(proto);

            if (_syslog->getPort() == SyslogFactory::kZeroconfPort) {
                output.print(F(" @ resolving zeroconf"));
            }
            else {
                output.printf_P(PSTR(" @ %s:%u"), _syslog->getHostname().c_str(), _syslog->getPort());
            }
            auto size = _syslog->getQueueSize();
            if (!size) {
                output.print(F(" - queue empty"));
            }
            else {
                output.printf_P(PSTR(" - %u message(s) queued, state %s"), size, _syslog->isSending() ? PSTR("sending") : PSTR("waiting"));
            }
            if (_syslog->getDropped()) {
                output.printf_P(PSTR(", %u dropped"), _syslog->getDropped());
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

#if AT_MODE_HELP_SUPPORTED

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

#endif

bool SyslogPlugin::atModeHasStream(AtModeArgs &args) const
{
    if (!_syslog) {
        args.print(F("Syslog is disabled"));
        return false;
    }
    return true;
}

bool SyslogPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SQ))) {
        if (atModeHasStream(args)) {
            if (args.equalsIgnoreCase(0, F("clear"))) {
                _syslog->clear();
                args.print(F("Queue cleared"));
            }
            else {
                auto cfg = SyslogClient::getConfig();
                args.print(F("hostname=%s port=%u resolved=%s protocol=%s queue=%u dropped=%u"), SyslogClient::getHostname(), cfg.getPort(), _syslog->getHostname().c_str(), protocolToString(cfg._get_enum_protocol()), _syslog->getQueueSize(), _syslog->getDropped());
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
