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
    static void printSSDPUUID(Print &output);

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

class PasswordTemplate : public LoginTemplate {
public:
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

class File2String {
public:
    File2String(const String &filename);
    String toString();
    void fromString(const String &value);

private:
    String _filename;
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
