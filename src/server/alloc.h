#pragma once

#include "../common.h"

#define FREE_LIST_GET_NEXT(L) (*(void**)(L))
#define FREE_LIST_SET_NEXT(L, P) ((*(void**)(L)) = P)

constexpr size_t s_binidx(size_t size)
{
    size_t idx = 0;
    size_t s = 1;
    while (s < size && idx < 31)
    {
        s = s << 1;
        idx++;
    }
    return idx;
}

class ServerAllocator
{
private:
    void* m_allocs[31] = { nullptr };

    void* m_allocatep2_slow(size_t size);
public:
    template<typename T>
    T* allocate()
    {
        constexpr size_t bin = s_binidx(sizeof(T));

        void* res = this->m_allocs[bin];
        if(res != nullptr) {
            this->m_allocs[bin] = FREE_LIST_GET_NEXT(res);
        }
        else {
            res = this->m_allocatep2_slow(sizeof(T));
        }

        return res;
    }

    template<typename T>
    void freep2(T* ptr)
    {
        constexpr size_t bin = s_binidx(sizeof(T));

        FREE_LIST_SET_NEXT(ptr, this->m_allocs[bin]);
        this->m_allocs[bin] = ptr;
    }

    constexpr uint8_t* allocate_bytes(size_t size)
    {
        size_t bin = s_binidx(size);

        void* res = this->m_allocs[bin];
        if(res != nullptr) {
            this->m_allocs[bin] = FREE_LIST_GET_NEXT(res);
        }
        else {
            res = this->m_allocatep2_slow(size);
        }

        return (uint8_t*)res;
    }
};
