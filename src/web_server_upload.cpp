/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config.h"
#include "blink_led_timer.h"
#include "logger.h"
#include "templates.h"
#include "web_server.h"
#if ESP8266
#include <Updater.h>
#include <save_crash.h>
#endif
#include "../src/plugins/plugins.h"

#if DEBUG_WEB_SERVER_ACTION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if WEBSERVER_KFC_OTA

#define U_ATMEGA 254

using KFCConfigurationClasses::System;

using namespace WebServer;

// ------------------------------------------------------------------------
// AsyncUpdateWebHandler
// ------------------------------------------------------------------------

bool AsyncUpdateWebHandler::canHandle(AsyncWebServerRequest *request)
{
    __LDBG_printf("update handler method_post=%u url=%s", (request->method() & WebRequestMethod::HTTP_POST), request->url().c_str());
    if (request->url() != getURI()) {
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

    // auto &plugin = Plugin::getInstance();
    PrintString errorStr;
    // AsyncWebServerResponse *response = nullptr;
    #if STK500V1
        if (status->command == U_ATMEGA) {
            if (!request->_tempFile || !request->_tempFile.fullName()) {
                errorStr = F("Failed to read temporary file");
                goto errorResponse;
            }
            // get filename and close the file
            String filename = request->_tempFile.fullName();
            request->_tempFile.close();

            // check if singleton exists
            if (stk500v1) {
                errorStr = F("ATmega firmware upgrade already running");
                Logger_error(errorStr);
                goto errorResponse;
            }
            else {
                Logger_security(F("Starting ATmega firmware upgrade..."));

                stk500v1 = new STK500v1Programmer(Serial);
                stk500v1->setSignature_P(PSTR("\x1e\x95\x0f"));
                stk500v1->setFile(filename);
                stk500v1->setLogging(STK500v1Programmer::LOG_FILE);

                // give it 3.5 seconds to upload the file (a few KB max)
                _Scheduler.add(3500, false, [filename](Event::CallbackTimerPtr timer) {
                    if (stk500v1) {
                        // start update
                        stk500v1->begin([filename]() {
                            free(stk500v1);
                            stk500v1 = nullptr;
                            // remove temporary file
                            KFCFS.remove(filename);
                        });
                    }
                    else {
                        // write something to the logfile
                        Logger_error(F("Cannot start ATmega firmware upgrade"));
                        // remove temporary file
                        KFCFS.remove(filename);
                    }
                });

                HttpHeaders headers(false);
                headers.replace<HttpConnectionHeader>(HttpConnectionHeader::ConnectionType::CLOSE);
                request->send(HttpLocationHeader::redir(request, String('/') + FSPGM(serial_console_html), headers));

                // auto response = request->beginResponse(302);
                // HttpHeaders httpHeaders(false);
                // httpHeaders.add<HttpLocationHeader>(String('/') + FSPGM(serial_console_html));
                // httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::ConnectionType::CLOSE);
                // httpHeaders.setResponseHeaders(response);
                // request->send(response);
            }
        }
        else
    #endif
    if (Update.hasError()) {
        // TODO check if we need to restart the file system
        // KFCFS_begin();

        Update.printError(errorStr);
        #if STK500V1
        errorResponse: ;
        #endif
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
            PGM_P imageTypeStr = PSTR("U_UNKNOWN");

            if (imageTypeArg == F("u_flash")) { // firmware selected
                imageType = 0;
                imageTypeStr = PSTR("U_FLASH");
            }
            else if (imageTypeArg == F("u_fs")) { // filesystem selected
                imageType = 1;
                imageTypeStr = PSTR("U_FS");
            }
            #if STK500V1
                else if (imageTypeArg == F("u_atmega")) { // atmega selected
                    imageType = 3;
                    imageTypeStr = PSTR("U_ATMEGA");
                }
                else if (filename.indexOf(F(".hex")) != -1) { // auto select
                    imageType = 3;
                    imageTypeStr = PSTR("U_ATMEGA(auto)");
                }
            #endif
            else if (filename.indexOf(F("spiffs")) != -1 || filename.indexOf(F("littlefs")) != -1) { // auto select
                imageType = 1;
                imageTypeStr = PSTR("U_FS(auto)");
            }

            #if STK500V1
                if (imageType == 3) {
                    status->command = U_ATMEGA;
                    request->_tempFile = KFCFS.open(FSPGM(stk500v1_tmp_file), fs::FileOpenMode::write);
                    __LDBG_printf("ATmega fw temp file %u, filename %s", (bool)request->tempFile, String(FSPGM(stk500v1_tmp_file)).c_str());
                }
                else
            #endif
            {
                if (imageType) {
                    #if (ARDUINO_ESP8266_MAJOR == 2 && ARDUINO_ESP8266_MINOR >= 6) || (ARDUINO_ESP8266_MAJOR >= 3)
                        size = ((size_t) &_FS_end - (size_t) &_FS_start);
                    #else
                        size = 1048576;
                    #endif
                    command = U_FS;
                    #ifndef ATOMIC_FS_UPDATE
                        KFCFS.end();
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
        }
        #if STK500V1
            if (status->command == U_ATMEGA) {
                if (!request->_tempFile) {
                    status->error = true;
                }
                else if (request->_tempFile.write(data, len) != len) {
                    status->error = true;
                }
                #if DEBUG_WEB_SERVER
                    if (final) {
                        if (request->_tempFile) {
                            __DBG_printf("upload success: %uB", request->_tempFile.size());
                        else {
                            __DBG_printf("upload error: tempFile = false");
                        }
                    }
                #endif
            }
            else
        #endif
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
            if (status->error) {
                #if DEBUG
                    // print error to debug output
                    Update.printError(DEBUG_OUTPUT);
                #endif
            }
        }
    }
}

#endif
