/**
 * Author: sascha_lammers@gmx.de
 */

#if FILE_MANAGER

#include <Arduino_compat.h>
#include "file_manager.h"
#include <Buffer.h>
#include <PrintString.h>
#include <HttpHeaders.h>
#include <ListDir.h>
#include "fs_mapping.h"
#include "async_web_response.h"
#include "async_web_handler.h"
#include "misc.h"
#include "logger.h"
#include "plugins.h"
#include "plugins_menu.h"

#if DEBUG_FILE_MANAGER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void FileManagerWebHandler::onRequestHandler(AsyncWebServerRequest *request)
{
    if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
        FileManager fm(request, true, FSPGM(upload, "upload"));
        fm.handleRequest();
    }
    else {
    WebServer::Plugin::send(403, request);
    }
}


FileManager::FileManager() :
    _isAuthenticated(false),
    _errors(0),
    _request(nullptr),
    _response(nullptr)
{
}

FileManager::FileManager(AsyncWebServerRequest *request, bool isAuthenticated, const String &uri) :
    _isAuthenticated(isAuthenticated),
    _errors(isAuthenticated ? 0 : 1),
    _uri(uri),
    _request(request),
    _response(nullptr)
{
}

void FileManager::addHandler(AsyncWebServer *server)
{
    server->addHandler(new AsyncFileUploadWebHandler(FSPGM(file_manager_upload_uri), FileManagerWebHandler::onRequestHandler));
    server->addHandler(new FileManagerWebHandler(FSPGM(file_manager_base_uri)));
}

void FileManager::_sendResponse(uint16_t httpStatusCode)
{
    __LDBG_printf("_sendResponse(%d)", httpStatusCode);
    if (httpStatusCode == 0) {
        httpStatusCode = _isValidData() ? 500 : 200;
    }
    if (!_response) {
        _response = _request->beginResponse(httpStatusCode);
    }
    _headers.setResponseHeaders(_response);
    _response->setCode(httpStatusCode);
    __LDBG_printf("filemanager:%s response %d", _uri.c_str(), httpStatusCode);
    _request->send(_response);
}

bool FileManager::_isValidData()
{
    __LDBG_printf("isValidData() = %d (%d)", _errors == 0, _errors);
    return _errors == 0;
}

bool FileManager::_requireAuthentication()
{
    if (!_isAuthenticated) {
        _errors++;
    }
    __LDBG_printf("requireAuthentication() = %d (%d)", _isAuthenticated, _errors);
    return _isAuthenticated;
}

String FileManager::_requireDir(const String &name)
{
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        __LDBG_printf("%s is not set", name.c_str());
    }
    String path = _request->arg(name);
    if (!path.length()) {
        _errors++;
        __LDBG_printf("%s is empty", name.c_str());
    }
    __LDBG_printf("requireDir(%s) = %s (%d)", name.c_str(), path.c_str(), _errors);
    return path;
}

String FileManager::_requireFile(const String &name, bool mustExists)
{
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        __LDBG_printf("%s is not set", name.c_str());
    }
    String path = _request->arg(name);
    if (!path.length()) {
        _errors++;
        __LDBG_printf("%s is empty", name.c_str());
    }
    else if (!FSWrapper::exists(path)) {
        _errors++;
        __LDBG_printf("File %s not found", path.c_str());
        path = String();
    }
    __LDBG_printf("requireFile(%s) = %s (%d)", name.c_str(), path.c_str(), _errors);
    return path;
}

File FileManager::_requireFile(const String &name)
{
    String path = _requireFile(name, true);
    if (!path.length()) {
        return File();
    }
    return FSWrapper::open(path, fs::FileOpenMode::read);
}

String FileManager::_requireArgument(const String &name)
{
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        __LDBG_printf("%s is not set", name.c_str());
    }
    String value = _request->arg(name);
    if (!value.length()) {
        _errors++;
        __LDBG_printf("%s is empty", name.c_str());
    }
    return value;
}

String FileManager::_getArgument(const String &name)
{
    return _request->arg(name);
}

void FileManager::handleRequest()
{
    _headers.addNoCache(true);

    __LDBG_printf("is authenticated %d request uri %s", _isAuthenticated, _uri.c_str());

    if (!_isAuthenticated) {
        _sendResponse(403);
    }
    else if (_uri.equals(F("list"))) {
        _sendResponse(list());
    }
    else if (_uri.equals(F("mkdir"))) {
        _sendResponse(mkdir());
    }
    else if (_uri.equals(F("upload"))) {
        _sendResponse(upload());
    }
    else if (_uri.equals(F("remove"))) {
        _sendResponse(remove());
    }
    else if (_uri.equals(F("rename"))) {
        _sendResponse(rename());
    }
    else if (_uri.equals(F("view"))) {
        _sendResponse(view(false));
    }
    else if (_uri.equals(F("download"))) {
        _sendResponse(view(true));
    }
    else {
        _sendResponse(404);
   }
}

uint16_t FileManager::list()
{
    __LDBG_printf("FileManager::list()");
    auto dirName = _requireDir(FSPGM(dir));
    if (_isValidData()) {
        _response = new AsyncDirResponse(dirName, _request->arg(F("hidden")).toInt());
        return 200;
    }
    return 500;
}

uint16_t FileManager::mkdir()
{
    auto dir = _requireDir(FSPGM(dir));
    auto newDir = _requireArgument(F("new_dir"));
    uint16_t httpCode = 200;
    String message;
    auto success = false;

    append_slash(newDir);
    newDir += '.';
    if (newDir.charAt(0) != '/') {
        append_slash(dir);
        newDir = dir + newDir;
    }
    normalizeFilename(newDir);

    if (FSWrapper::exists(newDir)) {
        message = FSPGM(ERROR_, "ERROR:");
        message += F("Directory already exists");
    } else {
        File file = FSWrapper::open(newDir, fs::FileOpenMode::write);
        if (file) {
            file.close();
            success = true;
        } else {
            message = FSPGM(ERROR_);
            message += F("Failed to create directory");
        }
    }

    Logger_notice(F("Create directory %s - %s"), newDir.c_str(), success ? SPGM(success, "success") : SPGM(failure, "failure"));
    _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
    return httpCode;
}

uint16_t FileManager::upload()
{
    uint16_t httpCode = 500;
    PrintString message;
    auto success = false;
    auto uploadDir = _requireDir(F("upload_current_dir"));
    auto filename = _request->arg(F("upload_filename"));
    // if (!_request->_tempFile) {
    //     __LDBG_printf("_temFile is closed");
    //     return httpCode;
    // }

    httpCode = 200;
    if (_request->hasParam(FSPGM(upload_file, "upload_file"), true, true)) {
        AsyncWebParameter *p = _request->getParam(FSPGM(upload_file), true, true);

        if (filename.length() == 0) {
            filename = p->value();
        }
        if (filename.charAt(0) != '/') {
            append_slash(uploadDir);
            filename = uploadDir + filename;
        }
        normalizeFilename(filename);

        if (FSWrapper::exists(filename)) {
            httpCode = 409;
            message = FSPGM(ERROR_);
            message.printf_P(PSTR("File %s already exists"), filename.c_str());

        } else {
            if (_request->_tempFile && _request->_tempFile.fullName()) {
                // get filename before closing the file
                String fullname = _request->_tempFile.fullName();
                _request->_tempFile.close();

                if (FSWrapper::rename(fullname, filename)) {
                    __LDBG_printf("Renamed upload %s to %s", fullname.c_str(), filename.c_str());
                    success = true;
                    message = F("Upload successful");
                }
                else {
                    // remove temporary file if it cannot be renamed
                    KFCFS.remove(fullname);
                }
            }

            if (!success) {
                message = FSPGM(ERROR_);
                message += F("Could not rename temporary file");
            }
        }

    } else {
        message = FSPGM(ERROR_);
        message += F("Upload file parameter missing");
    }

    __LDBG_printf("message %s http code %d", message.c_str(), httpCode);

    uint8_t ajax_request = _request->arg(F("ajax_upload")).toInt();
    __LDBG_printf("File upload status %d, message %s, ajax %d", httpCode, message.c_str(), ajax_request);

    if (success) {
        Logger_notice(F("File upload successful. Filename %s, size %d"), filename.c_str(), FSWrapper::open(filename, fs::FileOpenMode::read).size());
    }
    else {
        Logger_warning(F("File upload failed: %s"), message.c_str());
    }

    if (!ajax_request) {
        String url = PrintString(F("/%s?_message="), SPGM(file_manager_html_uri, "file-manager.html"));
        url += urlEncode(message);
        if (success) {
            url += F("&_type=success&_title=Information");
        }
        else {
            url += F("&_title=ERROR%20");
            url += String(httpCode);
        }
        url += '#';
        url += uploadDir;

        message = String();
        httpCode = 302;
        _headers.replace<HttpLocationHeader>(url);
        _headers.replace<HttpConnectionHeader>();
    }
    __LDBG_printf("msg=%s", message.c_str());
    _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
    return true;
}

uint16_t FileManager::view(bool isDownload)
{
    uint16_t httpCode = 200;
    File file = _requireFile(FSPGM(filename, "filename"));
    String requestFilename = _request->arg(FSPGM(filename));
    if (!file) {
        String message = FSPGM(ERROR_);
        message += F("Cannot open ");
        message += requestFilename;
        _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
        __LDBG_printf("msg=%s", message.c_str());
    }
    else {
        String filename = file.name();
        __LDBG_printf("%s %s (request %s)", isDownload ? PSTR("Downloading") : PSTR("Viewing"), filename.c_str(), requestFilename.c_str());
        _response = _request->beginResponse(file, filename, String(), isDownload);
    }
    return httpCode;
}

uint16_t FileManager::remove()
{
    uint16_t httpCode = 200;
    String message;
    bool success = false;
    File file = _requireFile(FSPGM(filename));
    String requestFilename = _request->arg(FSPGM(filename));

    if (!file) {
        message = FSPGM(ERROR_);
        message += F("Cannot open ");
        message += requestFilename;
    }
    else {
        String filename = file.name();
        file.close();

        if (!FSWrapper::remove(filename)) {
            message = FSPGM(ERROR_);
            message += F("Cannot remove ");
            message += filename;
        }
        else {
            message = FSPGM(OK);
            success = true;
        }
        Logger_notice(F("Removing %s (request %s) - %s"), filename.c_str(), requestFilename.c_str(), success ? SPGM(success) : SPGM(failure));
    }
    __LDBG_printf("%s", message.c_str());
    _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
    return httpCode;
}

uint16_t FileManager::rename()
{
    uint16_t httpCode = 200;
    String message;
    auto success = false;
    auto file = _requireFile(FSPGM(filename));
    auto requestFilename = _request->arg(FSPGM(filename));
    auto dir = _requireDir(FSPGM(dir));
    auto renameTo = _requireArgument(F("to"));

    if (!file) {
        message = FSPGM(ERROR_);
        message += F("Cannot open ");
        message += requestFilename;
    }
    else {
        FSInfo info;
        KFCFS.info(info);
        String renameFrom = file.name();
        file.close();

        if (renameTo.charAt(0) != '/') {
            append_slash(dir);
            renameTo = dir + renameTo;
        }
        normalizeFilename(renameTo);

        if (renameTo.length() >= info.maxPathLength) {
            message = PrintString(F("%sFilename %s exceeds %d characters"), SPGM(ERROR_), renameTo.c_str(), info.maxPathLength - 1);
        }
        else {
            if (FSWrapper::exists(renameTo)) {
                message = PrintString(F("%sFile %s already exists"), SPGM(ERROR_), renameTo.c_str());
                _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
            }
            else {
                if (!FSWrapper::rename(renameFrom, renameTo)) {
                    message = PrintString(F("%sCannot rename %s to %s"), SPGM(ERROR_), renameFrom.c_str(), renameTo.c_str());
                }
                else {
                    message = FSPGM(OK);
                    success = true;
                }
                Logger_notice(F("Renaming %s => %s - %s"), renameFrom.c_str(), renameTo.c_str(), success ? SPGM(success) : SPGM(failure));
            }
        }
    }
    __LDBG_printf("%s", message.c_str());
    _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
    return httpCode;
}

void FileManager::normalizeFilename(String &filename)
{
    while(filename.indexOf(F("//")) != -1) {
        filename.replace(F("//"), F("/"));
    }
}

bool FileManagerWebHandler::canHandle(AsyncWebServerRequest *request)
{
    if (request->url().startsWith(_uri)) {
        request->addInterestingHeader(F("ANY"));
        return true;
    }
    return false;
}

void FileManagerWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    // request->url() starts with _uri
    auto uri = request->url().c_str() + strlen_P(RFPSTR(_uri));
    __LDBG_printf("file manager %s (%s)", uri, request->url().c_str());
    FileManager fm(request, WebServer::Plugin::getInstance().isAuthenticated(request) == true, uri);
    fm.handleRequest();
}

class FileManagerPlugin : public PluginComponent {
public:
    FileManagerPlugin();

    virtual void setup(SetupModeType mode, const DependenciesPtr &dependencies) override
    {
        dependencies->dependsOn(F("http"), [](const PluginComponent *, DependencyResponseType response) {
            WebServer::AsyncWebServerEx *server;
            if (response == DependencyResponseType::SUCCESS && (server = WebServer::Plugin::getWebServerObject()) != nullptr) {
                server->addHandler(new AsyncFileUploadWebHandler(FSPGM(file_manager_upload_uri), FileManagerWebHandler::onRequestHandler));
                server->addHandler(new FileManagerWebHandler(FSPGM(file_manager_base_uri)));
            }
            else {
                // remove menu
                bootstrapMenu.remove(bootstrapMenu.findMenuByURI(FSPGM(file_manager_html_uri)));
            }
        }, this);
    }

    // virtual void reconfigure(const String &source) override
    // {
    //     setup(SetupModeType::DEFAULT);
    // }

    virtual void createMenu() override
    {
        bootstrapMenu.addMenuItem(getFriendlyName(), FSPGM(file_manager_html_uri), navMenu.util);
    }
};

static FileManagerPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    FileManagerPlugin,
    "filemgr",          // name
    "File Manager",     // friendly name
    "",                 // web_templates
    "",                 // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::MAX,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    false,              // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

FileManagerPlugin::FileManagerPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(FileManagerPlugin))
{
    REGISTER_PLUGIN(this, "FileManagerPlugin");
}

#endif
