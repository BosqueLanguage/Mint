#pragma once

#include "../common.h"

#define SERVER_STRING "Server: Bosque RSHook\r\n"

#define READ_BYTE_SIZE 8192

/* Max number of entries in the request queue */
#define QUEUE_DEPTH 256

/* Max size that we allow for HTTP requests */
#define HTTP_MAX_REQUEST_SIZE 8192

#define EVENT_TYPE_ACCEPT 0
#define EVENT_TYPE_READ   1
#define EVENT_TYPE_WRITE  2

void initialize(const char* static_files_root);

std::optional<std::string> setup_listening_socket(int port, int& sock);
int server_loop(int server_socket);