/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if defined(ESP8266)

#include <Arduino.h>

#ifdef ARDUINO_ESP8266_RELEASE_2_6_3

extern "C" {
    #include <bearssl/bearssl_hash.h>
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
        br_sha1_init(&context);
    }

    void update(const void *data, size_t len) {
        br_sha1_update(&context, (uint8_t *)data, len);
    }

    void finalize(void *hash, size_t len) {
        if (len == hashSize()) {
            br_sha1_out(&context, (uint8_t *)hash);
        } else {
            uint8_t buf[hashSize()];
            bzero(hash, len);
            br_sha1_out(&context, buf);
            memcpy(hash, buf, len > sizeof(buf) ? sizeof(buf) : len);
        }
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
