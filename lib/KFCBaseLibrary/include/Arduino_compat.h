/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Quick and dirty library to compile ESP8266 code with MSVC++ as native win32 console app
 */

#pragma once

#define forward_delete(iterator)  \
    ({                            \
        auto next = iterator + 1; \
        delete *iterator;         \
        next;                     \
    })
#define reverse_delete(iterator)  \
    ({                            \
        auto prev = iterator - 1; \
        delete *iterator;         \
        prev;                     \
    })

#if defined(ESP32)

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>

#elif defined(ESP8266)

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <FS.h>

#elif _WIN32 || _WIN64

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock.h>
#include <iostream>

#define F(str) str
#define PSTR(str) str
#define FPSTR(str) str
#define snprintf_P snprintf
#define sprintf_P sprintf

typedef const char *PGM_P;
class __FlashStringHelper;

//#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
//#define F(string_literal) (FPSTR(PSTR(string_literal)))

#define strcpy_P strcpy
#define strncpy_P strncpy_s

const char *str_P(const char *str, uint8_t index = 0);  // not the same function as in misc.cpp just a dummy

#if DEBUG
#define if_debug_printf(...) printf(__VA_ARGS__);
#define if_debug_printf_P(...) printf(__VA_ARGS__);
#else
#define if_debug_printf(...) ;
#define if_debug_printf_P(...) ;
#endif

#define strcasecmp_P _stricmp
#define strcmp_P _strcmp

#define strdup _strdup
void throwException(PGM_P message);

uint16_t _crc16_update(uint16_t crc = ~0x0, const uint8_t a = 0);
uint16_t crc16_calc(uint8_t const *data, size_t length);

unsigned long millis();

void delay(uint32_t time_ms);

class Print;

class Printable {
   public:
    virtual size_t printTo(Print &p) const = 0;
};

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class Print {
   private:
    int write_error;
    size_t printNumber(unsigned long, uint8_t);
    size_t printFloat(double, uint8_t);

   protected:
    void setWriteError(int err = 1) {
        write_error = err;
    }

   public:
    Print() : write_error(0) {
    }

    int getWriteError() {
        return write_error;
    }
    void clearWriteError() {
        setWriteError(0);
    }

    virtual size_t write(uint8_t) = 0;
    size_t write(const char *str) {
        if (str == NULL) return 0;
        return write((const uint8_t *)str, strlen(str));
    }
    virtual size_t write(const uint8_t *buffer, size_t size);
    size_t write(const char *buffer, size_t size) {
        return write((const uint8_t *)buffer, size);
    }

    size_t printf(const char *format, ...);
    size_t printf_P(PGM_P format, ...);
    size_t print(const __FlashStringHelper *);
    //    size_t print(const String &);
    size_t print(const char[]);
    size_t print(char);
    size_t print(unsigned char, int = DEC);
    size_t print(int, int = DEC);
    size_t print(unsigned int, int = DEC);
    size_t print(long, int = DEC);
    size_t print(unsigned long, int = DEC);
    size_t print(double, int = 2);
    size_t print(const Printable &);

    size_t println(const __FlashStringHelper *);
    //    size_t println(const String &s);
    size_t println(const char[]);
    size_t println(char);
    size_t println(unsigned char, int = DEC);
    size_t println(int, int = DEC);
    size_t println(unsigned int, int = DEC);
    size_t println(long, int = DEC);
    size_t println(unsigned long, int = DEC);
    size_t println(double, int = 2);
    size_t println(const Printable &);
    size_t println(void);

    virtual void flush() { /* Empty implementation for backward compatibility */
    }
};

class String : public std::string {
   public:
    String() : std::string() {
    }
    String(bool value) : std::string() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        assign(buf);
    }
    String(int16_t value) : std::string() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        assign(buf);
    }
    String(uint16_t value) : std::string() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        assign(buf);
    }
    String(int32_t value) : std::string() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        assign(buf);
    }
    String(uint32_t value) : std::string() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", value);
        assign(buf);
    }
    String(int64_t value) : std::string() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", value);
        assign(buf);
    }
    String(uint64_t value) : std::string() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", value);
        assign(buf);
    }
    String(long value) : std::string() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", value);
        assign(buf);
    }
    String(unsigned long value) : std::string() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%lu", value);
        assign(buf);
    }
    String(float value) : std::string() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%f", value);
        assign(buf);
    }
    String(const char ch) : String() {
        assign(1, ch);
    }
    String(const char *str) : std::string(str) {
    }
    int indexOf(const String &find, int index = 0) const {
        return find_first_of(find, index);
    }
    int indexOf(char ch, int index = 0) const {
        return find_first_of(ch, index);
    }
    String substring(size_t from) const {
        return String(substr(from).c_str());
    }
    String substring(size_t from, size_t to) const {
        return String(substr(from, to).c_str());
    }
    long toInt() const {
        return atol(c_str());
    }
    float toFloat() const {
        return (float)atof(c_str());
    }
    unsigned char reserve(size_t size) {
        std::string::reserve(size);
        return (capacity() >= size);
    }
    bool equals(const String &value) const {
        return *this == value;
    }
    bool equals(const char *value) const {
        return strcmp(c_str(), value) == 0;
    }
    bool equalsIgnoreCase(const String &value) const {
        return _stricmp(c_str(), value.c_str()) == 0;
    }
    bool equalsIgnoreCase(const char *value) const {
        return _stricmp(c_str(), value) == 0;
    }
    void remove(int index, size_t count = -1) {
        if (index < 0) {
            index = length() + index;
        }
        if (count == -1 || index + count >= length()) {
            assign(substr(0, index));
        } else {
            assign(substr(0, index) + substr(index + count));
        }
    }
    char *begin() {
        return (char *)c_str();
    }
    void trim(void) {
        char *buffer = begin();
        size_t len = length();
        if (!buffer || len == 0) return;
        char *begin = buffer;
        while (isspace(*begin)) begin++;
        char *end = buffer + len - 1;
        while (isspace(*end) && end >= begin) end--;
        len = end + 1 - begin;
        if (begin > buffer) memmove(buffer, begin, len);
        buffer[len] = 0;
        assign(buffer);
    }
    void toLowerCase(void) {
        if (length()) {
            for (char *p = begin(); *p; p++) {
                *p = tolower(*p);
            }
        }
    }
    void toUpperCase(void) {
        if (length()) {
            for (char *p = begin(); *p; p++) {
                *p = toupper(*p);
            }
        }
    }
    void replace(char find, char replace) {
        if (length()) {
        }
        for (char *p = begin(); *p; p++) {
            if (*p == find) *p = replace;
        }
    }
    void replace(String fromStr, String toStr) {
        size_t pos = find(fromStr);
        while (pos != String::npos) {
            std::string::replace(pos, fromStr.length(), toStr);
            pos = find(fromStr, pos + toStr.length());
        }
    }

    unsigned char concat(const char *cstr, unsigned int length) {
        if (!cstr) {
            return 0;
        }
        if (length == 0) {
            return 1;
        }
        this->append(cstr, length);
        return length;
    }

    unsigned char concat(char c) {
        this->append(1, c);
        return 1;
    }

    bool startsWith(const String &str) {
        if (length() < str.length()) {
            return false;
        }
        return (strncmp(c_str(), str.c_str(), str.length()) == 0);
    }
};

enum SeekMode { SeekSet = SEEK_SET, SeekCur = SEEK_CUR, SeekEnd = SEEK_END };

class Stream : public Print {
   public:
    Stream() : Print() {
        _fp = nullptr;
    }
    Stream(FILE *fp) : _fp(fp) {
        if (fp) {
            seek(0, SeekEnd);
            _size = position();
            seek(0);
        }
    }

    FILE *getFile() {
        return _fp;
    }
    void setFile(FILE *fp) {
        _fp = fp;
        seek(0, SeekEnd);
        _size = position();
        seek(0);
    }

    virtual size_t position() const {
        return ftell(_fp);
    }

    virtual bool seek(long pos, int mode) {
        int res = fseek(_fp, pos, mode);
        if (res != 0) {
            perror("seek");
        }
        // printf("DEBUG seek %d %d = %d => %d\n", pos, mode, res, res == 0);
        return res == 0;
        // return fseek(_fp, pos, mode) == 0;
    }
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }

    virtual size_t size() const {
        return _size;
    }

    virtual operator bool() const {
        return _fp != nullptr;
    }

    virtual int read() {
        if (!_fp) {
            return -1;
        }
        uint8_t b;
        if (read(&b, sizeof(b)) != 1) {
            if (ferror(_fp)) {
                perror("read");
            }
            printf("DEBUG read -1\n");
            return -1;
        }
        // printf("DEBUG read byte\n");
        return b;
    }

    virtual int peek() {
        long pos = ftell(_fp);
        int res = read();
        fseek(_fp, pos, SEEK_SET);
        return res;
    }

    int read(uint8_t *buffer, size_t len) {
        uint8_t *ptr = buffer;
        while (len--) {
            int ch = read();
            if (ch == -1) {
                break;
            }
            *ptr++ = ch;
        }
        return ptr - buffer;
    }

    String readStringUntil(char terminator) {
        int ch;
        String buf;
        while (true) {
            if (!available() || (ch = read()) == -1) {
                printf("readStringUntil(%d) failed\n", terminator);
                break;
            }
            if (ch == terminator) {
                break;
            }
            if (buf.length() > 1024) {
                printf("readStringUntil(%d) failed, reached 1024byte length\n", terminator);
                return String();
            }
            buf += (char)ch;
        }
        return buf;
    }

    String readString() {
        String buf;
        while (available()) {
            int ch = read();
            if (ch == -1) {
                break;
            }
            buf += (char)ch;
        }
        return buf;
    }

    size_t write(char data) {
        return write((uint8_t)data);
    }
    size_t write(uint8_t data) {
        return write(&data, sizeof(data));
    }
    virtual size_t write(const uint8_t *data, size_t len) {
        size_t size = fwrite(data, 1, len, _fp);
        if (ferror(_fp)) {
            perror("write");
        }
        return size;
    }

    virtual void close() {
        fclose(_fp);
    }

    virtual void flush() {
        fflush(_fp);
    }

    virtual int available() {
        return feof(_fp) ? 0 : 1;
    }

   private:
    FILE *_fp;
    size_t _size;
};

class File : public Stream {
   public:
    File() : Stream() {
    }
    File(FILE *fp) : Stream(fp) {
    }
};


class _SPIFFS {
   public:
    File open(const String &filename, const char *mode) {
        FILE *fp;
        fopen_s(&fp, filename.c_str(), mode);
        return File(fp);
    }
};

class IPAddress {
   public:
    struct _address {
        uint8_t byte[4];
    };

    IPAddress() {
        _addr = 0;
    }
    IPAddress(uint32_t addr) {
        _addr = addr;
    }
    IPAddress(const String &addr) {
        fromString(addr);
    }
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        ((_address *)&_addr)->byte[0] = a;
        ((_address *)&_addr)->byte[1] = b;
        ((_address *)&_addr)->byte[2] = c;
        ((_address *)&_addr)->byte[3] = d;
    }

    IPAddress &operator=(uint32_t addr) {
        _addr = addr;
        return *this;
    }
    bool operator==(const IPAddress &addr) {
        return _addr = addr._addr;
    }

    void fromString(const String &addr) {
        _addr = inet_addr(addr.c_str());
    }
    String toString() const {
        in_addr a;
        a.S_un.S_addr = _addr;
        return inet_ntoa(a);
    }

    uint8_t *raw_address() const {
        return (uint8_t *)&_addr;
    }

   private:
    uint32_t _addr;
};

class EEPROMFile : public Stream {
   public:
    EEPROMFile(uint16_t size = 4096) {
        _position = 0;
        _size = size;
        _eeprom = nullptr;
    }
    ~EEPROMFile() {
        close();
    }

    bool begin() {
        if (_eeprom) {
            free(_eeprom);
        }
        _eeprom = (uint8_t *)malloc(_size);
        if (!_eeprom) {
            return false;
        }
        FILE *fp;
        errno_t err = fopen_s(&fp, "eeprom.bin", "rb");
        if (err) {
            memset(_eeprom, 0, _size);
            if (commit()) {
                return true;
            }
            return false;
        }
        if (fread(_eeprom, _size, 1, fp) != 1) {
            perror("read");
            fclose(fp);
            return false;
        }
        fclose(fp);
        return true;
    }

    void end() {
        if (_eeprom) {
            commit();
            free(_eeprom);
            _eeprom = nullptr;
        }
    }

    bool commit() {
        FILE *fp;
        errno_t err = fopen_s(&fp, "eeprom.bin", "wb");
        if (err) {
            return false;
        }
        if (fwrite(_eeprom, _size, 1, fp) != 1) {
            perror("write");
            return false;
        }
        fclose(fp);
        return true;
    }

    void close() {
        end();
    }

    size_t position() {
        return _position;
    }

    bool seek(long pos, int mode) override {
        if (mode == SeekSet) {
            if (pos >= 0 && pos < _size) {
                _position = (uint16_t)pos;
                return true;
            }
        } else if (mode == SeekCur) {
            return seek(_position + pos, SeekSet);
        } else if (mode == SeekEnd) {
            return seek(_size - pos, SeekSet);
        }
        return false;
    }
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }

    int read() override {
        uint8_t ch;
        if (read(&ch, sizeof(ch)) != 1) {
            return -1;
        }
        return ch;
    }
    int read(uint8_t *buffer, size_t len) {
        if (_position + len >= _size) {
            len = _size - _position;
        }
        memcpy(buffer, _eeprom + _position, len);
        _position += (uint16_t)len;
        return len;
    }


    size_t write(char data) {
        return write((uint8_t)data);
    }
    size_t write(uint8_t data) {
        return write(&data, sizeof(data));
    }
    size_t write(uint8_t *data, size_t len) {
        if (_position + len >= _size) {
            len = _size - _position;
        }
        memcpy(_eeprom + _position, data, len);
        _position += (uint16_t)len;
        return len;
    }

    void flush() {
    }

    size_t size() const override {
        return _size;
    }

    int available() override {
        return _size - _position;
    }

   private:
    FILE *fp;
    uint8_t *_eeprom;
    uint16_t _position;
    uint16_t _size;
};

extern _SPIFFS SPIFFS;

class Stdout : public Stream {
   public:
    Stdout() : Stream() {
        _size = 0xffff;
        _position = 0;
        _buffer = (uint8_t *)calloc(_size, 1);
    }
    ~Stdout() {
        close();
    }

    void close() {
        free(_buffer);
        _buffer = nullptr;
    }

    size_t position() const override {
        return _position;
    }

    bool seek(long pos, int mode) {
        if (mode == SeekSet) {
            if (pos >= 0 && pos < _size) {
                _position = (uint16_t)pos;
                return true;
            }
        } else if (mode == SeekCur) {
            return seek(_position + pos, SeekSet);
        } else if (mode == SeekEnd) {
            return seek(_size - pos, SeekSet);
        }
        return false;
    }
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }

    int read() {
        uint8_t ch;
        if (read(&ch, sizeof(ch)) != 1) {
            return -1;
        }
        return ch;
    }
    int read(uint8_t *buffer, size_t len) {
        if (_position + len >= _size) {
            len = _size - _position;
        }
        memcpy(buffer, _buffer + _position, len);
        return len;
    }


    size_t write(char data) {
        return write((uint8_t)data);
    }
    size_t write(uint8_t data) {
        return write(&data, sizeof(data));
    }
    size_t write(uint8_t *data, size_t len) {
        if (_position + len >= _size) {
            len = _size - _position;
        }
        printf("%04x-%04x: ", _position, _position + len - 1);
        for (size_t i = 0; i < len; i++) {
            printf("%02x ", data[i]);
        }
        for (size_t i = 0; i < len; i++) {
            if (isalnum(data[i])) {
                printf("%c", data[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
        memcpy(_buffer + _position, data, len);
        _position += (uint16_t)len;
        return len;
    }

    void flush() {
    }

    size_t size() const {
        return _size;
    }

    int available() override {
        return _size - _position;
    }


   private:
    uint8_t *_buffer;
    uint16_t _position;
    uint16_t _size;
};

class FakeSerial : public Stdout {
   public:
    FakeSerial() : Stdout() {
    }
    virtual int available() {
        return 0;
    }
    virtual int read() {
        return -1;
    }
    virtual int peek() {
        return -1;
    }
    void flush(void) override {
        fflush(stdout);
    }
    virtual size_t write(uint8_t c) {
        ::printf("%c", c);
        return 1;
    }
};

extern FakeSerial Serial;

#pragma comment(lib, "Ws2_32.lib")

class WiFiUDP : public Stream {
   public:
    WiFiUDP();
    ~WiFiUDP();

    int beginPacket(const char *host, uint16_t port);
    int beginPacket(IPAddress ip, uint16_t port);
    size_t write(uint8_t byte);
    size_t write(const uint8_t *buffer, size_t size);
    void flush();
    int endPacket();

   private:
    int _beginPacket(uint16_t port);
    void _clear();

    int available() override {
        return 0;
    }
    int read() override {
        return -1;
    }
    int peek() override {
        return -1;
    }


   private:
    SOCKET _socket;
    sockaddr_in _dst;
    char *_buffer;
    uint16_t _len;
};

class ESP8266WiFiClass {
   public:
    ESP8266WiFiClass() {
        _isConnected = true;
    }

    bool isConnected() {
        return _isConnected;
    }
    void setIsConnected(bool connected) {
        _isConnected = connected;
    }

   private:
    bool _isConnected;
};

extern ESP8266WiFiClass WiFi;

#else

#error Platform not supported

#endif
