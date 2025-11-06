#include "respond.h"

void add_write_request(struct request* req, struct io_uring* ring, size_t& submissions)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    req->event_type = RING_EVENT_TYPE::EVENT_TYPE_WRITE;

    io_uring_prep_writev(sqe, req->client_socket, req->iov, req->iovec_count, 0);
    io_uring_sqe_set_data(sqe, req);

    submissions++; //track number of submissions for batching
}

/**
 * For responding with static error messages
 **/
void send_static_content(const char *str, int client_socket, struct io_uring* ring, size_t& submissions) 
{
    struct request* req = (struct request*)malloc(sizeof(struct request) + sizeof(struct iovec));
    size_t slen = strlen(str);
    req->iovec_count = 1;
    req->client_socket = client_socket;
    req->iov[0].iov_base = malloc(slen);
    req->iov[0].iov_len = slen;
    memcpy(req->iov[0].iov_base, str, slen);
    add_write_request(req, ring, submissions);
}