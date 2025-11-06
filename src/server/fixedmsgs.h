#pragma once

#define SERVER_STRING "Server: Bosque RSHook\r\n"

#define UNSUPPORTED_VERB_MSG ="HTTP/1.0 400 Bad Request\r\nContent-type: text/html\r\n\r\n<html><head><title>Unsupported Operation Type</title></head><body><h1>Bad Request</h1><p>REST Style hooks for Bosque services should be GET or POST</p></body></html>"
#define MALFORMED_REQUEST_MSG ="HTTP/1.0 400 Bad Request\r\nContent-type: text/html\r\n\r\n<html><head><title>Malformed Request</title></head><body><h1>Bad Request</h1><p>Request could not be processed</p></body></html>"
#define CONTENT_404_MSG ="HTTP/1.0 404 Not Found\r\nContent-type: text/html\r\n\r\n<html><head><title>Resource Not Found</title></head><body><h1>Not Found (404)</h1><p>Request for an unknown resource</p></body></html>"
