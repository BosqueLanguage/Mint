#include "routes.h"

#ifdef DEFAULT_ROUTES
//Temp include for JSON parsing
#include "json.hpp"
typedef nlohmann::json json;
#endif

/*Compute the size of the buffer needed to send the result in bytes*/
size_t get_result_bytes(void* result_ptr) {
    xxxx;
}

/*Serialize the result into the provided buffer. Assumes the buffer is large enough*/
void serialize_result(void* result_ptr, uint8_t* buffer) {
    xxxx;
}

#ifdef DEFAULT_ROUTES
void* hello_route(RequestInput* input) {
    xxxx;
}

void* addition_route(RequestInput* input) {
    xxxx;
}

void* route_handler(RequestInput* input, RouteCB callback) {
    xxxx;
}
#endif
