#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <cstdlib>
#include <cmath>
#include <cstring>

#include <liburing.h>

enum class RING_EVENT_TYPE : int
{
    EVENT_TYPE_ACCEPT = 0,
    EVENT_TYPE_READ   = 1,
    EVENT_TYPE_WRITE  = 2
};

struct request
{
    RING_EVENT_TYPE event_type;
    int iovec_count;
    int client_socket;
    struct iovec iov[];
};

/**
 * Data structure representing the input to a route handler as extracted from the HTTP request
 **/
struct RequestInput 
{
    int client_socket;
    const char* route;
    const char* data;
};

/**
 * Callback function type for route handlers that load data from a static file -- nullptr resource was unable to be loaded
 **/
typedef void (*StaticFileCB)(RequestInput* input, const char* path);

/** 
 * Callback function type for route handlers that compute a result of some (opaque) type -- nullptr if the call failed
 **/
typedef void (*RouteCB)(RequestInput* input, void* result);

/**
 * Compute the size of the buffer needed to send the result in bytes
 **/
size_t get_result_bytes(void* result);

/**
 * Serialize the result into the provided buffer. Assumes the buffer is large enough
 **/
void serialize_result(void* result, uint8_t* buffer);

/**
 * Actually handle routing for the application. Returns true if a route was found dispatched false otherwise
 */
bool route_handler(RequestInput* input, StaticFileCB staticCB, RouteCB dynamicCB);
