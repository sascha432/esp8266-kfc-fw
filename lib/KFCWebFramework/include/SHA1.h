/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if defined(ESP8266)

#include <Arduino.h>

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
