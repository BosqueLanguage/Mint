#include "common.h"

size_t s_strlen(const char* str)
{
    if(str == nullptr) {
        return 0;
    }

    return strlen(str);
}
