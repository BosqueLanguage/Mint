#include "routes.h"

#ifdef DEFAULT_ROUTES
//Temp include for JSON parsing
#include "json.hpp"
typedef nlohmann::json json;
#endif

#ifdef DEFAULT_ROUTES
void* hello_route(RequestInput* input) 
{
    xxxx;
}

void* addition_route(RequestInput* input) 
{
    xxxx;
}

size_t get_result_bytes(void* result)
{
    return strlen((char*)result);
}

/**
 * Serialize the result into the provided buffer. Assumes the buffer is large enough
 **/
void serialize_result(void* result, uint8_t* buffer)
{
    size_t len = strlen((char*)result);
    memcpy(buffer, result, len);
}

/**
 * Actually handle routing for the application. Returns true if a route was found dispatched false otherwise
 */
bool route_handler(RequestInput* input, StaticFileCB staticCB, RouteCB dynamicCB)
{
    if(strcmp(input->route, "/hello") == 0)
    {
        void* res = hello_route(input);
        dynamicCB(input, res);
        return true;
    }
    else if(strcmp(input->route, "/add") == 0)
    {
        void* res = addition_route(input);
        dynamicCB(input, res);
        return true;
    }
    else if(strcmp(input->route, "/static") == 0) //we are very static for now -- later build a simple local file cache and loader
    {
        staticCB(input, "static file contents", 20);
        return true;
    }
    else
    {
        return false;
    }
}
#endif
