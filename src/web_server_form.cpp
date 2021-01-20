/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <Form.h>
#include "Utility/ProgMemHelper.h"
#include "web_server.h"

using KFCConfigurationClasses::System;

typedef struct WebServerConfigCombo_t {
    using Type = WebServerConfigCombo_t;
    using ModeType = WebServerTypes::ModeType;

    System::FlagsConfig::ConfigFlags_t *flags;
    System::WebServerConfig::WebServerConfig_t *cfg;

    static ModeType get_webserver_mode(const Type &obj) {
        return obj.flags->is_web_server_enabled == false ? ModeType::DISABLED : (obj.cfg->is_https ? ModeType::SECURE : ModeType::UNSECURE);
    }

    static void set_webserver_mode(Type &obj, ModeType mode) {
        if (mode == ModeType::DISABLED) {
            obj.flags->is_web_server_enabled = false;
        }
        else {
            obj.flags->is_web_server_enabled = true;
            obj.cfg->is_https = (mode == ModeType::SECURE);
        }
    }

} WebServerConfigCombo_t;

void WebServerPlugin::createConfigureForm(PluginComponent::FormCallbackType type, const String &name, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (type == PluginComponent::FormCallbackType::SAVE) {
        config.setConfigDirty(true);
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &flags = System::Flags::getWriteableConfig();
    auto &cfg = System::WebServer::getWriteableConfig();
    WebServerConfigCombo_t combo = { &flags, &cfg };

    auto &ui = form.createWebUI();
    ui.setTitle(F("Remote Access Configuration"));
    ui.setContainerId(F("rconf"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &webserverGroup = form.addCardGroup(F("http"), F("Web Server"), true);

    auto modeItems = FormUI::Container::List(
        0, F("Disabled"),
        1, F("Enabled")
#if WEBSERVER_TLS_SUPPORT
        2, F("Enabled with TLS")
#endif
    );

    form.addObjectGetterSetter(FSPGM(httpmode, "mode"), combo, combo.get_webserver_mode, combo.set_webserver_mode);
    form.addFormUI(F("HTTP Server"), modeItems);
    form.addValidator(FormUI::Validator::EnumRange<System::WebServer::ModeType>());

#if WEBSERVER_TLS_SUPPORT
    form.addValidator(FormUI::Validator::Match(F("There is not enough free RAM for TLS support"), [](FormField &field) {
        return (field.getValue().toInt() != HTTP_MODE_SECURE) || (ESP.getFreeHeap() > 24000);
    }));
#endif

#if defined(ESP8266) && 1 || SPEED_BOOSTER_ENABLED
    auto performanceItems = FormUI::Container::List(
        0, F("80MHz"),
        1, F("160MHz (Recommended for TLS)")
    );

    form.addObjectGetterSetter(F("perf"), flags, flags.get_bit_is_webserver_performance_mode_enabled, flags.set_bit_is_webserver_performance_mode_enabled);
    form.addFormUI(F("Performance"), performanceItems);

#endif

    form.addCallbackSetter(F("port"), cfg.getPortAsString(), [&cfg](const String &value, FormField &field) {
        cfg.setPort(value.toInt(), cfg.isSecure()); // setMode() is executed already and cfg.is_https set
        field.setValue(cfg.getPortAsString());
    });
    form.addFormUI(FSPGM(Port, "Port"), FormUI::Type::NUMBER);
    form.addValidator(FormUI::Validator::NetworkPort(true));

#if WEBSERVER_TLS_SUPPORT

    form.add(new FormObject<File2String>(F("ssl_cert"), File2String(FSPGM(server_crt)), nullptr));
    form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key)), nullptr));

#endif
    form.addStringGetterSetter(F("btok"), System::Device::getToken, System::Device::setToken); //->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Token for Web Server, Web Sockets and Tcp2Serial"))));
    form.addValidator(FormUI::Validator::Length(System::Device::kTokenMinSize, System::Device::kTokenMaxSize));

    webserverGroup.end();

#if SECURITY_LOGIN_ATTEMPTS

    auto &loginSecurityGroup = form.addCardGroup(F("ls"), F("Login Security"), true);

    form.addObjectGetterSetter(F("lsf"), flags, flags.get_bit_is_log_login_failures_enabled, flags.set_bit_is_log_login_failures_enabled);
    form.addFormUI(F("Login Security"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("la"), cfg, cfg.get_bits_login_attempts, cfg.set_bits_login_attempts);
    form.addFormUI(F("Maximum Login Attempts"));
    cfg.addRangeValidatorFor_login_attempts(form);

    form.addObjectGetterSetter(F("ltf"), cfg, cfg.get_bits_login_timeframe, cfg.set_bits_login_attempts);
    form.addFormUI(F("Login Attempts Timeframe"), FormUI::Suffix("seconds"));
    cfg.addRangeValidatorFor_login_timeframe(form);

    form.addObjectGetterSetter(F("lst"), cfg, cfg.get_bits_login_storage_timeframe, cfg.set_bits_login_storage_timeframe);
    form.addFormUI(F("Store Login Attempts Duration"), FormUI::Suffix("days"));
    cfg.addRangeValidatorFor_login_rewrite_interval(form);

    form.addObjectGetterSetter(F("lri"), cfg, cfg.get_bits_login_rewrite_interval, cfg.set_bits_login_rewrite_interval);
    form.addFormUI(F("Rewrite Login Storage File Interval"), FormUI::Suffix("minutes"));
    cfg.addRangeValidatorFor_login_rewrite_interval(form);

    loginSecurityGroup.end();

#endif

    form.finalize();
}

