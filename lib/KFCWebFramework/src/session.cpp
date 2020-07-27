/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "session.h"
#include "misc.h"
#include "kfc_fw_config_classes.h"

using KFCConfigurationClasses::System;

RNGClass rng;

const uint8_t *pepper_and_salt(const uint8_t *originalSalt, SessionHash &hash, SessionHash::SaltBuffer &saltBuffer)
{
    uint8_t *salt;
    if (originalSalt) {
        salt = const_cast<uint8_t *>(originalSalt);
    }
    else {
        salt = saltBuffer;
        rng.rand(salt, sizeof(saltBuffer));
    }
    hash.update(salt, sizeof(saltBuffer));
    return salt;
}

String generate_session_id(const char *username, const char *password, const uint8_t *salt)
{
    String sid;
    SessionHash hash;
    SessionHash::HashBuffer buf;
    SessionHash::SaltBuffer saltBuf;

    auto newSalt = pepper_and_salt(salt, hash, saltBuf);
    hash.update(password, strlen(password));
    hash.update(username, strlen(username));
    hash.finalize(buf, sizeof(buf));

    for(size_t i = 0; i < SessionHash::kRounds; i++) {
        hash.reset();
        hash.update(buf, sizeof(buf));
        hash.update(newSalt, sizeof(saltBuf));
        hash.finalize(buf, sizeof(buf));
    }

    bin2hex_append(sid, newSalt, sizeof(saltBuf));
    bin2hex_append(sid, buf, sizeof(buf));

    return sid;
}

bool verify_session_id(const char *sessionId, const char *username, const char *password)
{
#if HAVE_SESSION_DEVICE_TOKEN

    auto token = System::Device::getToken();
    if (token && strlen(token) >= System::Device::kTokenMinSize && !strcmp(sessionId, token)) {
        return true;
    }

#endif

    if (strlen(sessionId) != SessionHash::kSessionIdSize) {
        return false;
    }
    SessionHash::SaltBuffer salt;
    hex2bin(salt, sizeof(salt), sessionId); // get the salt from the beginning of the session id
    return generate_session_id(username, password, salt).equals(sessionId);
}
