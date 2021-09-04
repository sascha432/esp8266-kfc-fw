
#include "SafeRead.h"

#if 0
#    include <debug_helper_disable_ptr_validation.h>
#endif

static uint8_t *buffer = nullptr;
static size_t pos;
static size_t buffer_size;

static void __check_buffer()
{
    if (!buffer) {
        size_t size = 1024 * 2;
        pos = 0;
        while(true) {
            buffer = (uint8_t *)malloc(size);
            if (buffer) {
                break;
            }
            size -= 256;
        }
        buffer_size = size;
    }
}

bool is_safe_ptr(const uint8_t *ptr)
{
    __DBG_validatePointerCheck(ptr, VP_HPS);
#if _MSC_VER
    return true;
#else
    uintptr_t addr = (uintptr_t)ptr;
    return (addr >= SECTION_HEAP_START_ADDRESS && addr <= SECTION_HEAP_END_ADDRESS) || (addr >= SECTION_FLASH_START_ADDRESS && addr <= SECTION_FLASH_END_ADDRESS);
#endif
}

bool is_safe_ptr(const void *ptr)
{
    return is_safe_ptr(reinterpret_cast<const uint8_t *>(ptr));
}


size_t safe_read(uint8_t *buffer, const uint8_t *data, size_t len, uint16_t stop)
{
    __DBG_validatePointerCheck(data, VP_HPS);
    __DBG_validatePointerCheck(buffer, VP_HPS);
    auto dst = buffer;
    auto src = data;
    auto flag = true;
    while (len-- && (flag = is_safe_ptr(dst))) {
        uint8_t byte = pgm_read_byte(src);
        *dst++ = byte;
        src++;
        if (byte == stop) {
            break;
        }
    }
    if (!flag) {
        Serial.printf_P(PSTR("unsafe pointer: %p\n"), data);
    }
    return dst - buffer;
}


const char *safe_read(const char *ptr, size_t len)
{
    __DBG_validatePointerCheck(ptr, VP_HPS);
#if _MSC_VER
    return ptr;
#else
    __check_buffer();
    if (pos + len >= buffer_size - 2) {
        pos = 0;
    }
    auto start = &buffer[pos];
    memset(start, 0xcc, len);
    safe_read(start, (const uint8_t *)ptr, len, 0xffff);
    pos += len;
    buffer[pos++] = 0;
    return (const char *)start;
#endif
}


const char *safe_read(const char *ptr)
{
    __DBG_validatePointerCheck(ptr, VP_HPS);
#if _MSC_VER
    return ptr;
#else
    __check_buffer();
    size_t len = 64;
    if (pos + len >= buffer_size - 2) {
        pos = 0;
    }
    auto start = &buffer[pos];
    memset(start, 0xcc, len);
    len = safe_read(start, (const uint8_t *)ptr, len, 0);
    pos += len;
    buffer[pos++] = 0;
    return (const char *)start;
#endif
}

uint32_t safe_read(const uint32_t *ptr)
{
    __DBG_validatePointerCheck(ptr, VP_HPS);
#if _MSC_VER
    return *ptr;
#else
    uint32_t result = 0;
    safe_read((uint8_t *)&result, (const uint8_t *)ptr, sizeof(result), 0xffff);
    return result;
#endif
}

uintptr_t safe_read_uintptr(const uintptr_t *ptr)
{
    __DBG_validatePointerCheck(ptr, VP_HPS);
#if _MSC_VER
    return *ptr;
#else
    uintptr_t result = 0;
    safe_read((uint8_t *)&result, (const uint8_t *)ptr, sizeof(result), 0xffff);
    return result;
#endif
}
