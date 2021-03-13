/**
* Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#include "Arduino_compat.h"

File _SPIFFS::open(const String filename, const char *mode) 
{
    FILE *fp;
    String modeStr = mode;
    if (modeStr.indexOf('b') == -1 && modeStr.indexOf('t') == -1) {
        modeStr += 'b';
    }
    fp = fopen(filename.c_str(), modeStr.c_str());
    // fopen_s(&fp, filename.c_str(), modeStr.c_str());
    return File(fp, filename);
}

Dir _SPIFFS::openDir(const String dir) 
{
    return Dir(dir);
}

bool _SPIFFS::exists(const String filename) 
{
    File file   = open(filename, "r");
    bool result = !!file;
    file.close();
    return result;
}

bool _SPIFFS::remove(const String filename) 
{
    return ::remove(filename.c_str()) == 0;
}

bool _SPIFFS::rename(const String fromPath, const String toPath) 
{
    return ::rename(fromPath.c_str(), toPath.c_str()) == 0;
}

void _SPIFFS::begin() 
{
}

void _SPIFFS::end() {
}

File::File(FileImplPtr p, FS *baseFS) : 
    _p(p), 
    _baseFS(baseFS) 
{
}

File::File(FILE *fp, FileImplPtr p, FS *baseFS) : 
    _p(p), 
    _baseFS(baseFS), 
    Stream(fp) 
{
}

size_t File::write(uint8_t data) 
{
    return _fp_write(data);
}

int File::available() 
{
    return _fp_available();
}

int File::read() 
{
    return _fp_read();
}

int File::peek() 
{
    return _fp_peek();
}

size_t File::readBytes(char *buffer, size_t length) 
{
    return _fp_read((uint8_t *)buffer, length);
}

bool File::seek(uint32_t pos, SeekMode mode) 
{
    return _fp_seek(pos, mode);
}

void File::close() 
{
    _fp_close();
}

size_t File::position() const 
{
    return _fp_position();
}

void Dir::__test() 
{
    Dir dir = SPIFFS.openDir("./");
    while (dir.next()) {
        if (dir.isFile()) {
            printf("is_file: Dir::fileName() %s Dir::fileSize() %d\n", dir.fileName().c_str(), dir.fileSize());
            File file = dir.openFile(fs::FileOpenMode::read);
            printf("File::size %d\n", file.size());
            file.close();
        } else if (dir.isDirectory()) {
            printf("is_dir: Dir::fileName() %s\n", dir.fileName().c_str());
        }
    }
}

Dir::Dir(const String directory) : _handle(INVALID_HANDLE_VALUE) 
{
    _directory = directory;
    _directory.replace('/', '\\');
    if (!_directory.endsWith("\\")) {
        _directory += '\\';
    }
}

Dir::~Dir() 
{
    if (_handle != INVALID_HANDLE_VALUE) {
        FindClose(_handle);
        _handle = INVALID_HANDLE_VALUE;
    }
}

bool Dir::next() 
{
    bool result;
    if (_handle == INVALID_HANDLE_VALUE) {
        TCHAR path[MAX_PATH];
        size_t convertedChars = 0;
        mbstowcs_s(&convertedChars, path, MAX_PATH, _directory.c_str(), _TRUNCATE);
        StringCchCat(path, MAX_PATH, TEXT("*"));
        if ((_handle = FindFirstFile(path, &_ffd)) == INVALID_HANDLE_VALUE) {
            return false;
        }
        result = true;
    } else {
        result = FindNextFile(_handle, &_ffd);
    }
    return result;
}

bool Dir::isDirectory() const {
    return (_handle != INVALID_HANDLE_VALUE) && (_ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool Dir::isFile() const {
    return (_handle != INVALID_HANDLE_VALUE) && !(_ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

const String Dir::fileName() const 
{
    String tmp;
    tmp.reserve(wcslen(_ffd.cFileName) + 1);
    size_t convertedChars;
    wcstombs_s(&convertedChars, tmp.begin(), static_cast<ex::String &>(tmp).capacity(), _ffd.cFileName, _TRUNCATE);
    tmp.replace('\\', '/');
    return tmp;
}

uint32_t Dir::fileSize() const 
{
    if (isFile()) {
        return _ffd.nFileSizeLow;
    }
    return 0;
}

File Dir::openFile(const char *mode) 
{
    return SPIFFS.open(fileName(), mode);
}

#endif
