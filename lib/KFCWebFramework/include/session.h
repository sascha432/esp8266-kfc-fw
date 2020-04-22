/**
 * Author: sascha_lammers@gmx.de
 */

#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED

#include <Arduino_compat.h>

#ifndef SESSION_CONFIGURATION_SET
#define SESSION_RNG_RANDOM                      1
#define SESSION_SALT_LENGTH                     8
#endif

#if SESSION_RNG_RANDOM
#include <RNG.h>
extern RNGClass rng;
#endif

#ifndef SESSION_CONFIGURATION_SET
#define SESSION_CONFIGURATION_SET               1

#ifdef HAVE_SESSION_DEVICE_TOKEN
#define HAVE_SESSION_DEVICE_TOKEN               1
#endif

#ifndef SESSION_DEVICE_TOKEN_MIN_LENGTH
#define SESSION_DEVICE_TOKEN_MIN_LENGTH         16
#endif

#if HAVE_SESSION_DEVICE_TOKEN
extern const char *session_get_device_token();
#endif


#if defined(ESP32) || defined(ESP8266)

#include <SHA256.h>
#define SESSION_ROUNDS              1024
#define SESSION_HASH_LENGTH         32
#define SESSION_HASH_CLASSNAME      SHA256

#elif false //defined(ESP8266)

#include "SHA1.h"
#define SESSION_ROUNDS              1024
#define SESSION_HASH_LENGTH         20
#define SESSION_HASH_CLASSNAME      SHA1

#else

#error Platform not supported

#endif

#endif

void bin2hex_append(String &str, char *data, int length);
void hex2bin(char *buf, int length, const char *str);

char *pepper_and_salt(char *org_salt, SESSION_HASH_CLASSNAME &hash, char *buf = nullptr, size_t size = 0);
void free_pepper_and_salt(char *org_salt, char *salt);

String generate_session_id(const char *username, const char *password, char *salt);
bool verify_session_id(const char *session_id, const char *username, const char *password);

#endif
