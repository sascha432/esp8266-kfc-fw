/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef SESSION_RNG_RANDOM
#define SESSION_RNG_RANDOM                      1
#endif

#ifndef HAVE_SESSION_DEVICE_TOKEN
#define HAVE_SESSION_DEVICE_TOKEN               1
#endif

#ifndef SESSION_DEVICE_TOKEN_MIN_LENGTH
#define SESSION_DEVICE_TOKEN_MIN_LENGTH         16
#endif

#if SESSION_RNG_RANDOM
#include <RNG.h>
extern RNGClass rng;
#endif

#if defined(ESP32) || defined(ESP8266)

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

#elif 0

// old authentication

#include "SHA1.h"

class SessionHash : public SHA1
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

