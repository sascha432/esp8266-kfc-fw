/**
 * Author: sascha_lammers@gmx.de
 */

#include "blink_led_timer.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "templates.h"
#include "web_server.h"
#if ESP8266
#    include <Updater.h>
#    include <save_crash.h>
#endif
#include "../src/plugins/plugins.h"

#if DEBUG_WEB_SERVER_ACTION
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#if WEBSERVER_KFC_OTA

#    define U_ATMEGA 254

using KFCConfigurationClasses::System;

using namespace WebServer;

// ------------------------------------------------------------------------
// AsyncUpdateWebHandler
// ------------------------------------------------------------------------

bool AsyncUpdateWebHandler::canHandle(AsyncWebServerRequest *request)
{
    if (!request->url().equals(getURI())) {
        return false;
    }
    // emulate AsyncWebServerRequest dtor using onDisconnect callback
    request->_tempObject = new UploadStatus();
    request->onDisconnect([this, request]() {
        if (request->_tempObject) {
            delete reinterpret_cast<UploadStatus *>(request->_tempObject);
            request->_tempObject = nullptr;
        }
    });
    request->addInterestingHeader(FSPGM(Cookie));
    return true;
}

UploadStatus *AsyncUpdateWebHandler::_validateSession(AsyncWebServerRequest *request, int index)
{
    auto status = reinterpret_cast<UploadStatus *>(request->_tempObject);
    if (status && status->authenticated) {
        return status;
    }
    if (!status) {
        __LDBG_printf("upload status=nullptr");
        if (index != 0) {
            request->client()->close();
            return nullptr;
        }
        Plugin::send(500, request);
        return nullptr;
    }
    if (!Plugin::getInstance().isAuthenticated(request)) {
        __LDBG_printf("authentication failed");
        if (index != 0) {
            request->client()->close();
            return nullptr;
        }
        Plugin::send(403, request);
        return nullptr;
    }
    status->authenticated = true;
    return status;
}

void AsyncUpdateWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    auto status = _validateSession(request, 0);
    if (!status) {
        return;
    }

    if (!(request->method() & WebRequestMethod::HTTP_POST)) {
        __LDBG_printf("method not allowed");
        Plugin::send(405, request);
        return;
    }

    #ifndef ATOMIC_FS_UPDATE
        // restart FS in case it has been stopped during the update
        KFCFS_begin();
    #endif

    // auto &plugin = Plugin::getInstance();
    PrintString errorStr;
    // AsyncWebServerResponse *response = nullptr;
    if (Update.hasError()) {
        Update.printError(errorStr);
        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
        Plugin::message(request, MessageType::DANGER, errorStr, F("Firmware Upgrade Failed"));
    }
    else {
        Logger_security(F("Firmware upgrade successful"));

        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SLOW);
        Logger_notice(F("Rebooting after upgrade"));

        Plugin::message(request, MessageType::SUCCESS, F("Device is rebooting after firmware upload...."), F("Firmware Upgrade"));

        if (status) {
            switch (status->resetOptions) {
                case WebServer::ResetOptionsType::FSR:
                    if (config.erase() != Configuration::WriteResultType::SUCCESS) {
                        __DBG_printf("Failed to erase config.");
                    }
                    break;
                case WebServer::ResetOptionsType::FFS:
                    KFCFS.end();
                    if (!KFCFS.format()) {
                        __DBG_printf("FS format failed");
                    }
                    break;
                case WebServer::ResetOptionsType::FSR_FFS:
                    KFCFS.end();
                    if (!KFCFS.format()) {
                        __DBG_printf("FS format failed");
                    }
                    if (config.erase() != Configuration::WriteResultType::SUCCESS) {
                        __DBG_printf("Failed to erase config.");
                    }
                    break;
                case WebServer::ResetOptionsType::NONE:
                    break;
            }
            delete status;
            request->_tempObject = nullptr;
            status = nullptr;
        }

        Plugin::executeDelayed(request, []() {
            config.resetDevice();
        });
    }
}

#if DEBUG

static PGM_P _updateCommand2Str(int command)
{
    switch(command) {
        case U_ATMEGA:
            return PSTR("ATmega");
        case U_FLASH:
            return PSTR("Firmware");
        case U_FS:
            return PSTR("File System");
        case U_AUTH:
            return PSTR("Authentication");
    }
    return PSTR("Unknown");
}

#endif

void AsyncUpdateWebHandler::handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
    auto status = _validateSession(request, index);
    if (!status) {
        return;
    }

    if (!(request->method() & WebRequestMethod::HTTP_POST)) {
        if (index != 0) {
            request->client()->close();
        }
        return;
    }

    if (index == 0) {
        Logger_notice(F("Firmware upload started"));
        #if IOT_REMOTE_CONTROL
            RemoteControlPlugin::prepareForUpdate();
        #endif
    }

    auto &plugin = Plugin::getInstance();
    uint16_t progress = (index +  len) * 1000 / request->contentLength(); //send update every per mil
    if (status->progress != progress || final) {
        status->progress = progress;
        __LDBG_printf("progress=%u state=%u/%u%u done=%u remaining=%u size=%u clen=%u args=%u/%u/%u",
            progress, Update.isFinished(), Update.isRunning(), Update.getError(),
            Update.progress(), Update.remaining(), Update.size(), request->contentLength(),
            index, len, final
        );
        if (plugin._updateFirmwareCallback) {
            plugin._updateFirmwareCallback(index + len, request->contentLength());
        }
    }

    if (status && !status->error) {
        PrintString out;
        if (!index) {
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);

            auto resetOptionsArg = request->arg(F("rst_opts"));
            auto imageTypeArg = request->arg(FSPGM(image_type));

            if (resetOptionsArg == F("fsr")) { // clear EEPROM to restore factory settings during reboot
                status->resetOptions = WebServer::ResetOptionsType::FSR;
            }
            else if (resetOptionsArg == F("ffs")) { // format file system
                status->resetOptions = WebServer::ResetOptionsType::FFS;
            }
            else if (resetOptionsArg == F("fsr_ffs")) { // fsr + ffs
                status->resetOptions = WebServer::ResetOptionsType::FSR_FFS;
            }

            #if ESP8266
                if (imageTypeArg == F("u_flash")) {
                    // clear save crash, the stack traces are invalid after a firmware upgrade
                    // this needs to be done before the upload in case the partitioning changes
                    auto fs = SaveCrash::createFlashStorage();
                    fs.clear(SaveCrash::ClearStorageType::REMOVE_MAGIC);
                }
            #endif

            size_t size;
            uint8_t command;
            uint8_t imageType = 0;
            #if DEBUG
                PGM_P imageTypeStr = PSTR("U_UNKNOWN");
            #endif

            if (imageTypeArg == F("u_flash")) { // firmware selected
                imageType = 0;
                #if DEBUG
                    imageTypeStr = PSTR("U_FLASH");
                #endif
            }
            else if (imageTypeArg == F("u_fs")) { // filesystem selected
                imageType = 1;
                #if DEBUG
                    imageTypeStr = PSTR("U_FS");
                #endif
            }
            else if (filename.indexOf(F("spiffs")) != -1 || filename.indexOf(F("littlefs")) != -1) { // auto select
                imageType = 1;
                #if DEBUG
                    imageTypeStr = PSTR("U_FS(auto)");
                #endif
            }

            if (imageType) {
                #if (ARDUINO_ESP8266_MAJOR == 2 && ARDUINO_ESP8266_MINOR >= 6) || (ARDUINO_ESP8266_MAJOR >= 3)
                    size = ((size_t) &_FS_end - (size_t) &_FS_start);
                #else
                    size = 1048576;
                #endif
                command = U_FS;
                #ifndef ATOMIC_FS_UPDATE
                    KFCFS.end(); // end file system to avoid log files or other data being written during the update, which leads to FS corruption
                #endif
            }
            else {
                size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                command = U_FLASH;
            }
            status->command = command;
            __DBG_printf("Update starting: %s, image type %s (%d), size %d, command %s (%d)", filename.c_str(), imageTypeStr, imageType, (int)size, _updateCommand2Str(command), command);

            #if defined(ESP8266)
                Update.runAsync(true);
            #endif

            if (!Update.begin(size, command)) {
                status->error = true;
            }
        }
        {
            if (!status->error && !Update.hasError()) {
                if (Update.write(data, len) != len) {
                    status->error = true;
                }
            }

            // NOTE!: Update.size()/remaining() display the uncompressed size, but progress() only counts the compressed size

            // __LDBG_printf("is_finished=%u is_running=%u error=%u progress=%u remaining=%u size=%u", Update.isFinished(), Update.isRunning(), Update.getError(), Update.progress(), Update.remaining(), Update.size());

            if (final) {
                if (Update.end(true)) {
                    __LDBG_printf("update success: %uB", index + len);
                }
                else {
                    status->error = true;
                }
            }
            #if DEBUG
                if (status->error) {
                    // print error to debug output
                    Update.printError(DEBUG_OUTPUT);
                }
            #endif
        }
    }
}

#endif
