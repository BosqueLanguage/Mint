#pragma once

#include "common.h"
#include "alloc.h"
#include "fixedmsgs.h"
#include "filemgr.h"
#include "events.h"

#include <sys/stat.h>
#include <netinet/in.h>

#define RING_EVENT_TYPE_IO 0x0
#define RING_EVENT_TYPE_ACCEPT 0x1

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

    FileCacheManager file_cache_mgr;

    void write_user_direct(UserRequest* req, size_t size, const char* data);
    void write_user_direct_wheaders(UserRequest* req, size_t size, const char* data, const char* dkind);
    void write_user_direct_aio_wheaders(UserRequest* req, size_t size, const char* data, const char* dkind);
    void write_user_file_contents(UserRequest* req, size_t size, const char* data, bool should_release);
    void write_user_dynamic_response(UserRequest* req, size_t size, const char* data);

    void send_static_content(UserRequest* req, const char* str) {
        this->write_user_direct(req, strlen(str), str);
    }

    void send_immediate_fixed_content(UserRequest* req,  size_t size, const char* data, const char* dkind) {
        this->write_user_direct_wheaders(req, size, data, dkind);
    }

    void send_compute_content(UserRequest* req,  size_t size, const char* data, const char* dkind) {
        this->write_user_direct_aio_wheaders(req, size, data, dkind);
    }

    void send_cache_file_content(UserRequest* req, size_t size, const char* data) {
        this->write_user_file_contents(req, size, data, false);
    }

    void handle_error_code(UserRequest* req, RSErrorCode error_code);

    void process_user_connect(int listen_socket);
    void process_user_request(IOUserRequestEvent* event, size_t read_size);

    //TODO: process a user action request
    void process_http_file_access(IOUserRequestEvent* req, const char* file_path, bool memoize);

    void process_fstat_result(IOFileStatEvent* event);
    void process_fopen_result(IOFileOpenEvent* event, int file_descriptor);
    void process_fread_result(IOFileReadEvent* event);
    void process_fclose_result(IOFileCloseEvent* event);

    void process_job_request(IOUserRequestEvent* event, int64_t value);
    void process_job_complete(IOJobCompleteEvent* event);

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