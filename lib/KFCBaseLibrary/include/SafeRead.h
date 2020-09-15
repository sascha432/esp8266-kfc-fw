
#pragma once

#include <Arduino_compat.h>

const char *safe_read(const char *ptr);
const char *safe_read(const char *, size_t);
uint32_t safe_read(const uint32_t *);
uintptr_t safe_read_uintptr(const uintptr_t *);

bool is_safe_ptr(const uint8_t *ptr);
bool is_safe_ptr(const void *ptr);
