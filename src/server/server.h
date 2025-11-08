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
#define RING_EVENT_IO_FILE_CLOSE 0x4

#define RING_EVENT_IO_CLIENT_READ 0x10
#define RING_EVENT_IO_CLIENT_WRITE 0x20
#define RING_EVENT_IO_CLIENT_WRITE_VECTORED 0x30

#define HTTP_MAX_REQUEST_BUFFER_SIZE 16384

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

struct io_user_request_event
{
    struct io_event base;

    char* http_request_data;
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

struct io_file_close_event
{
    struct io_event base;

    const char* file_path;
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
};

struct io_client_write_event_vectored
{
    struct io_event base;

    struct iovec iov[2];
    int32_t iov_sizes[2]; //-1 if we should not free the corresponding iovec entry
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

class RSHookServer
{
private:
    int port;

    int server_socket;
    struct io_uring ring;

    size_t submission_count;

    ServerAllocator allocator;
    FileCacheManager file_cache_mgr;

    void write_user_direct(struct user_request* req, size_t size, const char* data);
    void write_user_file_contents(struct user_request* req, size_t size, const char* data, bool should_release);
    void write_user_dynamic_response(struct user_request* req, size_t size, const char* data);

    void send_static_content(struct user_request* req, const char* str) {
        this->write_user_direct(req, strlen(str), str);
    }

    void send_cache_file_content(struct user_request* req, const char* path, size_t size, const char* data) {
        this->write_user_file_contents(req, size, data, false);
    }

    void handle_error_code(struct user_request* req, RSErrorCode error_code);

    void process_user_connect(int listen_socket);
    void process_user_request(struct io_user_request_event* event);

    //TODO: process a user action request
    void process_http_file_access(struct user_request* req, const char* file_path, bool memoize);

    void process_fstat_result(struct io_file_stat_event* event);
    void process_fopen_result(struct io_file_open_event* event);
    void process_fread_result(struct io_file_read_event* event);
    void process_fclose_result(struct io_file_close_event* event);

    void cleanup_user_request(struct user_request* req);
    void cleanup_after_write(struct io_client_write_event* event);
    void cleanup_after_write_vectored(struct io_client_write_event_vectored* event);

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