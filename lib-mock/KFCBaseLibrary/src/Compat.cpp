/**
  Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#include "Arduino_compat.h"
#include <chrono>
#include <section_defines.h>
#include <ets_timer_win32.h>
#include <winternl.h>
#include <algorithm>
#include <BufferStream.h>
#include <DumpBinary.h>
#include "EEPROM.h"
#include <pgmspace.h>
#include <list>

EEPROMClass EEPROM;
_SPIFFS SPIFFS;

char *dtostrf(double val, signed char width, unsigned char prec, char *s) 
{
    sprintf(s, "%*.*f", width, prec, val);
    return s;
}

uint32_t crc32(const void *data, size_t length, uint32_t crc)
{
    const uint8_t *ldata = reinterpret_cast<const uint8_t *>(data);
    while (length--) {
        uint8_t c = pgm_read_byte(ldata++);
        for (uint32_t i = 0x80; i > 0; i >>= 1) {
            bool bit = crc & 0x80000000;
            if (c & i) {
                bit = !bit;
            }
            crc <<= 1;
            if (bit) {
                crc ^= 0x04c11db7;
            }
        }
    }
    return crc;
}

//const String emptyString;

// flash memory is a flash emulation that creates and deduplicates data, adding usage counter and a ESP8266 flash address
// all data is dword aligned
// TODO the memory is write protected and can be set to read protected that only functions like strcmp_P(), memcpy_P() can access it

#pragma push_macro("new")
#undef new

class flash_memory {
public:
    using Vector = std::vector<flash_memory>;
    using Iterator = Vector::iterator;
    using IteratorPair = std::pair<Iterator, Iterator>;
    using ConstIterator = Vector::const_iterator;
    using ConstIteratorPair = std::pair<ConstIterator, ConstIterator>;

public:
    flash_memory(const flash_memory &) = delete;
    flash_memory &operator=(const flash_memory &) = delete;

    flash_memory(flash_memory &&move) noexcept :
        _begin(std::exchange(move._begin, nullptr)),
        _end(std::exchange(move._end, nullptr)),
        _address(std::exchange(move._address, 0)),
        _deduplicated(std::exchange(move._deduplicated, 0))
    {
    }

    flash_memory &operator=(flash_memory &&move) noexcept
    {
        this->~flash_memory();
        ::new(static_cast<void *>(this)) flash_memory(std::move(move));
        return *this;
    }

    // copy object into flash memory
    template<typename _Ta>
    flash_memory(const _Ta &arg, size_t alignment) : flash_memory(sizeof(_Ta), alignment) {
        new(static_cast<void *>(_begin)) _Ta(arg);
    }

    // move object into flash memory
    template<typename _Ta>
    flash_memory(_Ta &&arg, size_t alignment) : flash_memory(sizeof(_Ta), alignment) {
        new(static_cast<void *>(_begin)) _Ta() = std::move(arg);
    }

    // create flash memory from string
    flash_memory(const char *str, size_t alignment) : flash_memory(str, strlen_P(str), alignment) {
    }

    // create flash memory string with specific size
    // the string will be truncated at length or zero padded if shorter
    flash_memory(const char *str, size_t length, size_t alignment) : flash_memory(length + 1, alignment) {
        std::fill(std::copy_n(str, std::min(strlen_P(str), length), _begin), _end, 0);
    }

    // create flash memory  and copy data from begin to end
    flash_memory(const void *begin, const void *end, size_t alignment) : flash_memory((const uint8_t *)begin, (const uint8_t *)end, alignment) {}

    // create flash memory and copy data from begin to end
    flash_memory(const uint8_t *begin, const uint8_t *end, size_t alignment) : _begin(_allocate(begin, end, (uint8_t)alignment)), _end(_begin + _align(end - begin, (uint8_t)alignment)), _address(_get_address()), _deduplicated(0)
    {
        std::fill(std::copy(begin, end, _begin), _end, 0);
    }


protected:
    flash_memory(size_t size, size_t alignment) : _begin(_allocate(size, (uint8_t)alignment)), _end(_begin + _align(size, (uint8_t)alignment)), _address(_get_address()), _deduplicated(0) {
        std::fill(_begin, _end, 0);
    }

    /*flash_memory(const void *ptr) : _begin((uint8_t *)ptr), _end(nullptr), _address(0), _deduplicated(0) {
    }*/

public:
    ~flash_memory() {
        if (_begin) {
            free(_begin);
        }
    }

    uint8_t *_allocate(size_t size, uint8_t alignment) {
        return (uint8_t *)malloc(_align(size, alignment));
    }

    uint8_t *_allocate(const uint8_t *begin, const uint8_t *end, uint8_t alignment) {
        return (uint8_t *)malloc(_align(end - begin, alignment));
    }

    const uint8_t *begin() const {
        return _begin;
    }

    const uint8_t *end() const {
        return _end;
    }

    size_t size() const {
        return _end - _begin;
    }

    const char *c_str() const {
        return (const char *)_begin;
    }

    size_t length() const {
        auto ptr = std::find(_begin, _end, 0);
        if (ptr != _end) {
            return (ptr - _begin) - 1;
        }
        return 0;
    }

    bool in_range(const uint8_t *ptr) const {
        return ptr >= _begin && ptr < _end;
    }

    bool in_range(const void *ptr) const {
        return in_range(reinterpret_cast<const uint8_t *>(ptr));
    }

    bool operator<(const flash_memory &fm) const {
        return _begin < fm._begin;
    }

    bool operator==(const flash_memory &fm) const {
        return _length() == fm._length() && memcmp_P(_begin, fm._begin, _length()) == 0;
    }

    bool operator==(const uint8_t *ptr) const {
        return _begin == ptr;
    }

    bool operator==(const void *ptr) const {
        return _begin == ptr;
    }

    bool operator==(const char *str) const {
        return _begin == (const uint8_t *)str;
    }

    bool operator==(const __FlashStringHelper *fpstr) const {
        return _begin == (const uint8_t *)fpstr;
    }

    const void *esp8266_address() {
        return (const void *)_address;
    }

    size_t deduplicated() const {
        return _deduplicated;
    }

    void duplicate() {
        _deduplicated++;
    }

    static IteratorPair find_pointer_in_range(const uint8_t *ptr) {
        return std::make_pair(
            std::lower_bound(_vector.begin(), _vector.end(), ptr, _comp_lower_end), 
            std::upper_bound(_vector.begin(), _vector.end(), ptr, _comp_upper_begin)
        );
    }

    static Iterator find_pointer(const uint8_t *ptr) {
        return std::lower_bound(_vector.begin(), _vector.end(), ptr, _comp_lower_begin);
    }

    static const char *find_str(const char *str) {
        auto iterator = find_pointer((const uint8_t *)str);
        if (iterator != _vector.end() && iterator->c_str() == str) {
            return iterator->c_str();
        }
        return nullptr;    
    }

    static const uint8_t *create_data(const uint8_t *ptr, size_t length, size_t alignment)
    {
        // we need to create an item to compare values or it gets complicated with zero padding and alignment
        flash_memory item(ptr, ptr + length, alignment);
        // find duplicates
        auto iterator = std::find(_vector.begin(), _vector.end(), item);
        if (iterator == _vector.end()) {
            // insert sorted
            return _vector.insert(std::upper_bound(_vector.begin(), _vector.end(), item), std::move(item))->begin();
        }
        // increase duplicate counter and discard item
        iterator->duplicate();
        return iterator->begin();
    }

protected:
    static bool _comp_upper_begin(const uint8_t *ptr, const flash_memory &item) {
        return ptr < item._begin;
    }

    static bool _comp_upper_end(const uint8_t *ptr, const flash_memory &item) {
        return ptr < item._end;
    }

    static bool _comp_lower_begin(const flash_memory &item, const uint8_t *ptr) {
        return item._begin < ptr;
    }

    static bool _comp_lower_end(const flash_memory &item, const uint8_t *ptr) {
        return item._end < ptr;
    }

    size_t _align(size_t size, uint8_t alignment) const {
        switch (alignment) {
        case 0:
        case 1:
            return size;
        case 2:
            return (size + 1) & ~1;
        case 4:
            return (size + 3) & ~3;
        case 8:
            return (size + 7) & ~7;
        }
        uint8_t padding = (size % alignment);
        if (padding == 0) {
            return size;
        }
        return size + alignment - padding;
    }

    size_t _length() const {
        return _end - _begin;
    }

    uintptr_t _get_address() const {
        return SECTION_IROM0_TEXT_END_ADDRESS - _length();
    }

    uint8_t *_begin;
    uint8_t *_end;
    uintptr_t _address;
    size_t _deduplicated;
public:
    static Vector _vector;
};

flash_memory::Vector flash_memory::_vector;

#pragma pop_macro("new")

//
//void __clear_flash_memory() {
//    flash_memory::_vector.clear();
//}
//
//const uint8_t *__flash_memory_front() {
//    return flash_memory::_vector.front().begin();
//}
//
//const uint8_t *__flash_memory_back() {
//    return flash_memory::_vector.back().end();
//}
//
//const char *__flash_memory_list() {
//
//    for (auto &item : flash_memory::_vector) {
//        Serial.printf("begin=%p end=%p len=%u str=%s\n", item.begin(), item.end(), item.size(), item.c_str());
//    }
//    return flash_memory::_vector.at(flash_memory::_vector.size() / 2).c_str();
//}


const void *__register_flash_memory(const void *ptr, size_t length, size_t alignment)
{
    return flash_memory::create_data((const uint8_t *)ptr, length, alignment);
}

const char *__register_flash_memory_string(const void *str, size_t alignment)
{
    return (const char *)flash_memory::create_data((const uint8_t *)str, strlen_P((const char *)str) + 1, alignment);
}

const char *__find_flash_memory_string(const char *str) 
{
    return flash_memory::find_str(str);
}

bool is_PGM_P(const void *ptr)
{
    // there are gaps between begin-end and the next begin-end
    // get range and check if there is a match
    auto range = flash_memory::find_pointer_in_range((const uint8_t *)ptr);
    for (auto iterator = range.first; iterator != range.second; ++iterator) {
        if (iterator->in_range(ptr)) {
            return true;
        }
    }
    return false;
}

void throwException(PGM_P message) 
{
    printf("EXCEPTION: %s\n", (const char *)message);
    exit(-1);
}

class GPIOPin {
public:
    GPIOPin() {
        mode = 0;
        value = 0;
    }
    uint8_t mode;
    int value;
};

GPIOPin pins[256];

void pinMode(uint8_t pin, uint8_t mode)
{
    pins[pin].mode = mode;
    printf("pinMode(%u, %u)\n", pin, mode);
}


void digitalWrite(uint8_t pin, uint8_t value)
{
    pins[pin].value = value;
    printf("digitalWrite(%u, %u)\n", pin, value);
}

int digitalRead(uint8_t pin)
{
    return pins[pin].value ? HIGH : LOW;
}

int analogRead(uint8_t pin)
{
    return pins[pin].value;
}

void analogReference(uint8_t mode)
{
}

void analogWrite(uint8_t pin, int value)
{
    pins[pin].value = value;
    printf("analogWrite(%u, %u)\n", pin, value);
}

void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
}

uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder)
{
    return uint8_t();
}

void attachInterrupt(uint8_t, void(*)(void), int mode)
{
}

void detachInterrupt(uint8_t) {

}

static bool panic_message_box_active = false;

static bool winsock_initialized = false;

void init_winsock() {
    if (!winsock_initialized) {
        int iResult;
        WSADATA wsaData;
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != NO_ERROR) {
            wprintf(L"WSAStartup failed with error: %d\n", iResult);
            exit(-1);
        }
        winsock_initialized = true;
    }
}




EspClass ESP;

void EspClass::rtcMemDump() {
    if (_rtcMemory) {
        DumpBinary dump(Serial);
        dump.dump(_rtcMemory + rtcMemoryReserved, rtcMemoryUser);
    }
}

void EspClass::rtcClear() {
    if (_rtcMemory) {
        memset(_rtcMemory, 0, rtcMemorySize);
        _writeRtcMemory();
    }
}

unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout)
{
    return 0;
}

unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout)
{
    return 0;
}

uint16_t __builtin_bswap16(uint16_t word) {
    return ((word >> 8) & 0xff) | ((word << 8) & 0xffff);
}

#endif

