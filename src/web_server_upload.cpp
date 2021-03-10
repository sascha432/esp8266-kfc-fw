/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config.h"
#include "blink_led_timer.h"
#include "logger.h"
#include "templates.h"
#include "web_server.h"
#include "Updater.h"
#if IOT_REMOTE_CONTROL
#include "./plugins/remote/remote.h"
#endif

#if DEBUG_WEB_SERVER_ACTION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#define U_ATMEGA 254

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
#ifndef U_SPIFFS
#define U_SPIFFS U_FS
#endif
#endif

using KFCConfigurationClasses::System;

using namespace WebServer;

// ------------------------------------------------------------------------
// AsyncUpdateWebHandler
// ------------------------------------------------------------------------

bool AsyncUpdateWebHandler::canHandle(AsyncWebServerRequest *request)
{
    __DBG_printf("update handler method_post=%u url=%s", (request->method() & WebRequestMethod::HTTP_POST), request->url().c_str());
    if (!(request->method() & WebRequestMethod::HTTP_POST)) {
        return false;
    }
    if (request->url() != F("/update")) {
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
    return true;
}

void AsyncUpdateWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    if (!Plugin::getInstance().isAuthenticated(request)) {
        __DBG_printf("auth. failed");
        Plugin::send(403, request);
        return;
    }

    auto status = reinterpret_cast<UploadStatus *>(request->_tempObject);
    if (!status) {
        __DBG_printf("upload status=nullptr");
        Plugin::send(500, request);
        return;
    }

    auto &plugin = Plugin::getInstance();
    PrintString errorStr;
    AsyncWebServerResponse *response = nullptr;
#if STK500V1
    if (status->command == U_ATMEGA) {
        if (!status->tempFile || !status->tempFile.fullName()) {
            errorStr = F("Failed to read temporary file");
            goto errorResponse;
        }
        // get filename and close the file
        String filename = status->tempFile.fullName();
        status->tempFile.close();

        // check if singleton exists
        if (stk500v1) {
            Logger::error(F("ATmega firmware upgrade already running"));
            send(200, request, F("Upgrade already running"));
        }
        else {
            Logger_security(F("Starting ATmega firmware upgrade..."));

            stk500v1 = __LDBG_new(STK500v1Programmer, Serial);
            stk500v1->setSignature_P(PSTR("\x1e\x95\x0f"));
            stk500v1->setFile(filename);
            stk500v1->setLogging(STK500v1Programmer::LOG_FILE);

            // give it 3.5 seconds to upload the file (a few KB max)
            _Scheduler.add(3500, false, [filename](Event::CallbackTimerPtr timer) {
                if (stk500v1) {
                    // start update
                    stk500v1->begin([]() {
                        __LDBG_free(stk500v1);
                        stk500v1 = nullptr;
                        // remove temporary file
                        KFCFS::unlink(filename);
                    });
                }
                else {
                    // write something to the logfile
                    Logger::error(F("Cannot start ATmega firmware upgrade"));
                    // remove temporary file
                    KFCFS::unlink(filename);
                }
            });

            response = request->beginResponse(302);
            HttpHeaders httpHeaders(false);
            httpHeaders.add<HttpLocationHeader>(String('/') + FSPGM(serial_console_html));
            httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::CLOSE);
            httpHeaders.setAsyncWebServerResponseHeaders(response);
            request->send(response);
        }
    }
    else
#endif
    if (Update.hasError()) {
        // TODO check if we need to restart the file system
        // KFCFS.begin();

        Update.printError(errorStr);
#if STK500V1
errorResponse: ;
#endif
        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
        Logger_error(F("Firmware upgrade failed: %s"), errorStr.c_str());

        String message = F("<h2>Firmware update failed with an error:<br></h2><h3>");
        message += errorStr;
        message += F("</h3>");

        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        if (!plugin._sendFile(String('/') + FSPGM(update_fw_html), String(), httpHeaders, plugin._clientAcceptsGzip(request), true, request, new UpgradeTemplate(message))) {
            // fallback if the file system cannot be read anymore
            Plugin::send(200, request, message);
        }
    }
    else {
        Logger_security(F("Firmware upgrade successful"));

#if IOT_CLOCK
        // turn dispaly off since the update will take a few seconds and freeze the clock
        ClockPlugin::getInstance()._setBrightness(0);
#endif

        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SLOW);
        Logger_notice(F("Rebooting after upgrade"));

        String location;
        switch(status->command) {
            case U_FLASH: {
#if DEBUG
                auto hash = request->arg(F("elf_hash"));
                __LDBG_printf("hash %u %s", hash.length(), hash.c_str());
                if (hash.length() == System::Firmware::getElfHashHexSize()) {
                    System::Firmware::setElfHashHex(hash.c_str());
                    config.write();
                }
#endif
                WebTemplate::_aliveRedirection = String(FSPGM(update_fw_html, "update-fw.html")) + F("#u_flash");
                location += '/';
                location += FSPGM(rebooting_html);
            } break;
            case U_SPIFFS:
                WebTemplate::_aliveRedirection = String(FSPGM(update_fw_html)) + F("#u_spiffs");
                location += '/';
                location += FSPGM(rebooting_html);
                break;
        }

        // executeDelayed sets a new ondisconnect callback
        // delete upload object before
        if (status) {
            delete status;
            request->_tempObject = nullptr;
            status = nullptr;
        }

        // execute restart with a delay to finish the current request
        Plugin::executeDelayed(request, []() {
            config.restartDevice();
        });

        response = request->beginResponse(302);
        HttpHeaders httpHeaders(false);
        httpHeaders.add<HttpLocationHeader>(location);
        httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::CLOSE);
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
}

void AsyncUpdateWebHandler::handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!Plugin::getInstance().isAuthenticated(request)) {
        __DBG_printf("auth. failed");
        Plugin::send(403, request);
        return;
    }
    UploadStatus *status = reinterpret_cast<UploadStatus *>(request->_tempObject);
    if (!status) {
        __DBG_printf("stastus=nullptr");
        Plugin::send(500, request);
        return;
    }

    if (index == 0) {
        Logger_notice(F("Firmware upload started"));
#if IOT_REMOTE_CONTROL
        RemoteControlPlugin::prepareForUpdate();
#endif
    }

    auto &plugin = Plugin::getInstance();
    if (plugin._updateFirmwareCallback) {
        plugin._updateFirmwareCallback(index + len, request->contentLength());
    }

    if (status && !status->error) {
        PrintString out;
        if (!index) {
            BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);

            size_t size;
            uint8_t command;
            uint8_t imageType = 0;

            if (request->arg(FSPGM(image_type)) == F("u_flash")) { // firmware selected
                imageType = 0;
            }
            else if (request->arg(FSPGM(image_type)) == F("u_spiffs")) { // spiffs selected
                imageType = 1;
            }
#if STK500V1
            else if (request->arg(FSPGM(image_type)) == F("u_atmega")) { // atmega selected
                imageType = 3;
            }
            else if (filename.indexOf(F(".hex")) != -1) { // auto select
                imageType = 3;
            }
#endif
            else if (filename.indexOf(F("spiffs")) != -1) { // auto select
                imageType = 2;
            }

#if STK500V1
            if (imageType == 3) {
                status->command = U_ATMEGA;
                status->tempFile = KFCFS.open(FSPGM(stk500v1_tmp_file), fs::FileOpenMode::write);
                __LDBG_printf("ATmega fw temp file %u, filename %s", (bool)status->tempFile, String(FSPGM(stk500v1_tmp_file)).c_str());
            }
            else
#endif
            {
                if (imageType) {
#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
                    size = ((size_t) &_FS_end - (size_t) &_FS_start);
#else
                    size = 1048576;
#endif
                    command = U_SPIFFS;
                } else {
                    size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                    command = U_FLASH;
                }
                status->command = command;
                debug_printf_P(PSTR("Update Start: %s, image type %d, size %d, command %d\n"), filename.c_str(), imageType, (int)size, command);

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
            if (!status->tempFile) {
                status->error = true;
            }
            else if (status->tempFile.write(data, len) != len) {
                status->error = true;
            }
#if DEBUG_WEB_SERVER
            if (final) {
                if (status->tempFile) {
                    __DBG_printf("upload success: %uB", status->tempFile.size());
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

            __DBG_printf("is_finished=%u is_running=%u error=%u progress=%u remaining=%u size=%u", Update.isFinished(), Update.isRunning(), Update.getError(), Update.progress(), Update.remaining(), Update.size());

            if (final) {
                if (Update.end(true)) {
                    __LDBG_printf("update success: %uB", index + len);
                } else {
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

