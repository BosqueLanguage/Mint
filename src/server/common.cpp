#include "common.h"


bool get_line(const char *src, char *dest, size_t dest_sz) 
{
    for (size_t i = 0; i < dest_sz; i++) {
        dest[i] = src[i];
        if (src[i] == '\r' && src[i + 1] == '\n') {
            dest[i] = '\0';
            return true;
        }
    }
    
    return false;
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
