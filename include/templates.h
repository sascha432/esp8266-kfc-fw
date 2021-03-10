/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_TEMPLATES
#define DEBUG_TEMPLATES         0
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
    WebTemplate();
    virtual ~WebTemplate();

    void setSelfUri(const String &selfUri);
    void setAuthenticated(bool isAuthenticated);
    bool isAuthenticated() const;

    void setForm(FormUI::Form::BaseForm *form);
    FormUI::Form::BaseForm *getForm();

    JsonUnnamedObject *getJson();

    PrintArgs &getPrintArgs();

    virtual void process(const String &key, PrintHtmlEntitiesString &output);

    static void printSystemTime(time_t now, PrintHtmlEntitiesString &output);
    static void printUniqueId(Print &output, const String &name, int8_t dashPos = -1);
    static void printVersion(Print &output);
    static void printWebInterfaceUrl(Print &output);
    static void printModel(Print &output);
#if IOT_SSDP_SUPPORT
    static void printSSDPUUID(Print &output);
#endif

public:
    static String _aliveRedirection;

protected:
    FormUI::Form::BaseForm *_form;
    String _selfUri;
    JsonUnnamedObject *_json;
    PrintArgs _printArgs;
    bool _isAuthenticated;
};

inline WebTemplate::WebTemplate() : _form(nullptr), _json(nullptr), _isAuthenticated(false)
{
}

inline WebTemplate::~WebTemplate()
{
    if (_json) {
        __LDBG_delete(_json);
    }
    if (_form) {
        __LDBG_delete(_form);
    }
}

inline void WebTemplate::setSelfUri(const String &selfUri)
{
    _selfUri = selfUri;
}

inline void WebTemplate::setAuthenticated(bool isAuthenticated)
{
    _isAuthenticated = isAuthenticated;
}

inline bool WebTemplate::isAuthenticated() const
{
    return _isAuthenticated;
}

inline void WebTemplate::setForm(FormUI::Form::BaseForm *form)
{
    _json = nullptr; //static_cast<SettingsForm *>(form)->_json;
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
    ConfigTemplate(FormUI::Form::BaseForm *form) {
        setForm(form);
    }
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

class UpgradeTemplate : public WebTemplate {
public:
    UpgradeTemplate(const String &errorMessage) : _errorMessage(errorMessage) {}

    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
    virtual void setErrorMessage(const String &errorMessage);

protected:
    String _errorMessage;
};

class StatusTemplate : public WebTemplate {
public:
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

class LoginTemplate : public WebTemplate {
public:
    LoginTemplate(const String &errorMessage) : _errorMessage(errorMessage) {}

    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
    virtual void setErrorMessage(const String &errorMessage);

protected:
    String _errorMessage;
};

class MessageTemplate : public WebTemplate {
public:
    static constexpr uint8_t kHtmlNone = 0x00;
    static constexpr uint8_t kHtmlMessage = 0x01;
    static constexpr uint8_t kHtmlTitle = 0x02;

public:
    // the message and title may contain html code
    MessageTemplate(const String &message, const String &title = String()) : _title(title), _message(message), _titleClass(nullptr), _messageClass(nullptr), _containsHtml(kHtmlNone) {
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
    // typedef std::vector<std::pair<String, String>> TokenVector;

    SettingsForm(AsyncWebServerRequest *request);

    // void setJson(JsonUnnamedObject *json) {
    //     _json = json;
    // }

// protected:
//     friend WebTemplate;

//     JsonUnnamedObject *_json;
};
