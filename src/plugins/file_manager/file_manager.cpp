/**
 * Author: sascha_lammers@gmx.de
 */

#if FILE_MANAGER

#include <Arduino_compat.h>
#include "file_manager.h"
#include "async_web_handler.h"
#include "async_web_response.h"
#include "fs_mapping.h"
#include "logger.h"
#include "misc.h"
#include "plugins.h"
#include "plugins_menu.h"
#include <Buffer.h>
#include <HttpHeaders.h>
#include <ListDir.h>
#include <PrintString.h>

#if DEBUG_FILE_MANAGER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

void FileManagerWebHandler::onRequestHandler(AsyncWebServerRequest *request)
{
    if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
        FileManager fm(request, true, F("upload"));
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

const String &FileManager::_requireDir(const String &name)
{
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        __LDBG_printf("%s is not set", name.c_str());
    }
    const String &path = _request->arg(name);
    if (!path.length()) {
        _errors++;
        __LDBG_printf("%s is empty", name.c_str());
    }
    __LDBG_printf("requireDir(%s) = %s (%d)", name.c_str(), path.c_str(), _errors);
    return path;
}

const String &FileManager::_requireFileMustExist(const String &name)
{
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        __LDBG_printf("%s is not set", name.c_str());
        return emptyString;
    }
    const String &path = _request->arg(name);
    if (!path.length()) {
        _errors++;
        __LDBG_printf("%s is empty", name.c_str());
        return emptyString;
    }
    else if (!FSWrapper::exists(path)) {
        _errors++;
        __LDBG_printf("File %s not found", path.c_str());
        return emptyString;
    }
    __LDBG_printf("requireFile(%s) = %s (%d)", name.c_str(), path.c_str(), _errors);
    return path;
}

File FileManager::_requireFile(const String &name)
{
    const String &path = _requireFileMustExist(name);
    if (!path.length()) {
        return File();
    }
    return FSWrapper::open(path, fs::FileOpenMode::read);
}

const String &FileManager::_requireArgument(const String &name)
{
    if (!_request->hasArg(name.c_str())) {
        _errors++;
        __LDBG_printf("%s is not set", name.c_str());
    }
    const String &value = _request->arg(name);
    if (!value.length()) {
        _errors++;
        __LDBG_printf("%s is empty", name.c_str());
    }
    return value;
}

const String &FileManager::_getArgument(const String &name)
{
    return _request->arg(name);
}

void FileManager::handleRequest()
{
    __LDBG_printf("is authenticated %d request uri %s", _isAuthenticated, _uri.c_str());
    _headers.addNoCache(true);
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
    const String &dirName = _requireDir(F("dir"));
    if (_isValidData()) {
        _response = new AsyncDirResponse(dirName, _request->arg(F("hidden")).toInt());
        return 200;
    }
    return 500;
}

uint16_t FileManager::mkdir()
{
    const String &dir = _requireDir(F("dir"));
    auto newDir = _requireArgument(F("new_dir"));
    uint16_t httpCode = 200;
    String message;
    auto success = false;

    append_slash(newDir);
    newDir += '.';
    if (!newDir.startsWith('/')) {
        newDir = append_slash(dir) + newDir;
    }
    normalizeFilename(newDir);

    if (FSWrapper::exists(newDir)) {
        message = F("ERROR:Directory already exists");
    } else {
        File file = FSWrapper::open(newDir, fs::FileOpenMode::write);
        if (file) {
            file.close();
            success = true;
        } else {
            message = F("ERROR:Failed to create directory");
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
    auto uploadDir = _requireDir(F("upload_current_dir"));
    auto filename = _request->arg(F("upload_filename"));
    auto overwriteTarget = _request->arg(F("overwrite_target")).toInt();

    httpCode = 200;
    if (_request->hasParam(F("upload_file"), true, true)) {
        AsyncWebParameter *p = _request->getParam(F("upload_file"), true, true);

        if (filename.length() == 0) {
            filename = p->value();
        }
        if (filename.charAt(0) != '/') {
            filename = append_slash(uploadDir) + filename;
        }
        normalizeFilename(filename);

        if (FSWrapper::exists(filename)) {
            if (overwriteTarget) {
                if (!KFCFS.remove(filename)) {
                    httpCode = 409;
                    message.printf_P(PSTR("ERROR:Cannot remove %s"), filename.c_str());
                }
            }
            else {
                httpCode = 409; // 409 Conflict
                message.printf_P(PSTR("ERROR:File %s already exists"), filename.c_str());
            }
        }

        // check if we can an error
        if (httpCode == 200) {
            if (_request->_tempFile && _request->_tempFile.fullName()) {
                // get filename before closing the file
                String fullname = _request->_tempFile.fullName();
                _request->_tempFile.close();

                if (FSWrapper::rename(fullname, filename)) {
                    __LDBG_printf("Renamed upload %s to %s", fullname.c_str(), filename.c_str());
                    message = F("Upload successful");
                }
                else {
                    httpCode = 410; // 410 Gone
                    // remove temporary file if it cannot be renamed
                    KFCFS.remove(fullname);
                }
            }

            if (httpCode != 200) {
                message = F("ERROR:Could not rename temporary file");
            }
        }

    }
    else {
        httpCode = 406; // 406 Not Acceptable
        message = F("ERROR:Upload file parameter missing");
    }

    __LDBG_printf("message %s http code %d", message.c_str(), httpCode);

    bool ajax_request = _request->arg(F("ajax_upload")).toInt();
    __LDBG_printf("File upload status %d, message %s, ajax %d", httpCode, message.c_str(), ajax_request);

    if (httpCode == 200) {
        Logger_notice(F("File upload successful. Filename %s, size %d"), filename.c_str(), FSWrapper::open(filename, fs::FileOpenMode::read).size());
    }
    else {
        Logger_warning(F("File upload failed: %s"), message.c_str());
    }

    if (!ajax_request) {
        PrintString url = '/';
        url.print(FSPGM(file_manager_html_uri, "file-manager.html"));
        url.print(F("?_message="));
        url.print(urlEncode(message));
        if (httpCode == 200) {
            url.print(F("&_type=success&_title=Information"));
        }
        else {
            url.print(F("&_title=ERROR%20"));
            url.printf_P(PSTR("%u"), httpCode);
        }
        url.print('#');
        url.print(uploadDir);

        message = String();
        httpCode = 302;
        _headers.replace<HttpLocationHeader>(url);
        _headers.replace<HttpConnectionHeader>();
    }
    __LDBG_printf("msg=%s", message.c_str());
    _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
    return httpCode;
}

uint16_t FileManager::view(bool isDownload)
{
    uint16_t httpCode = 200;
    File file = _requireFile(F("filename"));
    const String &requestFilename = _request->arg(F("filename"));
    if (!file) {
        String message = F("ERROR:Cannot open ");
        message += requestFilename;
        _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
        __LDBG_printf("msg=%s", message.c_str());
    }
    else {
        const String &filename = file.name();
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
    File file = _requireFile(F("filename"));
    const String &requestFilename = _request->arg(F("filename"));

    if (!file) {
        message = F("ERROR:Cannot open ");
        message += requestFilename;
    }
    else {
        const String &filename = file.fullName();
        file.close();

        if (!FSWrapper::remove(filename)) {
            message = F("ERROR:Cannot remove ");
            message += filename;
        }
        else {
            message = F("OK");
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
    auto file = _requireFile(F("filename"));
    const String &requestFilename = _request->arg(F("filename"));
    const String &dir = _requireDir(F("dir"));
    auto renameTo = _requireArgument(F("to"));

    if (!file) {
        message = F("ERROR:Cannot open ");
        message += requestFilename;
    }
    else {
        FSInfo info;
        KFCFS.info(info);
        const String &renameFrom = file.fullName();
        file.close();

        if (renameTo.charAt(0) != '/') {
            renameTo = append_slash(dir) + renameTo;
        }
        normalizeFilename(renameTo);

        if (renameTo.length() >= info.maxPathLength) {
            message = PrintString(F("ERROR:Filename %s exceeds %d characters"), renameTo.c_str(), info.maxPathLength - 1);
        }
        else {
            if (FSWrapper::exists(renameTo)) {
                message = PrintString(F("ERROR:File %s already exists"), renameTo.c_str());
                _response = _request->beginResponse(httpCode, FSPGM(mime_text_plain), message);
            }
            else {
                if (!FSWrapper::rename(renameFrom, renameTo)) {
                    message = PrintString(F("ERROR:Cannot rename %s to %s"), renameFrom.c_str(), renameTo.c_str());
                }
                else {
                    message = F("OK");
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
