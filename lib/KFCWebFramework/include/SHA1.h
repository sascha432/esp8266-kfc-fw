/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if defined(ESP8266)

#include <Arduino.h>
#include "global.h"

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603

extern "C" {
    #include <bearssl/bearssl_hash.h>
}

class SHA1
{
public:
    static constexpr size_t kHashSize = 20;
public:
    SHA1() {
        reset();
    }

    constexpr size_t hashSize() const {
        return kHashSize;
    }

    inline __attribute__((always_inline)) void reset() {
        br_sha1_init(&context);
    }

    inline __attribute__((always_inline)) void update(const void *data, size_t len) {
        br_sha1_update(&context, data, len);
    }

    // inline __attribute__((always_inline)) void finalize(void *hash, size_t len) {
    //     finalize(reinterpret_cast<uint8_t *>(hash), len);
    // }

    // void finalize(uint8_t *hash, size_t len) {
    //     if (len == hashSize()) {
    //         br_sha1_out(&context, hash);
    //     }
    //     else {
    //         uint8_t buf[hashSize()];
    //         br_sha1_out(&context, buf);
    //         std::fill(std::copy_n(buf, std::min(sizeof(buf), len), hash), hash + len, 0);
    //     }
    // }

    template<size_t _Len>
    inline __attribute__((always_inline)) void finalize(void *hash)
    {
        static_assert(_Len == kHashSize, "invalid size");
        br_sha1_out(&context, hash);
    }

private:
    br_sha1_context context;
};

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
