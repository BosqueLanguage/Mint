#include "server.h"

#define HEADER_BUFFER_MAX 512
#define QUEUE_DEPTH 256

#if ENABLE_CONSOLE_STATUS
#define CONSOLE_STATUS_PRINT(...) printf(__VA_ARGS__)
#else
#define CONSOLE_STATUS_PRINT(...)
#endif

#if ENABLE_CONSOLE_LOGGING
#define CONSOLE_LOG_PRINT(...) printf(__VA_ARGS__)
#else
#define CONSOLE_LOG_PRINT(...)
#endif

const char* get_header_content_type(const char* path)
{
    const char* file_ext = get_filename_ext(path);
    if (strcmp("jpg", file_ext) == 0) {
        return "Content-Type: image/jpeg\r\n";
    }
    else if (strcmp("jpeg", file_ext) == 0) {
        return "Content-Type: image/jpeg\r\n";
    }
    else if (strcmp("png", file_ext) == 0) {
        return "Content-Type: image/png\r\n";
    }
    else if (strcmp("gif", file_ext) == 0) {
        return "Content-Type: image/gif\r\n";
    }
    else if (strcmp("html", file_ext) == 0) {
        return "Content-Type: text/html\r\n";
    }
    else if (strcmp("js", file_ext) == 0) {
        return "Content-Type: application/javascript\r\n";
    }
    else if (strcmp("css", file_ext) == 0) {
        return "Content-Type: text/css\r\n";
    }
    else if (strcmp("txt", file_ext) == 0) {
        return "Content-Type: text/plain\r\n";
    }
    else if (strcmp("json", file_ext) == 0) {
        return "Content-Type: application/json\r\n";
    }
    else {
        return "Content-Type: application/octet-stream\r\n";
    }
}

int build_dynamic_headers(size_t contents_size, char* send_buffer)
{
    return std::snprintf(send_buffer, HEADER_BUFFER_MAX, "HTTP/1.0 200 OK\r\n%sContent-Type: application/json\r\ncontent-length: %ld\r\n\r\n", SERVER_STRING, contents_size);
}

int build_file_headers(const char* path, size_t contents_size, char* send_buffer)
{
    const char* ftype = get_header_content_type(path);
    return std::snprintf(send_buffer, HEADER_BUFFER_MAX, "HTTP/1.0 200 OK\r\n%sContent-Type: %s\r\ncontent-length: %ld\r\n\r\n", SERVER_STRING, ftype, contents_size);
}

void RSHookServer::write_user_direct(struct user_request* req, size_t size, const char* data)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);

    struct io_client_write_event* evt = this->allocator.allocate<struct io_client_write_event>();
    evt->base.io_event_type = RING_EVENT_IO_CLIENT_WRITE;
    evt->base.req = req;
    evt->size = size;
    evt->msg_data = data;

    io_uring_prep_write(sqe, req->client_socket, data, size, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::write_user_file_contents(struct user_request* req, size_t size, const char* data, bool should_release)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);

    struct io_client_write_event_vectored* evt = this->allocator.allocate<struct io_client_write_event_vectored>();
    evt->base.io_event_type = RING_EVENT_IO_CLIENT_WRITE_VECTORED;
    evt->base.req = req;

    //Set the headers as the first iovec entry
    void* header = (void*)this->allocator.allocatebytesp2(HEADER_BUFFER_MAX);
    int header_len = build_file_headers(req->route, size, (char*)header);
    evt->iov[0].iov_base = header;
    evt->iov[0].iov_len = header_len;
    evt->iov_sizes[0] = header_len;

    //Set the contents as the second iovec entry
    evt->iov[1].iov_base = (void*)data;
    evt->iov[1].iov_len = size;
    evt->iov_sizes[1] = should_release ? (int32_t)size : -1;

    io_uring_prep_writev(sqe, req->client_socket, evt->iov, 2, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::write_user_dynamic_response(struct user_request* req, size_t size, const char* data)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);

    struct io_client_write_event_vectored* evt = this->allocator.allocate<struct io_client_write_event_vectored>();
    evt->base.io_event_type = RING_EVENT_IO_CLIENT_WRITE_VECTORED;
    evt->base.req = req;

    //Set the headers as the first iovec entry
    void* header = (void*)this->allocator.allocatebytesp2(HEADER_BUFFER_MAX);
    int header_len = build_file_headers(req->route, size, (char*)header);
    evt->iov[0].iov_base = header;
    evt->iov[0].iov_len = header_len;
    evt->iov_sizes[0] = header_len;

    //Set the contents as the second iovec entry
    evt->iov[1].iov_base = (void*)data;
    evt->iov[1].iov_len = size;
    evt->iov_sizes[1] = (int32_t)size;

    io_uring_prep_writev(sqe, req->client_socket, evt->iov, 2, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::handle_error_code(struct user_request* req, RSErrorCode error_code)
{
    switch(error_code) {
    case RSErrorCode::MALFORMED_REQUEST:
        this->send_static_content(req, MALFORMED_REQUEST_MSG);
        break;
    case RSErrorCode::UNSUPPORTED_VERB:
        this->send_static_content(req, UNSUPPORTED_VERB_MSG);
        break;
    case RSErrorCode::ROUTE_NOT_FOUND:
        this->send_static_content(req, CONTENT_404_MSG);
        break;
    default:
        this->send_static_content(req, INTERNAL_SERVER_ERROR_MSG);
        break;
    }
}

void RSHookServer::process_user_connect(int listen_socket)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);

    struct io_user_request_event* evt = this->allocator.allocate<struct io_user_request_event>();
    evt->base.io_event_type = RING_EVENT_IO_CLIENT_READ;
    evt->base.req = nullptr; //will be set later when we parse the request
    evt->http_request_data = (char*)this->allocator.allocatebytesp2(HTTP_MAX_REQUEST_BUFFER_SIZE);

    io_uring_prep_read(sqe, listen_socket, evt->http_request_data, HTTP_MAX_REQUEST_BUFFER_SIZE, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_http_file_access(struct io_event* event, const char* file_path, bool memoize)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    struct io_file_stat_event* evt = this->allocator.allocate<struct io_file_stat_event>();
    evt->base.io_event_type = RING_EVENT_IO_FILE_STAT;
    evt->base.req = event->req;
    evt->file_path = file_path;
    evt->memoize = memoize;

    this->allocator.freep2<struct io_event>(event);

    io_uring_prep_statx(sqe, AT_FDCWD, file_path, AT_STATX_SYNC_AS_STAT, STATX_ALL, &evt->stat_buf);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fstat_result(struct io_file_stat_event* event)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    struct io_file_open_event* evt = this->allocator.allocate<struct io_file_open_event>();
    evt->base.io_event_type = RING_EVENT_IO_FILE_OPEN;
    evt->base.req = event->base.req;
    evt->file_path = event->file_path;
    evt->stat_buf = event->stat_buf;
    evt->file_fd = -1;
    evt->memoize = event->memoize;

    this->allocator.freep2<struct io_file_stat_event>(event);

    io_uring_prep_openat(sqe, AT_FDCWD, evt->file_path, O_RDONLY | O_NONBLOCK, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fopen_result(struct io_file_open_event* event)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    struct io_file_read_event* evt = this->allocator.allocate<struct io_file_read_event>();
    evt->base.io_event_type = RING_EVENT_IO_FILE_READ;
    evt->base.req = event->base.req;
    evt->file_path = event->file_path;
    evt->file_fd = event->file_fd;

    evt->size = event->stat_buf.stx_size;
    evt->file_data = (char*)this->allocator.allocatebytesp2(event->stat_buf.stx_size);

    this->allocator.freep2<struct io_file_open_event>(event);

    io_uring_prep_read(sqe, event->file_fd, (void*)evt->file_data, evt->size, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fread_result(struct io_file_read_event* event)
{
    ////
    //Setup the response to the user now that we have the file data and handle any caching
    //Right now everything is cached permanently 
    this->file_cache_mgr.put(event->file_path, strlen(event->file_path), event->file_data, event->size);
    this->send_cache_file_content(event->base.req, event->file_path, event->size, event->file_data);

    ////
    //Setup the close event to clean up the file descriptor
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    struct io_file_close_event* evt = this->allocator.allocate<struct io_file_close_event>();
    evt->base.io_event_type = RING_EVENT_IO_FILE_CLOSE;
    evt->base.req = event->base.req;
    evt->file_path = event->file_path;

    this->allocator.freep2<struct io_file_read_event>(event);

    io_uring_prep_close(sqe, event->file_fd);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fclose_result(struct io_file_close_event* event)
{
    //no continuation as of now -- just clean up
    this->allocator.freebytesp2((uint8_t*)event->file_path, strlen(event->file_path) + 1);
    this->allocator.freep2<struct io_file_close_event>(event);
}

void RSHookServer::cleanup_user_request(struct user_request* req)
{
    this->allocator.freebytesp2((uint8_t*)req->route, strlen(req->route) + 1);
    this->allocator.freebytesp2((uint8_t*)req->argdata, req->size);
    this->allocator.freep2<struct user_request>(req);
}

void RSHookServer::cleanup_after_write(struct io_client_write_event* event)
{
    this->cleanup_user_request(event->base.req);

    this->allocator.freebytesp2((uint8_t*)event->msg_data, event->size);
    this->allocator.freep2<struct io_client_write_event>(event);
}

void RSHookServer::cleanup_after_write_vectored(struct io_client_write_event_vectored* event)
{
    this->cleanup_user_request(event->base.req);

    for(int i = 0; i < 2; i++) {
        if(event->iov_sizes[i] != -1) {
            this->allocator.freebytesp2((uint8_t*)event->iov[i].iov_base, event->iov_sizes[i]);
        }
    }
    this->allocator.freep2<struct io_client_write_event_vectored>(event);
}

RSHookServer::RSHookServer() : port(0), server_socket(-1), submission_count(0), ring()
{
    ;
}

RSHookServer::~RSHookServer()
{
    ;
}

void RSHookServer::startup(int port, int server_socket)
{
    this->port = port;
    this->server_socket = server_socket;

    io_uring_queue_init(QUEUE_DEPTH, &this->ring, 0);
}

void RSHookServer::shutdown()
{
    CONSOLE_STATUS_PRINT("Shutting down server...\n");
    //TODO: need to gracefully stop accepting new connections and wait for existing ones to finish then exit

    io_uring_queue_exit(&this->ring);

    this->file_cache_mgr.clear(this->allocator);

    CONSOLE_STATUS_PRINT("Server shutdown complete.\n");
}

void RSHookServer::runloop()
{
    CONSOLE_STATUS_PRINT("Server starting...\n");

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_multishot_accept(sqe, this->server_socket, nullptr, nullptr, 0);

    io_uring_sqe_set_data64(sqe, RING_EVENT_TYPE_ACCEPT);
    io_uring_submit(&this->ring);

    CONSOLE_STATUS_PRINT("Server listening...\n");

    while (1) {
        assert(this->submission_count == 0);
        
        struct io_uring_cqe* cqe;
        int ret = io_uring_wait_cqe(&this->ring, &cqe);
        if (ret < 0) {
            CONSOLE_LOG_PRINT("Fatal error waiting for CQE: %s\n", strerror(-ret));
            assert(false);
        }

        if (cqe->res < 0) {
            CONSOLE_LOG_PRINT("Fatal error in CQE: %s\n", strerror(-cqe->res));
            assert(false);
        }

        while(1) {
            if((cqe->user_data & RING_EVENT_TYPE_ACCEPT) == RING_EVENT_TYPE_ACCEPT) {
                this->process_user_connect(cqe->res);
            }
            else {
                struct io_event* event = (struct io_event*)cqe->user_data;

                switch (event->io_event_type) {
                    case RING_EVENT_IO_CLIENT_READ: {
                        CONSOLE_LOG_PRINT("Handling client read event -- %x\n", event->req->client_socket);
                        this->process_user_request((struct io_user_request_event*)event);
                        break;
                    }
                    case RING_EVENT_IO_CLIENT_WRITE: {
                        CONSOLE_LOG_PRINT("Handling file write event -- %x %s\n", event->req->client_socket, event->req->route);
                        this->cleanup_after_write((struct io_client_write_event*)event);
                        break;
                    }
                    case RING_EVENT_IO_CLIENT_WRITE_VECTORED: {
                        CONSOLE_LOG_PRINT("Handling vectored write event -- %x %s\n", event->req->client_socket, event->req->route);
                        this->cleanup_after_write_vectored((struct io_client_write_event_vectored*)event);
                        break;
                    }
                    case RING_EVENT_IO_FILE_STAT: {
                        CONSOLE_LOG_PRINT("Handling file stat event -- %x %s\n", event->req->client_socket, event->req->route);
                        this->process_fstat_result((struct io_file_stat_event*)event);
                        break;
                    }
                    case RING_EVENT_IO_FILE_OPEN: {
                        CONSOLE_LOG_PRINT("Handling file open event -- %x %s\n", event->req->client_socket, event->req->route);
                        this->process_fopen_result((struct io_file_open_event*)event);
                        break;
                    }
                    case RING_EVENT_IO_FILE_READ: {
                        CONSOLE_LOG_PRINT("Handling file read event -- %x %s\n", event->req->client_socket, event->req->route);
                        this->process_fread_result((struct io_file_read_event*)event);
                        break;
                    }
                    case RING_EVENT_IO_FILE_CLOSE: {
                        CONSOLE_LOG_PRINT("Handling file close event -- %x %s\n", event->req->client_socket, event->req->route);
                        this->process_fclose_result((struct io_file_close_event*)event);
                        break;
                    }
                    default: {
                        CONSOLE_LOG_PRINT("Unexpected req type %d\n", event->io_event_type);
                        break;
                    }
                }
            }

            io_uring_cqe_seen(&this->ring, cqe);

            if (io_uring_sq_space_left(&this->ring) < 16) {
                break;     // the submission queue is full
            }

            ret = io_uring_peek_cqe(&this->ring, &cqe);
            if (ret == -EAGAIN) {
                break;     // no remaining work in completion queue
            }

            if (ret < 0) {
               CONSOLE_LOG_PRINT("Fatal error waiting for CQE: %s\n", strerror(-ret));
                assert(false);
            }

            if (cqe->res < 0) {
                CONSOLE_LOG_PRINT("Fatal error in CQE: %s\n", strerror(-cqe->res));
                assert(false);
            }
        }

        if (this->submission_count > 0) {
            io_uring_submit(&this->ring);
            this->submission_count = 0;
        }
    }
}

void handle_http_method(char* method_buffer, int client_socket) {
    char* saveptr = nullptr;

    const char* method = strtok_r(method_buffer, " ", &saveptr);
    const char* path = strtok_r(NULL, " ", &saveptr);

    string_to_lower((char*)method);
    if (strcmp(method, "get") == 0)
    {
        if(strncmp(path, static_prefix, strlen(static_prefix)) == 0)
        {
            handle_get_file_method(path, client_socket);
        }
        else
        {
            //TODO: we plan to add dynamic handling here later
            handle_unimplemented_method(client_socket);
        }
    }
    else
    {
        handle_unimplemented_method(client_socket);
    }
}

