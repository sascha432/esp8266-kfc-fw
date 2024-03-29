/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_TEMPLATES
#   define DEBUG_TEMPLATES (0 || defined(DEBUG_ALL))
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <PrintHtmlEntitiesString.h>
#include <PrintArgs.h>
#include <pgmspace.h>
#include <map>
#include <KFCForms.h>
#include <KFCJson.h>
#include "kfc_fw_config.h"

class SettingsForm;

class WebTemplate {
public:
    enum class AuthType : uint8_t {
        NONE,
        AUTH,
        NO_AUTH
    };

public:
    WebTemplate(AuthType authenticated = AuthType::NONE);
    virtual ~WebTemplate();

    void setSelfUri(const String &selfUri);
    void setAuthenticated(bool isAuthenticated);
    bool isAuthenticated() const;
    bool isAuthenticationSet() const;

    void setForm(FormUI::Form::BaseForm *form);
    FormUI::Form::BaseForm *getForm();

    // JsonUnnamedObject *getJson() {
    //     return _json;
    // }

    PrintArgs &getPrintArgs();

    virtual void process(const String &key, PrintHtmlEntitiesString &output);

    static void printSystemTime(time_t now, PrintHtmlEntitiesString &output);
    static void printUniqueId(Print &output, const String &name, int8_t dashPos = -1);
    static void printVersion(Print &output, bool full = true);
    static void printWebInterfaceUrl(Print &output);
    static void printModel(Print &output);
    static void printFileSystemInfo(Print &output);
#if IOT_SSDP_SUPPORT
    static void printSSDPUUID(Print &output);
#endif

public:
    static String _aliveRedirection;

protected:
    FormUI::Form::BaseForm *_form;
    String _selfUri;
    // JsonUnnamedObject *_json;
    PrintArgs _printArgs;
    AuthType _isAuthenticated;
};

inline WebTemplate::WebTemplate(AuthType authenticated) :
    _form(nullptr),
    // _json(nullptr),
    _isAuthenticated(authenticated)
{
}

inline WebTemplate::~WebTemplate()
{
    // if (_json) {
    //     delete _json;
    // }
    if (_form) {
        delete _form;
    }
}

inline void WebTemplate::setSelfUri(const String &selfUri)
{
    _selfUri = selfUri;
}

inline void WebTemplate::setAuthenticated(bool isAuthenticated)
{
    _isAuthenticated = isAuthenticated ? AuthType::AUTH : AuthType::NO_AUTH;
}

inline bool WebTemplate::isAuthenticated() const
{
    return _isAuthenticated == AuthType::AUTH;
}

inline bool WebTemplate::isAuthenticationSet() const
{
    return _isAuthenticated != AuthType::NONE;
}

inline void WebTemplate::setForm(FormUI::Form::BaseForm *form)
{
    // _json = nullptr; //static_cast<SettingsForm *>(form)->_json;
    _form = form;
}

inline FormUI::Form::BaseForm *WebTemplate::getForm()
{
    return _form;
}

inline PrintArgs &WebTemplate::getPrintArgs()
{
    return _printArgs;
}


class ConfigTemplate : public WebTemplate {
public:
    ConfigTemplate(FormUI::Form::BaseForm *form, bool isAuthenticated) {
        setForm(form);
        setAuthenticated(isAuthenticated);
    }
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

class StatusTemplate : public WebTemplate {
public:
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

class LoginTemplate : public WebTemplate {
public:
    LoginTemplate(const String &errorMessage) :
        _errorMessage(errorMessage)
    {
        setAuthenticated(false);
    }

    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
    virtual void setErrorMessage(const String &errorMessage);

protected:
    String _errorMessage;
};

#define MESSAGE_TEMPLATE_AUTO_RELOAD(timeout)       "<span class=\"auto-reload\" data-timeout=\"" _STRINGIFY(timeout) "\"><img src=\"images/spinner.gif\" class=\"p-1 ml-3\" width=\"36\" height=\"36\"></span>"

class MessageTemplate : public WebTemplate {
public:
    static constexpr uint8_t kHtmlNone = 0x00;
    static constexpr uint8_t kHtmlMessage = 0x01;
    static constexpr uint8_t kHtmlTitle = 0x02;

public:
    // the message and title may contain html code
    MessageTemplate(const String &message, const String &title = String(), bool isAuthenticated = false) :
        WebTemplate(isAuthenticated ? AuthType::AUTH : AuthType::NO_AUTH),
        _title(title),
        _message(message),
        _titleClass(nullptr),
        _messageClass(nullptr),
        _containsHtml(kHtmlNone)
    {
        checkForHtml();
    }

    void setTitle(const String &title) {
        _title = title;
    }
    void setMessage(const String &message) {
        _message = message;
    }

    void setTitleClass(const __FlashStringHelper *titleClass) {
        _titleClass = titleClass;
    }
    void setMessageClass(const __FlashStringHelper *messageClass) {
        _messageClass = messageClass;
    }

    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;

    inline bool containsHtml() const {
        return _containsHtml != kHtmlNone;
    }

    inline static const __FlashStringHelper *getDefaultAlertClass() {
        return F("success");
    }

private:
    void checkForHtml();

protected:
    String _title;
    String _message;
    const __FlashStringHelper *_titleClass;
    const __FlashStringHelper *_messageClass;
    uint8_t _containsHtml;
};

class NotFoundTemplate : public MessageTemplate {
public:
    NotFoundTemplate(uint16_t code, const String &message, const String &title = String()) : MessageTemplate(message, title), _code(code) {}

    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;

protected:
    uint16_t _code;
};

class PasswordTemplate : public LoginTemplate {
public:
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

template<typename _Ta = const __FlashStringHelper *>
class File2String {
public:
    File2String(_Ta filename) : _filename(filename) {
    }
    String toString() {
        return KFCFS.open(_filename, fs::FileOpenMode::read).readString();
    }
    void fromString(const String &value) {
        KFCFS.open(_filename, fs::FileOpenMode::write).print(value);
    }
    void fromString(const __FlashStringHelper *value) {
        KFCFS.open(_filename, fs::FileOpenMode::write).print(value);
    }

private:
    _Ta _filename;
};

class SettingsForm : public FormUI::Form::BaseForm {
public:
    SettingsForm(AsyncWebServerRequest *request);
};
