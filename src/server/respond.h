#pragma once

#include "alloc.h"

void send_static_content(const char *str, int client_socket, struct io_uring* ring, size_t& submissions);