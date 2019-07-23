/**
 * Author: sascha_lammers@gmx.de
 */

#if FILE_MANAGER

#include "file_manager.h"
#include <Buffer.h>
#include <PrintString.h>
#include <ProgmemStream.h>
#include <HttpHeaders.h>
#include "async_web_response.h"
#include "async_web_handler.h"
#include "progmem_data.h"
#include "misc.h"
#include "logger.h"
#include "plugins.h"

#if DEBUG_FILE_MANAGER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(success, "success");
PROGMEM_STRING_DEF(failure, "failure");
PROGMEM_STRING_DEF(file_manager_base_uri, "/file_manager/");
PROGMEM_STRING_DEF(upload, "upload");
PROGMEM_STRING_DEF(upload_file, "upload_file");
PROGMEM_STRING_DEF(ERROR_, "ERROR:");


void file_manager_upload_handler(AsyncWebServerRequest *request) {
    if (request->_tempObject) {
        FileManager fm(request, web_server_is_authenticated(request), FSPGM(upload));
        fm.handleRequest();
    } else {
        request->send(403);
    }
}

void file_manager_install_web_server_hook() {
    if (get_web_server_object()) {
        String uploadDir = FSPGM(file_manager_base_uri);
        uploadDir += FSPGM(upload);
        web_server_add_handler(new AsyncFileUploadWebHandler(uploadDir, file_manager_upload_handler));
        web_server_add_handler(new FileManagerWebHandler(FSPGM(file_manager_base_uri)));
    }
}

FileManager::FileManager() {
    _request = nullptr;
    _response = nullptr;
    _isAuthenticated = false;
    _errors = 0;
}

FileManager::FileManager(AsyncWebServerRequest *request, bool isAuthenticated, const String &uri) : FileManager()  {
    _uri = uri;
    _request = request;
    _isAuthenticated = isAuthenticated;
    _errors = _isAuthenticated ? 0 : 1;
}

void FileManager::_sendResponse(uint16_t httpStatusCode) {
    _debug_printf_P(PSTR("_sendResponse(%d)\n"), httpStatusCode);
    if (httpStatusCode == 0) {
        httpStatusCode = _isValidData() ? 500 : 200;
    }
    if (!_response) {
        _response = _request->beginResponse(httpStatusCode);
    }
    _headers.setWebServerResponseHeaders(_response);
    _response->setCode(httpStatusCode);
    _debug_printf_P(PSTR("filemanager:%s response %d\n"), _uri.c_str(), httpStatusCode);
    _request->send(_response);
}

bool FileManager::_isValidData() {
    _debug_printf_P(PSTR("isValidData() = %d (%d)\n"), _errors == 0, _errors);
    return _errors == 0;
}

bool FileManager::_requireAuthentication() {
    if (!_isAuthenticated) {
        _errors++;
    }
    _debug_printf_P(PSTR("requireAuthentication() = %d (%d)\n"), _isAuthenticated, _errors);
    return _isAuthenticated;
}

String FileManager::_requireDir(const String &name, bool mustExist) {
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        _debug_printf_P(PSTR("%s is not set\n"), name.c_str());
    }
    String path = _request->arg(name);
    if (!path.length()) {
        _errors++;
        _debug_printf_P(PSTR("%s is empty\n"), name.c_str());
    }
    _debug_printf_P(PSTR("requireDir(%s) = %s (%d)\n"), name.c_str(), path.c_str(), _errors);
    return path;
}

AsyncDirWrapper FileManager::_requireDir(const String &name) {
    String path = _requireDir(name, true);
    if (!path.length()) {
        return AsyncDirWrapper();
    }
    AsyncDirWrapper dir = AsyncDirWrapper(path);
    if (!dir.isValid()) {
        _errors++;
        _debug_printf_P(PSTR("Directory %s does not exist or empty\n"), path.c_str());
    }
    return dir;
}

String FileManager::_requireFile(const String &name, bool mustExists) {
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        _debug_printf_P(PSTR("%s is not set\n"), name.c_str());
    }
    String path = _request->arg(name);
    if (!path.length()) {
        _errors++;
        _debug_printf_P(PSTR("%s is empty\n"), name.c_str());
    }
    else if (!SPIFFSWrapper::exists(path)) {
        _errors++;
        _debug_printf_P(PSTR("File %s not found\n"), path.c_str());
        path = String();
    }
    _debug_printf_P(PSTR("requireFile(%s) = %s (%d)\n"), name.c_str(), path.c_str(), _errors);
    return path;
}

File FileManager::_requireFile(const String &name) {
    String path = _requireFile(name, true);
    if (!path.length()) {
        return File();
    }
    return SPIFFSWrapper::open(path, "r");
}

String FileManager::_requireArgument(const String &name) {
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        _debug_printf_P(PSTR("%s is not set\n"), name.c_str());
    }
    String value = _request->arg(name);
    if (!value.length()) {
        _errors++;
        _debug_printf_P(PSTR("%s is empty\n"), name.c_str());
    }
    return value;
}

String FileManager::_getArgument(const String &name) {
    return _request->arg(name);
}

void FileManager::handleRequest() {

    _headers.addNoCache(true);

    _debug_printf_P(PSTR("is authenticated %d request uri %s\n"), _isAuthenticated, _uri.c_str());

    if (!_isAuthenticated) {
        _sendResponse(403);
    } else if (_uri == F("list")) {
        _sendResponse(list());
    } else if (_uri == F("mkdir")) {
        _sendResponse(mkdir());
    } else if (_uri == FSPGM(upload)) {
        _sendResponse(upload());
    } else if (_uri == F("remove")) {
        _sendResponse(remove());
    } else if (_uri == F("rename")) {
        _sendResponse(rename());
    } else if (_uri == F("view")) {
        _sendResponse(view(false));
    } else if (_uri == F("download")) {
        _sendResponse(view(true));
    } else {
        _sendResponse(404);
   }
}

uint16_t FileManager::list() {
    _debug_printf_P(PSTR("FileManager::list()\n"));

    String vfsRoot = _getArgument(F("vfs_root"));
    AsyncDirWrapper dir = _requireDir(FSPGM(dir));
    if (vfsRoot.length()) {
        append_slash(vfsRoot);
    }
    dir.setVirtualRoot(vfsRoot);
    if (_isValidData()) {
        _response = _debug_new AsyncDirResponse(dir);
        return 200;
    }
    return 500;
}

uint16_t FileManager::mkdir() {

    AsyncDirWrapper dir = _requireDir(FSPGM(dir));
    String newDir = _requireArgument(F("new_dir"));
    uint16_t httpCode = 200;
    String message;
    bool success = false;

    append_slash(newDir);
    newDir += '.';
    if (newDir.charAt(0) != '/') {
        newDir = dir.getDirName() + newDir;
    }

    if (SPIFFSWrapper::exists(newDir)) {
        message = FSPGM(ERROR_);
        message += F("Directory already exists");
    } else {
        File file = SPIFFSWrapper::open(newDir, "w");
        if (file) {
            file.close();
            success = true;
        } else {
            message = FSPGM(ERROR_);
            message += F("Failed to create directory");
        }
    }

    Logger_notice(F("Create directory %s - %s"), success ? SPGM(success) : SPGM(failure));
    _response = _request->beginResponse(httpCode, FSPGM(text_plain), message);
    return httpCode;
}

uint16_t FileManager::upload() {

    uint16_t httpCode = 500;
    PrintString message;
    bool success = false;
    AsyncDirWrapper uploadDir = _requireDir(F("upload_current_dir"));
    String filename = _request->arg(F("upload_filename"));
    if (!_request->_tempObject) {
        _debug_printf_P(PSTR("_tempObject is null\n"));
        return httpCode;
    }

    httpCode = 200;
    if (_request->hasParam(FSPGM(upload_file), true, true)) {
        AsyncWebParameter *p = _request->getParam(FSPGM(upload_file), true, true);

        if (filename.length() == 0) {
            filename = p->value();
        }
        if (filename.charAt(0) != '/') {
            filename = append_slash_copy(uploadDir.getDirName()) + filename;
        }

        if (SPIFFSWrapper::exists(filename)) {
            httpCode = 409;
            message = FSPGM(ERROR_);
            message.printf_P(PSTR("File %s already exists"), filename.c_str());

        } else if (SPIFFSWrapper::rename((char *)_request->_tempObject, filename)) {

            _debug_printf_P(PSTR("Renamed upload %s to %s\n"), _request->_tempObject, filename.c_str());
            AsyncFileUploadWebHandler::markTemporaryFileAsProcessed(_request);

            success = true;
            message = F("Upload successful");
        } else {
            message = FSPGM(ERROR_);
            message += F("Could not rename temporary file");
        }

    } else {
        message = FSPGM(ERROR_);
        message += F("Upload file parameter missing");
    }

    _debug_printf_P(PSTR("message %s http code %d\n"), message.c_str(), httpCode);

    uint8_t ajax_request = _request->arg(F("ajax_upload")).toInt();
    _debug_printf_P(PSTR("File upload status %d, message %s, ajax %d\n"), httpCode, message.c_str(), ajax_request);

    if (success) {
        Logger_notice(F("File upload successful. Filename %s, size %d"), filename.c_str(), SPIFFSWrapper::open(filename, "r").size());
    } else {
        Logger_warning(F("File upload failed: %s"), message.c_str());
    }

    if (!ajax_request) {
        _response = _request->beginResponse(302);
        String url = F("/file_manager.html?_message=%s");
        url += url_encode(message);
        if (success) {
            url += F("&_type=success&_title=Information");
        } else {
            url += F("&_title=ERROR%20");
            url += String(httpCode);
        }
        url += '#';
        url += uploadDir.getDirName();

        message = PrintString();
        httpCode = 302;
        _headers.replace(HttpLocationHeader(url));
        _headers.replace(HttpConnectionHeader(HttpConnectionHeader::HTTP_CONNECTION_CLOSE));
    }
    _debug_println(message);
    _response = _request->beginResponse(httpCode, FSPGM(text_plain), message);
    return true;

}

uint16_t FileManager::view(bool isDownload) {

    uint16_t httpCode = 200;
    File file = _requireFile(FSPGM(filename));
    String requestFilename = _request->arg(FSPGM(filename));
    if (!file) {
        String message;
        message = FSPGM(ERROR_);
        message += F("Cannot open ");
        message += requestFilename;
        _response = _request->beginResponse(httpCode, FSPGM(text_plain), message);
        _debug_println(message);
    } else {
        String filename = file.name();
        _debug_printf_P(PSTR("%s %s (request %s)\n"), isDownload ? PSTR("Downloading") : PSTR("Viewing"), filename.c_str(), requestFilename.c_str());
        _response = _request->beginResponse(file, filename, String(), isDownload);
    }
    return httpCode;
}

uint16_t FileManager::remove() {

    uint16_t httpCode = 200;
    String message;
    bool success = false;
    File file = _requireFile(FSPGM(filename));
    String requestFilename = _request->arg(FSPGM(filename));

    if (!file) {
        message = FSPGM(ERROR_);
        message += F("Cannot open ");
        message += requestFilename;
    } else {
        String filename = file.name();
        file.close();

        if (!SPIFFSWrapper::remove(filename)) {
            message = FSPGM(ERROR_);
            message += F("Cannot remove ");
            message += filename;
        } else {
            message = FSPGM(OK);
            success = true;
        }
        Logger_notice(F("Removing %s (request %s) - "), file.name(), requestFilename.c_str(), success ? SPGM(success) : SPGM(failure));
    }
    _debug_println(message);
    _response = _request->beginResponse(httpCode, FSPGM(text_plain), message);
    return httpCode;
}

uint16_t FileManager::rename() {
    uint16_t httpCode = 200;
    PrintString message;
    bool success = false;
    File file = _requireFile(FSPGM(filename));
    String requestFilename = _request->arg(FSPGM(filename));
    AsyncDirWrapper dir = _requireDir(FSPGM(dir));
    String renameTo = _requireArgument(F("to"));

    if (!file) {
        message = FSPGM(ERROR_);
        message += F("Cannot open ");
        message += requestFilename;
    } else {
        FSInfo info;
        SPIFFS.info(info);
        String renameFrom = file.name();
        file.close();

        if (renameTo.charAt(0) != '/') {
            renameTo = append_slash_copy(dir.getDirName()) + renameTo;
        }
        if (renameTo.length() >= info.maxPathLength) {
            message = FSPGM(ERROR_);
            message.printf_P(PSTR("Filename %s exceeds %d characters"), renameTo.c_str(), info.maxPathLength - 1);
        } else {
            if (SPIFFSWrapper::exists(renameTo)) {
                message = FSPGM(ERROR_);
                message.printf_P(PSTR("File %s already exists"), renameTo.c_str());
                _response = _request->beginResponse(httpCode, FSPGM(text_plain), message);
            } else {
                if (!SPIFFSWrapper::rename(renameFrom, renameTo)) {
                    message = FSPGM(ERROR_);
                    message.printf_P(PSTR("Cannot rename %s to %s"), renameFrom.c_str(), renameTo.c_str());
                } else {
                    message = FSPGM(OK);
                    success = true;
                }
                Logger_notice(F("Renaming %s => %s - %s"), renameFrom.c_str(), renameTo.c_str(), success ? SPGM(success) : SPGM(failure));
            }
        }
    }
    _debug_println(message);
    _response = _request->beginResponse(httpCode, FSPGM(text_plain), message);
    return httpCode;
}

bool FileManagerWebHandler::canHandle(AsyncWebServerRequest *request) {
    if (strncmp_P(request->url().c_str(), _uri, strlen_P(_uri))) {
        return false;
    }
    request->addInterestingHeader("ANY");
    return true;
}


void FileManagerWebHandler::handleRequest(AsyncWebServerRequest *request) {

    auto uri = request->url().substring(strlen_P(_uri));
    _debug_printf_P(PSTR("file manager %s (%s)\n"), uri.c_str(), request->url().c_str())
    FileManager fm(request, web_server_is_authenticated(request), uri);
    fm.handleRequest();
}

PROGMEM_STRING_DECL(plugin_config_name_http);

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ filemgr,
/* setupPriority            */ PLUGIN_MAX_PRIORITY,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ file_manager_install_web_server_hook,
/* statusTemplate           */ nullptr,
/* configureForm            */ nullptr,
/* reconfigurePlugin        */ file_manager_install_web_server_hook,
/* reconfigure Dependencies */ SPGM(plugin_config_name_http),
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ nullptr
);

#endif
