/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "session.h"

RNGClass rng;

void  bin2hex_append(String &str, char *data, int length) {
    char *p = data;
    char buf[3];
    while (length--) {
        snprintf(buf, 3, "%02x", (int)*p++);
        str += buf;
    }
}

void  hex2bin(char *buf, int length, const char *str) {
    char hex[3];
    const char *p = str;
    uint8_t *q = (uint8_t *)buf;
    hex[2] = 0;
    while (length--) {
        if ((hex[0] = *p++) == 0 || (hex[1] = *p++) == 0) {
            break;
        }
        *q++ = (unsigned char)strtoul(hex, nullptr, HEX);
    }
}

char *pepper_and_salt(char *salt, SESSION_HASH_CLASSNAME &hash, char *buf, size_t size) {
    if (!salt) {
        salt = (char *)malloc(SESSION_SALT_LENGTH);
        rng.rand((uint8_t *)salt, SESSION_SALT_LENGTH);
    }
    hash.update(salt, SESSION_SALT_LENGTH);
    if (buf) {
        hash.finalize(buf, size);
    }
    return salt;
}

void free_pepper_and_salt(char *org_salt, char *salt) {
    if (org_salt != salt) {
        free(salt);
    }
}

String generate_session_id(const char *username, const char *password, char *salt) {

    String sid;
    SESSION_HASH_CLASSNAME hash;
    char buf[hash.hashSize()];

    char *new_salt = pepper_and_salt(salt, hash);
    hash.update(password, strlen(password));
    hash.update(username, strlen(username));
    hash.finalize(buf, sizeof(buf));

    for(int i = 0; i < SESSION_ROUNDS; i++) {
        hash.reset();
        hash.update(buf, sizeof(buf));
        hash.update(new_salt, SESSION_SALT_LENGTH);
        hash.finalize(buf, sizeof(buf));
    }

    bin2hex_append(sid, new_salt, SESSION_SALT_LENGTH);
    bin2hex_append(sid, buf, sizeof(buf));

    free_pepper_and_salt(salt, new_salt);

    return sid;
}

bool verify_session_id(const char *session_id, const char *username, const char *password) {
    char salt[8];

    if (strlen(session_id) != (SESSION_SALT_LENGTH + SESSION_HASH_LENGTH) * 2) {
        // debug_printf("SID length %d != %d\n", strlen(session_id), (8 + SESSION_HASH_LENGTH) * 2);
        return false;
    }
    hex2bin(salt, SESSION_SALT_LENGTH, session_id);
    // #if DEBUG
    // String tmp;
    // bin2hex_append(tmp, salt, 8);
    // debug_printf("SID %s, generated SID %s, salt %s, user %s, pass %s\n", session_id, generate_session_id(username, password, salt).c_str(), tmp.c_str(), username, password);
    // #endif
    return generate_session_id(username, password, salt).equals(session_id);
}
