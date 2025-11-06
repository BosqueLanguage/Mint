#pragma once

#include "common.h"
#include "alloc.h"
#include "fixedmsgs.h"
#include "filemgr.h"

#include <sys/stat.h>
#include <netinet/in.h>

#include <liburing.h>


#define RING_EVENT_TYPE_IO 0x0
#define RING_EVENT_TYPE_ACCEPT 0x1

#define RING_EVENT_IO_FILE_STAT 0x1
#define RING_EVENT_IO_FILE_OPEN 0x2
#define RING_EVENT_IO_FILE_READ 0x3

#define RING_EVENT_IO_CLIENT_READ 0x2
#define RING_EVENT_IO_CLIENT_WRITE 0x3

/**
 * Data structure representing the input to a route handler as extracted from the HTTP request
 **/
struct user_request 
{
    int32_t client_socket;
    const char* route;
    
    size_t size;
    const char* argdata;
};

struct io_event
{
    int32_t io_event_type;
    struct user_request* req;
};

struct io_file_stat_event
{
    struct io_event base;

    const char* file_path;
    struct statx stat_buf;

    bool memoize;
};

struct io_file_open_event
{
    struct io_event base;

    const char* file_path;
    struct statx stat_buf;
    int32_t file_fd;

    bool memoize;
};

struct io_file_read_event
{
    struct io_event base;

    const char* file_path;
    int32_t file_fd;

    size_t size;
    const char* file_data;

    bool memoize;
};

struct io_client_read_event
{
    struct io_event base;

    size_t size;
    const char* client_data;
};

struct io_client_write_event
{
    struct io_event base;

    size_t size;
    const char* msg_data;

    bool should_release;
};

union event {
    struct { int32_t fd; uint32_t op; } data_as_accept;
    struct { struct io_event* io; } data_as_io;
    uint64_t data_as_u64;
};

#define GET_RING_EVENT_TYPE(E) ((E)->data_as_u64 & 0x3)

enum class RSErrorCode
{
    NONE = 0,
    MALFORMED_REQUEST,
    UNSUPPORTED_VERB,
    ROUTE_NOT_FOUND,
    INTERNAL_SERVER_ERROR
};

/** 
 * Callback function type for route handlers that compute a result of some (opaque) type -- nullptr if the call failed
 **/
typedef void (*RouteCB)(io_client_read_event* input, void* result);

class RSHookServer
{
private:
    int port;

    int server_socket;
    struct io_uring ring;

    size_t submission_count;

    ServerAllocator allocator;
    FileCacheManager file_cache_mgr;

    void write_user(struct user_request* req, size_t size, const char* data, bool should_release);

    void send_static_content(struct user_request* req, const char* str) {
        this->write_user(req, strlen(str), str, false);
    }

    void send_cache_file_content(struct user_request* req, const char* path, size_t size, const char* data) {
        this->write_user(req, size, data, false);
    }

    void send_dynamic_content(struct user_request* req, size_t size, const char* data) {
        this->write_user(req, size, data, true);
    }

    void handle_error_code(struct user_request* req, RSErrorCode error_code);

    void send_fstat(struct user_request* req, const char* file_path, bool memoize);
    void send_fopen(struct io_file_stat_event* req);
    void send_read_file(struct io_file_open_event* req);

public:
    RSHookServer();
    ~RSHookServer();

    void startup(int port, int server_socket);
    void shutdown();

    void runloop();
};

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