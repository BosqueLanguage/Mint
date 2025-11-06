#include "server.h"

#define QUEUE_DEPTH 256

int get_line(const char *src, char *dest, size_t dest_sz) {
    for (size_t i = 0; i < dest_sz; i++)
    {
        dest[i] = src[i];
        if (src[i] == '\r' && src[i + 1] == '\n')
        {
            dest[i] = '\0';
            return 0;
        }
    }
    return 1;
}

void RSHookServer::write_user(struct user_request* req, size_t size, const char* data, bool should_release)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);

    struct io_client_write_event* evt = this->allocator.allocate<struct io_client_write_event>();
    evt->base.io_event_type = RING_EVENT_IO_CLIENT_WRITE;
    evt->base.req = req;
    evt->size = size;
    evt->msg_data = data;
    evt->should_release = should_release;

    io_uring_prep_write(sqe, req->client_socket, data, size, 0);
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

void RSHookServer::send_fstat(struct user_request* req, const char* file_path, bool memoize)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    struct io_file_stat_event* evt = this->allocator.allocate<struct io_file_stat_event>();
    evt->base.io_event_type = RING_EVENT_IO_FILE_STAT;
    evt->base.req = req;
    evt->file_path = file_path;
    evt->memoize = memoize;

    io_uring_prep_statx(sqe, AT_FDCWD, file_path, AT_STATX_SYNC_AS_STAT, STATX_ALL, &evt->stat_buf);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::send_fopen(struct io_file_stat_event* req)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    struct io_file_open_event* evt = this->allocator.allocate<struct io_file_open_event>();
    evt->base.io_event_type = RING_EVENT_IO_FILE_OPEN;
    evt->base.req = req->base.req;
    evt->file_path = req->file_path;
    evt->stat_buf = req->stat_buf;
    evt->file_fd = -1;
    evt->memoize = req->memoize;

    io_uring_prep_openat(sqe, AT_FDCWD, evt->file_path, O_RDONLY | O_NONBLOCK, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
}

void RSHookServer::send_read_file(struct io_file_open_event* req)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->ring);
    struct io_file_read_event* evt = this->allocator.allocate<struct io_file_read_event>();
    evt->base.io_event_type = RING_EVENT_IO_FILE_READ;
    evt->base.req = req->base.req;
    evt->file_path = req->file_path;
    evt->file_fd = req->file_fd;

    evt->size = req->stat_buf.stx_size;
    evt->file_data = (char*)this->allocator.allocatebytesp2(req->stat_buf.stx_size);

    io_uring_prep_read(sqe, req->file_fd, (void*)evt->file_data, evt->size, 0);
    io_uring_sqe_set_data(sqe, evt);

    this->submission_count++; //track number of submissions for batching
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
    //TODO: need to gracefully stop accepting new connections and wait for existing ones to finish then exit

    io_uring_queue_exit(&this->ring);

    this->file_cache_mgr.clear(this->allocator);
}

void RSHookServer::runloop()
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_multishot_accept(sqe, this->server_socket, nullptr, nullptr, 0);

    io_uring_sqe_set_data64(sqe, RING_EVENT_TYPE_ACCEPT);
    io_uring_submit(&this->ring);

    while(1) {

    }
}

void add_read_request(int client_socket)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct request* req = (struct request*)malloc(sizeof(struct request) + sizeof(struct iovec));
    req->iov[0].iov_base = malloc(READ_BYTE_SIZE);
    req->iov[0].iov_len = READ_BYTE_SIZE;
    req->event_type = EVENT_TYPE_READ;
    req->client_socket = client_socket;
    memset(req->iov[0].iov_base, 0, READ_BYTE_SIZE);

    io_uring_prep_readv(sqe, client_socket, &req->iov[0], 1, 0);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);
}

void add_write_request(struct request *req)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    req->event_type = EVENT_TYPE_WRITE;

    io_uring_prep_writev(sqe, req->client_socket, req->iov, req->iovec_count, 0);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);
}


std::optional<char*> copy_file_contents(const char* file_path, off_t file_size)
{
    //TODO: cache files here to avoid repeated reads

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        return std::nullopt;
    }

    char* buf = (char*)malloc(file_size);
    int ret = read(fd, buf, file_size);
    if (ret < file_size)
    {
        free(buf);
        return std::nullopt;
    }
    close(fd);

    return buf;
}

const char* get_filename_ext(const char* filename)
{
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return "";
    }
    else {
        return dot + 1;
    }
}

void send_headers(const char* path, off_t len, struct iovec *iov)
{
    size_t slen;
    char send_buffer[256];

    const char *str = "HTTP/1.0 200 OK\r\n";
    slen = strlen(str);
    iov[0].iov_base = malloc(slen);
    iov[0].iov_len = slen;
    memcpy(iov[0].iov_base, str, slen);

    slen = strlen(SERVER_STRING);
    iov[1].iov_base = malloc(slen);
    iov[1].iov_len = slen;
    memcpy(iov[1].iov_base, SERVER_STRING, slen);

    const char* file_ext = get_filename_ext(path);
    if (strcmp("jpg", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: image/jpeg\r\n");
    }
    else if (strcmp("jpeg", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: image/jpeg\r\n");
    }
    else if (strcmp("png", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: image/png\r\n");
    }
    else if (strcmp("gif", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: image/gif\r\n");
    }
    else if (strcmp("html", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: text/html\r\n");
    }
    else if (strcmp("js", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: application/javascript\r\n");
    }
    else if (strcmp("css", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: text/css\r\n");
    }
    else if (strcmp("txt", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: text/plain\r\n");
    }
    else if (strcmp("json", file_ext) == 0) {
        strcpy(send_buffer, "Content-Type: application/json\r\n");
    }
    else {
        strcpy(send_buffer, "Content-Type: application/octet-stream\r\n");
    }
    
    slen = strlen(send_buffer);
    iov[2].iov_base = malloc(slen);
    iov[2].iov_len = slen;
    memcpy(iov[2].iov_base, send_buffer, slen);

    /* Send the content-length header, which is the file size in this case. */
    sprintf(send_buffer, "content-length: %ld\r\n", len);
    slen = strlen(send_buffer);
    iov[3].iov_base = malloc(slen);
    iov[3].iov_len = slen;
    memcpy(iov[3].iov_base, send_buffer, slen);

    /*
     * When the browser sees a '\r\n' sequence in a line on its own,
     * it understands there are no more headers. Content may follow.
     * */
    strcpy(send_buffer, "\r\n");
    slen = strlen(send_buffer);
    iov[4].iov_base = malloc(slen);
    iov[4].iov_len = slen;
    memcpy(iov[4].iov_base, send_buffer, slen);
}

void handle_get_file_method(const char* path, int client_socket)
{
    if (static_rootdir_len + strlen(path) > PATH_MAX)
    {
        handle_http_404(client_socket);
        return;
    }

    strcpy(fullpath, static_rootdir);
    strcpy(fullpath + static_rootdir_len, path + (strlen(static_prefix) - 1)); //Skip the static prefix but leave the /

    char* npath = realpath(fullpath, normalizedpath);
    if(npath == nullptr || strncmp(npath, static_rootdir, static_rootdir_len) != 0) //Make sure we are not escaping the root dir
    {
        handle_http_404(client_socket);
        return;
    }

    struct stat path_stat;
    if (stat(npath, &path_stat) == -1)
    {
        handle_http_404(client_socket);
    }
    else
    {
        /* Check if this is a normal/regular file and not a directory or something else */
        if (!S_ISREG(path_stat.st_mode))
        {
            handle_http_404(client_socket);
            return;
        }

        std::optional<char*> data = copy_file_contents(npath, path_stat.st_size);
        if(!data.has_value())
        {
            handle_http_404(client_socket);
            return;
        }

        struct request* req = (struct request*)malloc(sizeof(struct request) + (sizeof(struct iovec) * 6));
        req->iovec_count = 6;
        req->client_socket = client_socket;
        send_headers(npath, path_stat.st_size, req->iov);
        
        req->iov[5].iov_base = data.value();
        req->iov[5].iov_len = path_stat.st_size;

        add_write_request(req);
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

void handle_client_request(struct request* req)
{
    if (get_line((const char*) req->iov[0].iov_base, http_request, sizeof(http_request))) {
        handle_http_400(req->client_socket);
    }
    else {
        handle_http_method(http_request, req->client_socket);
    }
}

int server_loop(int server_socket) {
    struct io_uring_cqe* cqe;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    add_accept_request(server_socket, &client_addr, &client_addr_len);

    printf("Server listening...\n");

    while (1)
    {
        int ret = io_uring_wait_cqe(&ring, &cqe);
        struct request* req = (struct request*)cqe->user_data;
        if (ret < 0) {
            //fatal error -- return to main for cleanup
            return ret; //the error code -- later we might be able to recover from some errors
        }

        if (cqe->res < 0) {
            //fatal error -- return to main for cleanup
            return cqe->res; //the error code -- later we might be able to recover from some errors
        }

        switch (req->event_type) {
        case EVENT_TYPE_ACCEPT:
            add_accept_request(server_socket, &client_addr, &client_addr_len);
            add_read_request(cqe->res);
            //since the accept request is statically pre-allocated and reused don't free it
            break;
        case EVENT_TYPE_READ:
            handle_client_request(req);
            free(req->iov[0].iov_base);
            free(req);
            break;
        case EVENT_TYPE_WRITE:
            for (int i = 0; i < req->iovec_count; i++) {
                free(req->iov[i].iov_base);
            }
            close(req->client_socket);
            free(req);
            break;
        }

        io_uring_cqe_seen(&ring, cqe);
    }
}
