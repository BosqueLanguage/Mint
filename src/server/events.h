#pragma once

#include "common.h"
#include "alloc.h"

#define RING_EVENT_IO_FILE_STAT 0x1
#define RING_EVENT_IO_FILE_OPEN 0x2
#define RING_EVENT_IO_FILE_READ 0x3
#define RING_EVENT_IO_FILE_CLOSE 0x4

#define RING_EVENT_IO_CLIENT_READ 0x10
#define RING_EVENT_IO_CLIENT_WRITE 0x20
#define RING_EVENT_IO_CLIENT_WRITE_VECTORED 0x30

#define RING_EVENT_JOB_COMPLETE 0x100

/**
 * Data structure representing the input to a route handler as extracted from the HTTP request
 **/
class UserRequest
{
public:
    const int32_t client_socket;
    const char* route;
    
    const size_t size;
    const char* argdata;

    UserRequest(int32_t client_socket, const char* route, size_t size, const char* argdata): client_socket(client_socket), route(route), size(size), argdata(argdata) { ; }
    ~UserRequest() = default;

    static UserRequest* create(int32_t client_socket, const char* route, size_t size, const char* argdata) 
    {
        return new (s_allocator.allocate<UserRequest>()) UserRequest(client_socket, route, size, argdata);
    }

    UserRequest* clone() 
    {
        char* route_copy = s_allocator.strcopyp2(this->route);
        char* argdata_copy = s_allocator.strcopyp2(this->argdata);

        return this->create(this->client_socket, route_copy, this->size, argdata_copy);
    }

    void release() 
    {
        s_allocator.freebytesp2((uint8_t*)this->route, s_strlen(this->route) + 1);
        s_allocator.freebytesp2((uint8_t*)this->argdata, this->size);

        s_allocator.freep2<UserRequest>(this);
    }
};

class IOEvent
{
public:
    int32_t io_event_type;
    UserRequest* req;

    IOEvent(int32_t io_event_type, UserRequest* req): io_event_type(io_event_type), req(req) { ; }
    virtual ~IOEvent() = default;

    virtual void release() = 0;
};

class IOUserRequestEvent : public IOEvent
{
public:
    char* http_request_data;

    IOUserRequestEvent(UserRequest* req, char* http_request_data): IOEvent(RING_EVENT_IO_CLIENT_READ, req), http_request_data(http_request_data) { ; }
    virtual ~IOUserRequestEvent() = default;

    static IOUserRequestEvent* create(UserRequest* req, char* http_request_data)
    {
        return new (s_allocator.allocate<IOUserRequestEvent>()) IOUserRequestEvent(req, http_request_data);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }

        s_allocator.freebytesp2((uint8_t*)this->http_request_data, HTTP_MAX_REQUEST_BUFFER_SIZE);
        s_allocator.freep2<IOUserRequestEvent>(this);
    }
};

class IOFileStatEvent : public IOEvent
{
public:
    const char* file_path;
    struct statx stat_buf;

    const bool memoize;

    IOFileStatEvent(UserRequest* req, const char* file_path, bool memoize): IOEvent(RING_EVENT_IO_FILE_STAT, req), file_path(file_path), stat_buf(), memoize(memoize) { ; }
    virtual ~IOFileStatEvent() = default;

    static IOFileStatEvent* create(IOUserRequestEvent* ure, const char* file_path, bool memoize)
    {
        auto req = ure->req;
        ure->req = nullptr; //transfer ownership

        return new (s_allocator.allocate<IOFileStatEvent>()) IOFileStatEvent(req, file_path, memoize);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }
    
        s_allocator.freebytesp2((uint8_t*)this->file_path, s_strlen(this->file_path) + 1);
        s_allocator.freep2<IOFileStatEvent>(this);
    }
};

class IOFileOpenEvent : public IOEvent
{
public:
    const char* file_path;
    struct statx stat_buf;

    const bool memoize;

    IOFileOpenEvent(UserRequest* req, const char* file_path, struct statx stat_buf, bool memoize): IOEvent(RING_EVENT_IO_FILE_OPEN, req), file_path(file_path), stat_buf(stat_buf), memoize(memoize) { ; }
    virtual ~IOFileOpenEvent() = default;

    static IOFileOpenEvent* create(IOFileStatEvent* fse, struct statx stat_buf, bool memoize)
    {
        auto req = fse->req;
        auto fpath = fse->file_path;

        fse->req = nullptr; //transfer ownership
        fse->file_path = nullptr;

        return new (s_allocator.allocate<IOFileOpenEvent>()) IOFileOpenEvent(req, fpath, stat_buf, memoize);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }
        
        s_allocator.freebytesp2((uint8_t*)this->file_path, s_strlen(this->file_path) + 1);
        s_allocator.freep2<IOFileOpenEvent>(this);
    }
};

class IOFileReadEvent : public IOEvent
{
public:
    const char* file_path;
    int32_t file_fd;

    size_t size;
    char* file_data;

    bool memoize;

    IOFileReadEvent(UserRequest* req, const char* file_path, int32_t file_fd, size_t size, char* file_data, bool memoize): IOEvent(RING_EVENT_IO_FILE_READ, req), file_path(file_path), file_fd(file_fd), size(size), file_data(file_data), memoize(memoize) { ; }
    virtual ~IOFileReadEvent() = default;

    static IOFileReadEvent* create(IOFileOpenEvent* foe, int file_descriptor, size_t size, char* file_data, bool memoize)
    {
        auto req = foe->req;
        auto fpath = foe->file_path;
        auto ffd = file_descriptor;

        foe->req = nullptr; //transfer ownership
        foe->file_path = nullptr;

        return new (s_allocator.allocate<IOFileReadEvent>()) IOFileReadEvent(req, fpath, ffd, size, file_data, memoize);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }

        s_allocator.freebytesp2((uint8_t*)this->file_path, s_strlen(this->file_path) + 1);
        s_allocator.freebytesp2((uint8_t*)this->file_data, this->size + 1); //has a null terminator
        s_allocator.freep2<IOFileReadEvent>(this);
    }
};

class IOFileCloseEvent : public IOEvent
{
public:
    const char* file_path;

    IOFileCloseEvent(UserRequest* req, const char* file_path): IOEvent(RING_EVENT_IO_FILE_CLOSE, req), file_path(file_path) { ; }
    virtual ~IOFileCloseEvent() = default;

    static IOFileCloseEvent* create(IOFileReadEvent* fre)
    {
        auto req = fre->req;
        auto fpath = fre->file_path;

        fre->req = nullptr; //transfer ownership
        fre->file_path = nullptr;

        return new (s_allocator.allocate<IOFileCloseEvent>()) IOFileCloseEvent(req, fpath);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }

        s_allocator.freebytesp2((uint8_t*)this->file_path, s_strlen(this->file_path) + 1);
        s_allocator.freep2<IOFileCloseEvent>(this);
    }
};

class IOClientWriteEvent : public IOEvent
{
public:
    size_t size;
    const char* msg_data;

    IOClientWriteEvent(UserRequest* req, size_t size, const char* msg_data): IOEvent(RING_EVENT_IO_CLIENT_WRITE, req), size(size), msg_data(msg_data) { ; }
    virtual ~IOClientWriteEvent() = default;

    static IOClientWriteEvent* create(UserRequest* req, size_t size, const char* msg_data)
    {
        return new (s_allocator.allocate<IOClientWriteEvent>()) IOClientWriteEvent(req, size, msg_data);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }
        
        //msg data in this case is always owned by others
        s_allocator.freep2<IOClientWriteEvent>(this);
    }
};

enum class IOClientWriteEventVectoredReleaseFlag
{
    None,
    Std,
    AIO
};

class IOClientWriteEventVectored : public IOEvent
{
public:
    std::pair<IOClientWriteEventVectoredReleaseFlag, int32_t> iov_release[2];
    struct iovec iov[2];

    IOClientWriteEventVectored(UserRequest* req): IOEvent(RING_EVENT_IO_CLIENT_WRITE_VECTORED, req) { ; }
    virtual ~IOClientWriteEventVectored() = default;

    static IOClientWriteEventVectored* create(UserRequest* req)
    {
        return new (s_allocator.allocate<IOClientWriteEventVectored>()) IOClientWriteEventVectored(req);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }

        for (int i = 0; i < 2; i++) {
            if (this->iov_release[i].first == IOClientWriteEventVectoredReleaseFlag::None) {
                ;
            }
            else {
                if(this->iov_release[i].first == IOClientWriteEventVectoredReleaseFlag::Std) {
                    s_allocator.freebytesp2((uint8_t*)this->iov[i].iov_base, this->iov_release[i].second);
                }
                else if(this->iov_release[i].first == IOClientWriteEventVectoredReleaseFlag::AIO) {
                    s_aio_allocator.freeAIOBuffer((uint8_t*)this->iov[i].iov_base);
                }
            }
        }
        s_allocator.freep2<IOClientWriteEventVectored>(this);
    }
};

class IOJobCompleteEvent : public IOEvent
{
public:
    std::thread::id m_tid;
    int rpipe;
    int wpipe;
    char status[4];

    size_t size;
    uint8_t* result;
    
    IOJobCompleteEvent(UserRequest* req, int rpipe, int wpipe, size_t size, uint8_t* result): IOEvent(RING_EVENT_JOB_COMPLETE, req), m_tid(), rpipe(rpipe), wpipe(wpipe), status{0}, size(size), result(result) { ; }
    virtual ~IOJobCompleteEvent() = default;

    static IOJobCompleteEvent* create(IOUserRequestEvent* event, int rpipe, int wpipe)
    {
        auto req = event->req;
        event->req = nullptr; //transfer ownership
        
        return new (s_allocator.allocate<IOJobCompleteEvent>()) IOJobCompleteEvent(req, rpipe, wpipe, 0, nullptr);
    }

    void release() override
    {
        if(this->req != nullptr) {
            this->req->release();
        }

        s_aio_allocator.freeAIOBuffer(this->result);
        s_allocator.freep2<IOJobCompleteEvent>(this);
    }
};
