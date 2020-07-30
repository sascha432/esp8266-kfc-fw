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

    void setForm(Form *form);
    Form *getForm();

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
    Form *_form;
    JsonUnnamedObject *_json;
    PrintArgs _printArgs;
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
    // typedef std::vector<std::pair<String, String>> TokenVector;

    SettingsForm(AsyncWebServerRequest *request);

    // void setJson(JsonUnnamedObject *json) {
    //     _json = json;
    // }

// protected:
//     friend WebTemplate;

//     JsonUnnamedObject *_json;
};
