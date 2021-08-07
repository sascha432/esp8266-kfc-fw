/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef HAVE_SESSION_DEVICE_TOKEN
#define HAVE_SESSION_DEVICE_TOKEN               1
#endif

#ifndef SESSION_DEVICE_TOKEN_MIN_LENGTH
#define SESSION_DEVICE_TOKEN_MIN_LENGTH         16
#endif

#if defined(ESP8266) && ARDUINO_ESP8266_MAJOR >= 3

#include <bearssl/bearssl.h>

namespace Session {

    class SHA256
    {
    public:
        static constexpr size_t kHashSize = br_sha256_SIZE;

    public:
        SHA256();

        constexpr size_t hashSize() const {
            return kHashSize;
        }

        void reset();
        void update(const void *data, size_t len);

        template<size_t _Len>
        inline __attribute__((always_inline)) void finalize(void *hash)
        {
            static_assert(_Len == kHashSize, "invalid size");
            br_sha256_out(&_context, hash);
        }

    private:
        br_sha256_context _context;
    };

    inline SHA256::SHA256()
    {
        reset();
    }

    inline __attribute__((always_inline)) void SHA256::reset()
    {
        br_sha256_init(&_context);
    }

    inline __attribute__((always_inline)) void SHA256::update(const void *data, size_t len)
    {
        br_sha256_update(&_context, data, len);
    }

}

class SessionHash : public Session::SHA256
{
public:
    using HashBuffer = uint8_t[SHA256::kHashSize];
    using SaltBuffer = uint8_t[8];

    static constexpr size_t kRounds = 1024;
    static constexpr size_t kSessionIdSize = (sizeof(SaltBuffer) + sizeof(HashBuffer)) * 2;
};


#elif ESP8266

#include <SHA256.h>

class SessionHash : public SHA256
{
public:
    using SHA256::SHA256;
    using HashBuffer = uint8_t[32];
    using SaltBuffer = uint8_t[8];

    static constexpr size_t kRounds = 1024;
    static constexpr size_t kSessionIdSize = (sizeof(SaltBuffer) + sizeof(HashBuffer)) * 2;
};

#elif ESP32

#include "az_iot/c-utility/inc/azure_c_shared_utility/sha.h"

namespace Session {

    class SHA256
    {
    public:
        static constexpr size_t kHashSize = SHA256HashSize;

    public:
        SHA256();

        static constexpr size_t hashSize() {
            return kHashSize;
        }

        void reset();
        void update(const void *data, size_t len);

        template<size_t _Len>
        inline __attribute__((always_inline)) void finalize(void *hash)
        {
            static_assert(_Len == kHashSize, "invalid size");
            SHA256Result(&_context, hash);
        }

    private:
        SHA256Context _context;
    };

    inline SHA256::SHA256()
    {
        reset();
    }

    inline __attribute__((always_inline)) void SHA256::reset()
    {
        SHA256Reset(&_context);
    }

    inline __attribute__((always_inline)) void SHA256::update(const void *data, size_t len)
    {
        SHA256Input(&_context, reinterpret_cast<const uint8_t *>(data), len);
    }

}

class SessionHash : public Session::SHA256
{
public:
    using SHA256::SHA256;
    using HashBuffer = uint8_t[SHA256::kHashSize];
    using SaltBuffer = uint8_t[8];

    static constexpr size_t kRounds = 1024;
    static constexpr size_t kSessionIdSize = (sizeof(SaltBuffer) + sizeof(HashBuffer)) * 2;
};


#elif 0

// old authentication

#include "SHA1.h"

class SessionHash : public Session::SHA1
{
public:
    using SHA1::SHA1;
    using HashBuffer = uint8_t[20];
    using SaltBuffer = uint8_t[8];

    static constexpr size_t kRounds = 1024;
    static constexpr size_t kSessionIdSize = (sizeof(SaltBuffer) + sizeof(HashBuffer)) * 2;
};

#else

#error Platform not supported

#endif

const uint8_t *pepper_and_salt(const uint8_t *originalSalt, SessionHash &hash, SessionHash::SaltBuffer &saltBuffer);

String generate_session_id(const char *username, const char *password, const uint8_t *salt);
bool verify_session_id(const char *sessionId, const char *username, const char *password);

