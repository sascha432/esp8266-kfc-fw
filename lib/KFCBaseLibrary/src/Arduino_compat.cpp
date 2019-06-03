/**
  Author: sascha_lammers@gmx.de
*/

#include "Arduino_compat.h"

#if _WIN32 || _WIN64

_SPIFFS SPIFFS;

void throwException(PGM_P message) {
    printf("EXCEPTION: %s\n", (const char *)message);
    exit(-1);
}

size_t strftime_P(char *buf, size_t size, PGM_P format, const tm *tm) {
	return strftime(buf, size, str_P(FPSTR(format)), tm);
}

tm *timezone_localtime(const time_t *timer) {
	static struct tm tm;
	if (localtime_s(&tm, timer)) {
        memset(&tm, 0, sizeof(tm));
	}
	return &tm;
}

uint16_t _crc16_update(uint16_t crc, const uint8_t a) {
    crc ^= a;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc = (crc >> 1);
        }
    }
    return crc;
}

uint16_t crc16_calc(uint8_t const *data, size_t length) {
    uint16_t crc = ~0;
    for (size_t index = 0; index < length; ++index) {
        crc = _crc16_update(crc, data[index]);
    }
    return crc;
}

static bool init_millis();

static ULONG64 millis_start_time = 0;
static bool millis_initialized = init_millis();

static bool init_millis() {
    millis_start_time = millis();
    return true;
}

unsigned long millis() {
    SYSTEMTIME sysTime;
    GetSystemTime(&sysTime);
    time_t now = time(nullptr);
    return (unsigned long)((sysTime.wMilliseconds + now * 1000) - millis_start_time);
}

void delay(uint32_t time_ms) {
    Sleep(time_ms);
}


FakeSerial Serial;

const char *str_P(const char *str, uint8_t index) {
    return str;
}

/* default implementation: may be overridden */
size_t Print::write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) {
        size_t ret = write(*buffer++);
        if (ret == 0) {
            // Write of last byte didn't complete, abort additional processing
            break;
        }
        n += ret;
    }
    return n;
}

size_t Print::printf(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char *buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
        buffer = new char[len + 1];
        if (!buffer) {
            return 0;
        }
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    len = write((const uint8_t *)buffer, len);
    if (buffer != temp) {
        delete[] buffer;
    }
    return len;
}

size_t Print::printf_P(PGM_P format, ...) {
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char *buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
        buffer = new char[len + 1];
        if (!buffer) {
            return 0;
        }
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    len = write((const uint8_t *)buffer, len);
    if (buffer != temp) {
        delete[] buffer;
    }
    return len;
}

// size_t Print::print(const String &s) {
//    return write(s.c_str(), s.length());
//}

size_t Print::print(const char str[]) {
    return write(str);
}

size_t Print::print(char c) {
    return write(c);
}

size_t Print::print(unsigned char b, int base) {
    return print((unsigned long)b, base);
}

size_t Print::print(int n, int base) {
    return print((long)n, base);
}

size_t Print::print(unsigned int n, int base) {
    return print((unsigned long)n, base);
}

size_t Print::print(long n, int base) {
    if (base == 0) {
        return write((uint8_t)n);
    } else if (base == 10) {
        if (n < 0) {
            int t = print('-');
            n = -n;
            return printNumber(n, 10) + t;
        }
        return printNumber(n, 10);
    } else {
        return printNumber(n, base);
    }
}

size_t Print::print(unsigned long n, int base) {
    if (base == 0)
        return write((uint8_t)n);
    else
        return printNumber(n, base);
}

size_t Print::print(double n, int digits) {
    return printFloat(n, digits);
}

size_t Print::println(const __FlashStringHelper *ifsh) {
    size_t n = print(ifsh);
    n += println();
    return n;
}

size_t Print::print(const Printable &x) {
    return x.printTo(*this);
}

size_t Print::println(void) {
    return print("\r\n");
}

// size_t Print::println(const String &s) {
//    size_t n = print(s);
//    n += println();
//    return n;
//}

size_t Print::println(const char c[]) {
    size_t n = print(c);
    n += println();
    return n;
}

size_t Print::println(char c) {
    size_t n = print(c);
    n += println();
    return n;
}

size_t Print::println(unsigned char b, int base) {
    size_t n = print(b, base);
    n += println();
    return n;
}

size_t Print::println(int num, int base) {
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(unsigned int num, int base) {
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(long num, int base) {
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(unsigned long num, int base) {
    size_t n = print(num, base);
    n += println();
    return n;
}

size_t Print::println(double num, int digits) {
    size_t n = print(num, digits);
    n += println();
    return n;
}

size_t Print::println(const Printable &x) {
    size_t n = print(x);
    n += println();
    return n;
}

// Private Methods /////////////////////////////////////////////////////////////

size_t Print::printNumber(unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1];  // Assumes 8-bit chars plus zero byte.
    char *str = &buf[sizeof(buf) - 1];

    *str = '\0';

    // prevent crash if called with base == 1
    if (base < 2) base = 10;

    do {
        unsigned long m = n;
        n /= base;
        char c = (char)(m - base * n);
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while (n);

    return write(str);
}

size_t Print::printFloat(double number, uint8_t digits) {
    size_t n = 0;

    if (isnan(number)) return print("nan");
    if (isinf(number)) return print("inf");
    if (number > 4294967040.0) return print("ovf");  // constant determined empirically
    if (number < -4294967040.0) return print("ovf");  // constant determined empirically

    // Handle negative numbers
    if (number < 0.0) {
        n += print('-');
        number = -number;
    }

    // Round correctly so that print(1.999, 2) prints as "2.00"
    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; ++i) rounding /= 10.0;

    number += rounding;

    // Extract the integer part of the number and print it
    unsigned long int_part = (unsigned long)number;
    double remainder = number - (double)int_part;
    n += print(int_part);

    // Print the decimal point, but only if there are digits beyond
    if (digits > 0) {
        n += print(".");
    }

    // Extract digits from the remainder one at a time
    while (digits-- > 0) {
        remainder *= 10.0;
        int toPrint = int(remainder);
        n += print(toPrint);
        remainder -= toPrint;
    }

    return n;
}

size_t Print::print(const __FlashStringHelper *ifsh) {
    PGM_P p = reinterpret_cast<PGM_P>(ifsh);

    size_t n = 0;
    while (1) {
        uint8_t c = *p++;
        if (c == 0) break;
        n += write(c);
    }
    return n;
}

static bool winsock_initialized = false;

static void init_winsock() {
    int iResult;
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"WSAStartup failed with error: %d\n", iResult);
        exit(-1);
    }
    winsock_initialized = true;
}

WiFiUDP::WiFiUDP() {
    if (!winsock_initialized) {
        init_winsock();
    }
    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    _buffer = nullptr;
    _clear();
}

WiFiUDP::~WiFiUDP() {
    closesocket(_socket);
    free(_buffer);
}

int WiFiUDP::beginPacket(const char *host, uint16_t port) {
    struct hostent *dns;
    if (!(dns = gethostbyname(host))) {
        return 0;
    }
    if (dns->h_addrtype != AF_INET) {
        return 0;
    }
    _dst.sin_addr.S_un.S_addr = *(u_long *)dns->h_addr_list[0];
    return _beginPacket(port);
}

int WiFiUDP::beginPacket(IPAddress ip, uint16_t port) {
    _dst.sin_addr.S_un.S_addr = inet_addr(ip.toString().c_str());
    return 1;
}

int WiFiUDP::_beginPacket(uint16_t port) {
    _clear();
    _buffer = (char *)malloc(0xffff);
    if (!_buffer) {
        return 0;
    }
    _dst.sin_family = AF_INET;
    _dst.sin_port = port;
    return 1;
}

size_t WiFiUDP::write(uint8_t byte) {
    if (_buffer && _len < 0xffff) {
        _buffer[_len++] = byte;
        return 1;
    }
    return 0;
}

size_t WiFiUDP::write(const uint8_t *buffer, size_t size) {
    const uint8_t *ptr = buffer;
    while (size--) {
        if (write(*ptr) != 1) {
            break;
        }
        ptr++;
    }
    return ptr - buffer;
}

void WiFiUDP::flush() {
    endPacket();
}

int WiFiUDP::endPacket() {
    if (_socket == INVALID_SOCKET || !_buffer || !_dst.sin_family) {
        return 0;
    }

    int result = sendto(_socket, _buffer, _len, 0, (SOCKADDR *)&_dst, sizeof(_dst));
    _clear();

    if (result == SOCKET_ERROR) {
        return 0;
    }
    return 1;
}

void WiFiUDP::_clear() {
    _len = 0;
    _dst.sin_family = 0;
    free(_buffer);
    _buffer = nullptr;
}

ESP8266WiFiClass WiFi;

void Dir::__test() {
	Dir dir = SPIFFS.openDir("./");
	while (dir.next()) {
		if (dir.isFile()) {
			printf("is_file: Dir::fileName() %s Dir::fileSize() %d\n", dir.fileName().c_str(), dir.fileSize());
			File file = dir.openFile("r");
			printf("File::size %d\n", file.size());
			file.close();
		} else if (dir.isDirectory()) {
            printf("is_dir: Dir::fileName() %s\n", dir.fileName().c_str());
		}
	}
}

File Dir::openFile(const char *mode) {
    return SPIFFS.open(fileName(), mode);
}

#endif
