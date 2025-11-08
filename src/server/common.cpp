#include "common.h"

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
