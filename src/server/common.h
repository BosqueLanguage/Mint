#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <unistd.h>

#include <thread>
#include <stdatomic.h>
#include <linux/futex.h>

#include "liburing.h"

#define ENABLE_CONSOLE_STATUS 1
#define ENABLE_CONSOLE_LOGGING 1

#define HTTP_MAX_REQUEST_BUFFER_SIZE 8192

const char* get_filename_ext(const char* filename);

size_t s_strlen(const char* str);