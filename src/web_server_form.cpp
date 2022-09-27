/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <Form.h>
#include "Utility/ProgMemHelper.h"
#include "web_server.h"

using KFCConfigurationClasses::System;

struct WebServerConfigCombo
{
    using Type = WebServerConfigCombo;
    using ModeType = WebServerTypes::ModeType;

    System::FlagsConfig::ConfigFlags_t *flags;
    System::WebServerConfig::WebServerConfig_t *cfg;

    WebServerConfigCombo(System::FlagsConfig::ConfigFlags_t *_flags, System::WebServerConfig::WebServerConfig_t *_cfg) :
        flags(_flags),
        cfg(_cfg)
    {
    }

    static ModeType get_webserver_mode(const Type &obj)
    {
        return obj.flags->is_web_server_enabled == false ? ModeType::DISABLED : (obj.cfg->is_https ? ModeType::SECURE : ModeType::UNSECURE);
    }

    static void set_webserver_mode(Type &obj, ModeType mode)
    {
        if (mode == ModeType::DISABLED) {
            obj.flags->is_web_server_enabled = false;
        } else {
            obj.flags->is_web_server_enabled = true;
            obj.cfg->is_https = (mode == ModeType::SECURE);
        }
    }
};

using namespace WebServer;

void Plugin::createConfigureForm(FormCallbackType type, const String &name, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (type == FormCallbackType::SAVE) {
        config.setConfigDirty(true);
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &flags = System::Flags::getWriteableConfig();
    auto &cfg = System::WebServer::getWriteableConfig();
    WebServerConfigCombo combo(&flags, &cfg);

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

    form.addObjectGetterSetter(F("mode"), combo, combo.get_webserver_mode, combo.set_webserver_mode);
    form.addFormUI(F("HTTP Server"), modeItems);
    form.addValidator(FormUI::Validator::EnumRange<System::WebServer::ModeType>());

    #if WEBSERVER_TLS_SUPPORT
        form.addValidator(FormUI::Validator::Match(F("There is not enough free RAM for TLS support"), [](FormField &field) {
            return (field.getValue().toInt() != HTTP_MODE_SECURE) || (ESP.getFreeHeap() > 24000);
        }));
    #endif

    form.addCallbackSetter(F("port"), cfg.getPortAsString(), [&cfg](const String &value, FormField &field) {
        cfg.setPort(value.toInt());
        field.setValue(cfg.getPortAsString());
    });
    form.addFormUI(FormUI::Type::NUMBER, FSPGM(Port), FormUI::PlaceHolder(System::WebServer::kPortDefault));
    form.addValidator(FormUI::Validator::NetworkPort(true));

    #if WEBSERVER_TLS_SUPPORT

        form.add(new FormObject<File2String>(F("ssl_cert"), File2String(FSPGM(server_crt)), nullptr));
        form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key)), nullptr));

    #endif

    //TODO causes crash?
    form.addStringGetterSetter(F("btok"), System::Device::getToken, System::Device::setToken);
    form.addFormUI(F("Remote Authentication Token"));
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

