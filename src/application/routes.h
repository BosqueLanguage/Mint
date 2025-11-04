#pragma once

#include <stdint.h>
#include <stddef.h>

struct RequestInput {
    const char* route;
    uint8_t* data;
    size_t size;
};

enum class RouteResultKind {
    InvalidRequest,
    StaticFileRequest,
    OperationRequest
};

typedef void (*RouteCB)(RequestInput* input, RouteResultKind kind, void* result);

/*Compute the size of the buffer needed to send the result in bytes*/
size_t get_result_bytes(void* result_ptr);

/*Serialize the result into the provided buffer. Assumes the buffer is large enough*/
void serialize_result(void* result_ptr, uint8_t* buffer);

#ifdef DEFAULT_ROUTES
void* hello_route(RequestInput* input);
void* addition_route(RequestInput* input);

void* route_handler(RequestInput* input, RouteCB callback);
#endif
