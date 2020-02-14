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
#include "kfc_fw_config.h"

PROGMEM_STRING_DECL(Not_supported);

class SettingsForm;

class WebTemplate {
public:
    WebTemplate();
    virtual ~WebTemplate();

    void setForm(Form *form);
    Form *getForm();

    PrintArgs &getPrintArgs() {
        return _printArgs;
    }

    virtual void process(const String &key, PrintHtmlEntitiesString &output);

protected:
    Form *_form;
    PrintArgs _printArgs;
};

class EmptyTemplate : public WebTemplate {
public:
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

class ConfigTemplate : public WebTemplate {
public:
    ConfigTemplate() {
    }
    ConfigTemplate(Form *form) : WebTemplate() {
        setForm(form);
    }
    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
};

class UpgradeTemplate : public WebTemplate {
public:
    UpgradeTemplate();
    UpgradeTemplate(const String &errorMessage);

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
    LoginTemplate();
    LoginTemplate(const String &errorMessage);

    virtual void process(const String &key, PrintHtmlEntitiesString &output) override;
    virtual void setErrorMessage(const String &errorMessage);

protected:
    String _errorMessage;
};

class PasswordTemplate : public LoginTemplate {
public:
    PasswordTemplate(const String &errorMessage);

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

class SettingsForm : public Form {
public:
    typedef std::vector<std::pair<String, String>> TokenVector;

    SettingsForm(AsyncWebServerRequest *request);

    inline TokenVector &getTokens() {
        return _tokens;
    }

protected:
    FormData _data;
    TokenVector _tokens;
};

class WifiSettingsForm : public SettingsForm {
public:
    WifiSettingsForm(AsyncWebServerRequest *request);
};

class NetworkSettingsForm : public SettingsForm {
public:
    NetworkSettingsForm(AsyncWebServerRequest *request);
};

class PasswordSettingsForm : public SettingsForm {
public:
    PasswordSettingsForm(AsyncWebServerRequest *request);
};

class PinsSettingsForm : public SettingsForm {
public:
    PinsSettingsForm(AsyncWebServerRequest *request);
};
