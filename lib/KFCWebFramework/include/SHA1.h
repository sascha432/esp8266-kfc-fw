/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if defined(ESP8266)

#include <Arduino.h>
#include "global.h"

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603

#include <bearssl/bearssl_hash.h>

namespace Session {

    class SHA1
    {
    public:
        static constexpr size_t kHashSize = br_sha1_SIZE;
    public:
        SHA1() {
            reset();
        }

        constexpr size_t hashSize() const {
            return kHashSize;
        }

        void reset();
        void update(const void *data, size_t len);

        template<size_t _Len>
        inline __attribute__((always_inline)) void finalize(void *hash)
        {
            static_assert(_Len == kHashSize, "invalid size");
            br_sha1_out(&_context, hash);
        }

    private:
        br_sha1_context _context;
    };

    inline __attribute__((always_inline)) void SHA1::reset()
    {
        br_sha1_init(&_context);
    }

    inline __attribute__((always_inline)) void SHA1::update(const void *data, size_t len)
    {
        br_sha1_update(&_context, data, len);
    }

}

#else

extern "C" {
    #include "sha1/sha1.h"
}

class SHA1
{
public:
    SHA1() {
        reset();
    }

    size_t hashSize() const {
        return 20;
    }

    void reset() {
        SHA1Init(&context);
    }
    void update(const void *data, size_t len) {
        SHA1Update(&context, (uint8_t *)data, len);
    }
    void finalize(void *hash, size_t len) {
        if (len == hashSize()) {
            SHA1Final((unsigned char *)hash, &context);
        } else {
            unsigned char buf[hashSize()];
            bzero(hash, len);
            SHA1Final(buf, &context);
            memcpy(hash, buf, len > sizeof(buf) ? sizeof(buf) : len);
        }
    }

private:
    SHA1_CTX context;
};

#endif

#endif
