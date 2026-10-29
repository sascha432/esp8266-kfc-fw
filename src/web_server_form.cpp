/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <Form.h>
#include "Utility/ProgMemHelper.h"
#include "web_server.h"

using KFCConfigurationClasses::System;

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

    // the field is read and stored after this function returned (validate() of a POST): capture the writeable
    // configs, never an object on this stack (a local helper object here crashed every save with Exception (3))
    form.addCallbackGetterSetter<System::WebServer::ModeType>(F("mode"), [&flags, &cfg](System::WebServer::ModeType &mode, FormUI::Field::BaseField &, bool store) {
        using ModeType = System::WebServer::ModeType;
        if (store) {
            flags.is_web_server_enabled = (mode != ModeType::DISABLED);
            if (mode != ModeType::DISABLED) {
                cfg.is_https = (mode == ModeType::SECURE);
            }
        }
        else {
            mode = !flags.is_web_server_enabled ? ModeType::DISABLED : (cfg.is_https ? ModeType::SECURE : ModeType::UNSECURE);
        }
        return true;
    }, FormUI::InputFieldType::TEXT);
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
    form.addFormUI(FormUI::Type::NUMBER, F("Port"), FormUI::PlaceHolder(System::WebServer::kPortDefault));
    form.addValidator(FormUI::Validator::NetworkPort(true));

    #if WEBSERVER_TLS_SUPPORT

        form.add(new FormObject<File2String>(F("ssl_cert"), File2String(FSPGM(server_crt)), nullptr));
        form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key)), nullptr));

    #endif

    webserverGroup.end();

    #if SECURITY_LOGIN_ATTEMPTS

        auto &loginSecurityGroup = form.addCardGroup(F("ls"), F("Login Security"), true);

        form.addObjectGetterSetter(F("lsf"), flags, flags.get_bit_is_log_login_failures_enabled, flags.set_bit_is_log_login_failures_enabled);
        form.addFormUI(F("Login Security"), FormUI::BoolItems());

        form.addObjectGetterSetter(F("la"), cfg, cfg.get_bits_login_attempts, cfg.set_bits_login_attempts);
        form.addFormUI(F("Maximum Login Attempts"));
        cfg.addRangeValidatorFor_login_attempts(form);

        form.addObjectGetterSetter(F("ltf"), cfg, cfg.get_bits_login_timeframe, cfg.set_bits_login_attempts);
        form.addFormUI(F("Login Attempts Timeframe"), FormUI::Suffix(F("seconds")));
        cfg.addRangeValidatorFor_login_timeframe(form);

        form.addObjectGetterSetter(F("lst"), cfg, cfg.get_bits_login_storage_timeframe, cfg.set_bits_login_storage_timeframe);
        form.addFormUI(F("Store Login Attempts Duration"), FormUI::Suffix(F("days")));
        cfg.addRangeValidatorFor_login_rewrite_interval(form);

        form.addObjectGetterSetter(F("lri"), cfg, cfg.get_bits_login_rewrite_interval, cfg.set_bits_login_rewrite_interval);
        form.addFormUI(F("Rewrite Login Storage File Interval"), FormUI::Suffix(F("minutes")));
        cfg.addRangeValidatorFor_login_rewrite_interval(form);

        loginSecurityGroup.end();

    #endif

    form.finalize();
}

