#include "server.h"

#include "../application/apis.h"

#include "json.hpp"
typedef nlohmann::json json;

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

const char* get_header_content_type(const char* extstr)
{
    if (strcmp("jpg", extstr) == 0) {
        return "Content-Type: image/jpeg\r\n";
    }
    else if (strcmp("jpeg", extstr) == 0) {
        return "Content-Type: image/jpeg\r\n";
    }
    else if (strcmp("png", extstr) == 0) {
        return "Content-Type: image/png\r\n";
    }
    else if (strcmp("gif", extstr) == 0) {
        return "Content-Type: image/gif\r\n";
    }
    else if (strcmp("html", extstr) == 0) {
        return "Content-Type: text/html\r\n";
    }
    else if (strcmp("js", extstr) == 0) {
        return "Content-Type: application/javascript\r\n";
    }
    else if (strcmp("css", extstr) == 0) {
        return "Content-Type: text/css\r\n";
    }
    else if (strcmp("txt", extstr) == 0) {
        return "Content-Type: text/plain\r\n";
    }
    else if (strcmp("json", extstr) == 0) {
        return "Content-Type: application/json\r\n";
    }
    else {
        return "Content-Type: application/octet-stream\r\n";
    }
}

int build_dynamic_headers(size_t contents_size, char* send_buffer)
{
    return std::snprintf(send_buffer, HEADER_BUFFER_MAX, "HTTP/1.0 200 OK\r\n%sContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n", SERVER_STRING, contents_size);
}

int build_direct_user_headers(const char* path, size_t contents_size, char* send_buffer, const char* dkind)
{
    const char* ftype = get_header_content_type(path);
    return std::snprintf(send_buffer, HEADER_BUFFER_MAX, "HTTP/1.0 200 OK\r\n%s%sContent-Length: %ld\r\n\r\n", SERVER_STRING, ftype, contents_size);
}

int build_file_headers(const char* path, size_t contents_size, char* send_buffer)
{
    const char* ftype = get_header_content_type(get_filename_ext(path));
    return std::snprintf(send_buffer, HEADER_BUFFER_MAX, "HTTP/1.0 200 OK\r\n%s%sContent-Length: %ld\r\n\r\n", SERVER_STRING, ftype, contents_size);
}

void RSHookServer::write_user_direct(UserRequest* req, size_t size, const char* data)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOClientWriteEvent* evt = IOClientWriteEvent::create(this->allocator, req, size, data);

    io_uring_prep_write(sqe, req->client_socket, data, size, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::write_user_direct_wheaders(UserRequest* req, size_t size, const char* data, const char* dkind, bool should_release)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOClientWriteEventVectored* evt = IOClientWriteEventVectored::create(this->allocator, req);

    //Set the headers as the first iovec entry
    char* header = (char*)this->allocator.allocatebytesp2(HEADER_BUFFER_MAX);
    int header_len = build_direct_user_headers(req->route, size, header, dkind);
    evt->iov[0].iov_base = header;
    evt->iov[0].iov_len = header_len;
    evt->iov_sizes[0] = header_len;

    //Set the contents as the second iovec entry
    evt->iov[1].iov_base = (char*)data;
    evt->iov[1].iov_len = size;
    evt->iov_sizes[1] = should_release ? (int32_t)size : -1;

    io_uring_prep_writev(sqe, req->client_socket, evt->iov, 2, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::write_user_file_contents(UserRequest* req, size_t size, const char* data, bool should_release)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOClientWriteEventVectored* evt = IOClientWriteEventVectored::create(this->allocator, req);

    //Set the headers as the first iovec entry
    char* header = (char*)this->allocator.allocatebytesp2(HEADER_BUFFER_MAX);
    int header_len = build_file_headers(req->route, size, header);
    evt->iov[0].iov_base = header;
    evt->iov[0].iov_len = header_len;
    evt->iov_sizes[0] = header_len;

    //Set the contents as the second iovec entry
    evt->iov[1].iov_base = (char*)data;
    evt->iov[1].iov_len = size;
    evt->iov_sizes[1] = should_release ? (int32_t)size : -1;

    io_uring_prep_writev(sqe, req->client_socket, evt->iov, 2, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::write_user_dynamic_response(UserRequest* req, size_t size, const char* data)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOClientWriteEventVectored* evt = IOClientWriteEventVectored::create(this->allocator, req);

    //Set the headers as the first iovec entry
    char* header = (char*)this->allocator.allocatebytesp2(HEADER_BUFFER_MAX);
    int header_len = build_file_headers(req->route, size, header);
    evt->iov[0].iov_base = header;
    evt->iov[0].iov_len = header_len;
    evt->iov_sizes[0] = header_len;

    //Set the contents as the second iovec entry
    evt->iov[1].iov_base = (char*)data;
    evt->iov[1].iov_len = size;
    evt->iov_sizes[1] = (int32_t)size;

    io_uring_prep_writev(sqe, req->client_socket, evt->iov, 2, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::handle_error_code(UserRequest* req, RSErrorCode error_code)
{
    UserRequest* req_clone = req->clone(this->allocator);

    switch(error_code) {
    case RSErrorCode::MALFORMED_REQUEST:
        this->send_static_content(req_clone, MALFORMED_REQUEST_MSG);
        break;
    case RSErrorCode::UNSUPPORTED_VERB:
        this->send_static_content(req_clone, UNSUPPORTED_VERB_MSG);
        break;
    case RSErrorCode::ROUTE_NOT_FOUND:
        this->send_static_content(req_clone, CONTENT_404_MSG);
        break;
    default:
        this->send_static_content(req_clone, INTERNAL_SERVER_ERROR_MSG);
        break;
    }
}

void RSHookServer::process_user_connect(int listen_socket)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    UserRequest* req = UserRequest::create(this->allocator, listen_socket, nullptr, 0, nullptr);
    IOUserRequestEvent* evt = IOUserRequestEvent::create(this->allocator, req, (char*)this->allocator.allocatebytesp2(HTTP_MAX_REQUEST_BUFFER_SIZE));

    io_uring_prep_read(sqe, listen_socket, evt->http_request_data, HTTP_MAX_REQUEST_BUFFER_SIZE, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

std::pair<const char*, const char*> extractHTTPVerb(const char* http_request_data)
{
    const char* vstart = http_request_data;
    const char* vend = strstr(http_request_data, " ");

    return std::make_pair(vstart, vend);
}

std::pair<const char*, const char*> extractHTTPPath(const char* http_request_data)
{
    const char* pstart = strstr(http_request_data, " /") + 1;
    const char* pend = strstr(pstart, " ");

    return std::make_pair(pstart, pend);
}

size_t extractHTTPContentLength(const char* http_request_data)
{
    const char* clstart = strstr(http_request_data, "Content-Length: ") + strlen("Content-Length: ");

    return strtol(clstart, nullptr, 10);
}

std::pair<const char*, const char*> extractHTTPData(const char* http_request_data, size_t content_length)
{
    const char* dstart = strstr(http_request_data, "\r\n\r\n") + 4;
    const char* dend = dstart + content_length;

    return std::make_pair(dstart, dend);
}

bool pathMatchsRoute(const std::pair<const char*, const char*>& path, const char* match)
{
    return (strncmp(path.first, match, path.second - path.first) == 0);
}

void RSHookServer::process_user_request(IOUserRequestEvent* event)
{
    std::pair<const char*, const char*> verb = extractHTTPVerb(event->http_request_data);
    std::pair<const char*, const char*> path = extractHTTPPath(event->http_request_data);

    event->req->route = this->allocator.strcopyp2(path.first, path.second - path.first);
    event->req->argdata = nullptr;

    if (strncmp(verb.first, "get", verb.second - verb.first) == 0)
    {
        /** TODO: wan't a known URI for hyper-agent discovery **/

        if(pathMatchsRoute(path, "/sample.json")) /*Route type #1 file service -- static (permanent) files or basic caching as resources*/
        {
            std::pair<const char*, size_t> cached_data = this->file_cache_mgr.tryGet(event->req->route);
            if(cached_data.first != nullptr) {
                CONSOLE_LOG_PRINT("Cache hit for %s\n", event->req->route);
                this->send_cache_file_content(event->req->clone(this->allocator), cached_data.second, cached_data.first);
            }
            else {
                //TODO: better base lookup
                const char* fpath = (const char*)this->allocator.allocatebytesp2(s_strlen("/home/mark/Code/RSHook/build/static") + s_strlen("/sample.json") + 1);
                sprintf((char*)fpath, "%s%s", "/home/mark/Code/RSHook/build/static", "/sample.json");

                this->process_http_file_access(event, fpath, true);
            }
        }
        else if(pathMatchsRoute(path, "/hello")) /* Route type #2 immediate response of fixed (small) values */
        {
            const char* response = "{\"message\": \"Hello, world!\"}";
            this->send_immediate_fixed_content(event->req->clone(this->allocator), s_strlen(response), response, "json");
        }
        else if(pathMatchsRoute(path, "/helloname")) /* Route type #3 compute response based on input data inline with request */
        {
            size_t datalen = extractHTTPContentLength(event->http_request_data);
            std::pair<const char*, const char*> data = extractHTTPData(event->http_request_data, datalen);

            auto jpayload = json::parse(data.first, data.second, nullptr, false, false);
            std::string name = jpayload["name"].get<std::string>();
            
            char* sendbuff = (char*)this->allocator.allocatebytesp2(256);
            size_t datasize = std::snprintf(sendbuff, 256, "{\"message\":\"Hello, %s!\"}", name.c_str());

            this->write_user_dynamic_response(event->req->clone(this->allocator), datasize, sendbuff);
        }
        else if(pathMatchsRoute(path, "/fib")) /* Route type #4 compute response based on input data but run on thread-pool for non-blocking */
        {
            size_t datalen = extractHTTPContentLength(event->http_request_data);
            std::pair<const char*, const char*> data = extractHTTPData(event->http_request_data, datalen);

            auto jpayload = json::parse(data.first, data.second, nullptr, false, false);
            int64_t value = jpayload["value"].get<int64_t>();

            //Compute Fibonacci (inefficiently on purpose) -- run in separate thread with callback/futex for iouring 
            xxxx;
        }
        else
        {
            handle_error_code(event->req, RSErrorCode::ROUTE_NOT_FOUND);
        }
    }
    else
    {
        /* posts here and more */

        handle_error_code(event->req, RSErrorCode::UNSUPPORTED_VERB);
    }
}

void RSHookServer::process_http_file_access(IOUserRequestEvent* req, const char* file_path, bool memoize)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOFileStatEvent* evt = IOFileStatEvent::create(this->allocator, req, file_path, memoize);

    io_uring_prep_statx(sqe, AT_FDCWD, file_path, AT_STATX_SYNC_AS_STAT, STATX_ALL, &evt->stat_buf);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fstat_result(IOFileStatEvent* event)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOFileOpenEvent* evt = IOFileOpenEvent::create(this->allocator, event, event->stat_buf, event->memoize);

    io_uring_prep_openat(sqe, AT_FDCWD, evt->file_path, O_RDONLY | O_NONBLOCK, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fopen_result(IOFileOpenEvent* event, int file_descriptor)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOFileReadEvent* evt = IOFileReadEvent::create(this->allocator, event, file_descriptor, event->stat_buf.stx_size, (char*)this->allocator.allocatebytesp2(event->stat_buf.stx_size + 1), event->memoize);

    io_uring_prep_read(sqe, file_descriptor, evt->file_data, evt->size, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fread_result(IOFileReadEvent* event)
{
    //Add null terminator for uniformity on the read file (note we made sure there was an extra byte allocated)
    event->file_data[event->size] = '\0';

    ////
    //Setup the response to the user now that we have the file data and handle any caching
    //Right now everything is cached permanently 
    const char* cdata = this->file_cache_mgr.put(event->req->route, s_strlen(event->req->route), this->allocator.strcopyp2(event->file_data), event->size);
    this->send_cache_file_content(event->req->clone(this->allocator), event->size, cdata);

    ////
    //Setup the close event to clean up the file descriptor
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    IOFileCloseEvent* evt = IOFileCloseEvent::create(this->allocator, event);

    io_uring_prep_close(sqe, event->file_fd);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::process_fclose_result(IOFileCloseEvent* event)
{
    //no continuation as of now -- just stop processing
}

void RSHookServer::process_job_request(IOUserRequestEvent* event)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);

    io_uring_prep_futex_wait(sqe, nullptr, FUTEX_WAIT, 0, nullptr, 0, 0);
    xxxx;
}

void RSHookServer::process_job_complete(IOJobCompleteEvent* event)
{
    char* data = event->result;
    event->result = nullptr;
    
    this->send_compute_content(event->req->clone(this->allocator), event->size, data, "json");
}

RSHookServer::RSHookServer() : port(0), server_socket(-1), submission_count(0)
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

        while(1) {
            if((cqe->user_data & RING_EVENT_TYPE_ACCEPT) == RING_EVENT_TYPE_ACCEPT) {
                this->process_user_connect(cqe->res);
            }
            else {
                IOEvent* event = (IOEvent*)cqe->user_data;

                switch (event->io_event_type) {
                    case RING_EVENT_IO_CLIENT_READ: {
                        CONSOLE_LOG_PRINT("Handling client read event -- %x\n", event->req->client_socket);

                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error reading from client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                            handle_error_code(event->req, RSErrorCode::MALFORMED_REQUEST);
                            break;
                        }

                        this->process_user_request((IOUserRequestEvent*)event);
                        break;
                    }
                    case RING_EVENT_IO_CLIENT_WRITE: {
                        CONSOLE_LOG_PRINT("Handling file write event -- %x %s\n", event->req->client_socket, event->req->route);
                        
                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error writing to client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                        }

                        //either way the user has been responded to just cleanup
                        close(event->req->client_socket);
                        break;
                    }
                    case RING_EVENT_IO_CLIENT_WRITE_VECTORED: {
                        CONSOLE_LOG_PRINT("Handling vectored write event -- %x %s\n", event->req->client_socket, event->req->route);
                        
                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error writing to client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                        }
                        
                        //either way the user has been responded to just cleanup
                        close(event->req->client_socket);
                        break;
                    }
                    case RING_EVENT_IO_FILE_STAT: {
                        CONSOLE_LOG_PRINT("Handling file stat event -- %x %s\n", event->req->client_socket, event->req->route);
                        
                        IOFileStatEvent* eevt = (IOFileStatEvent*)event;
                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error processing file stat from client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                            handle_error_code(eevt->req, RSErrorCode::INTERNAL_SERVER_ERROR);
                            break;
                        }
                        this->process_fstat_result(eevt);
                        break;
                    }
                    case RING_EVENT_IO_FILE_OPEN: {
                        CONSOLE_LOG_PRINT("Handling file open event -- %x %s\n", event->req->client_socket, event->req->route);
                        
                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error opening file for client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                            handle_error_code(event->req, RSErrorCode::INTERNAL_SERVER_ERROR);
                            break;
                        }
                        
                        this->process_fopen_result((IOFileOpenEvent*)event, cqe->res);
                        break;
                    }
                    case RING_EVENT_IO_FILE_READ: {
                        CONSOLE_LOG_PRINT("Handling file read event -- %x %s\n", event->req->client_socket, event->req->route);
                        
                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error reading file for client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                            handle_error_code(event->req, RSErrorCode::INTERNAL_SERVER_ERROR);
                            break;
                        }

                        this->process_fread_result((IOFileReadEvent*)event);
                        break;
                    }
                    case RING_EVENT_IO_FILE_CLOSE: {
                        CONSOLE_LOG_PRINT("Handling file close event -- %x %s\n", event->req->client_socket, event->req->route);
                        
                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error closing file for client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                            handle_error_code(event->req, RSErrorCode::INTERNAL_SERVER_ERROR);
                            break;
                        }
                        
                        this->process_fclose_result((IOFileCloseEvent*)event);
                        break;
                    }
                    case RING_EVENT_JOB_COMPLETE: {
                        CONSOLE_LOG_PRINT("Handling job request completion event -- %x %s\n", event->req->client_socket, event->req->route);
                        
                        if (cqe->res < 0) {
                            CONSOLE_LOG_PRINT("Error processing job request for client socket %d: %s\n", event->req->client_socket, strerror(-cqe->res));
                            handle_error_code(event->req, RSErrorCode::INTERNAL_SERVER_ERROR);
                            break;
                        }
                        
                        this->process_job_complete((IOJobCompleteEvent*)event);
                        break;
                    }
                    default: {
                        CONSOLE_LOG_PRINT("Unexpected req type %d\n", event->io_event_type);
                        
                        handle_error_code(event->req, RSErrorCode::INTERNAL_SERVER_ERROR);
                        break;
                    }
                }

                event->release(this->allocator);
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
        }

        if (this->submission_count > 0) {
            io_uring_submit(&this->ring);
            this->submission_count = 0;
        }
    }
}
