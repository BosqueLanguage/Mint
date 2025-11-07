#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>

#define ENABLE_CONSOLE_STATUS 1
#define ENABLE_CONSOLE_LOGGING 1

bool get_line(const char *src, char *dest, size_t dest_sz);
const char* get_filename_ext(const char* filename);
