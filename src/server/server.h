#pragma once

#include "../common.h"

#define READ_BYTE_SIZE 8192

/* Max number of entries in the request queue */
#define QUEUE_DEPTH 256

/* Max size that we allow for HTTP requests */
#define HTTP_MAX_REQUEST_SIZE 8192

bool setup_listening_socket(int port, int& sock);
int server_loop(int server_socket);

/**
 * Links and topics to experiment with:
 * - Futexes for thread synchronization
 * 
 * - Batching system calls
 * 
 * - Multishot accept
 * 
 * - Provided buffers
 *     Looks like perf is not massively improved with provided buffers for networking and I am confused on some details.
 *     We will do pool allocation and reuse in our allocator and, maybe with better workloads, try out provided buffers later.
 * 
 * https://github.com/axboe/liburing/wiki/io_uring-and-networking-in-2023
 * https://developers.redhat.com/articles/2023/04/12/why-you-should-use-iouring-network-io
**/