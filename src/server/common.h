#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>

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
bool route_handler(RequestInput* input, ImmediateContentCB immedeiateCB, StaticFileCB staticCB, RouteCB dynamicCB);
