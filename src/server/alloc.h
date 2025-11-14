#pragma once

#include "common.h"

#define FREE_LIST_GET_NEXT(L) (*(void**)(L))
#define FREE_LIST_SET_NEXT(L, P) ((*(void**)(L)) = P)

#define AIO_BUFFER_SIZE 8192

constexpr size_t s_binidx(size_t size)
{
    return std::ceil(std::log2(size));
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

        return (T*)res;
    }

    template<typename T>
    void freep2(T* ptr)
    {
        if(ptr == nullptr) {
            return;
        }

        constexpr size_t bin = s_binidx(sizeof(T));

        FREE_LIST_SET_NEXT(ptr, this->m_allocs[bin]);
        this->m_allocs[bin] = ptr;
    }

    uint8_t* allocatebytesp2(size_t size)
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

    char* strcopyp2(const char* str, size_t size)
    {
        if(str == nullptr) {
            return nullptr;
        }

        char* res = (char*)this->allocatebytesp2(size + 1);
        memcpy(res, str, size);
        res[size] = '\0';

        return res;
    }

    char* strcopyp2(const char* str)
    {
        if(str == nullptr) {
            return nullptr;
        }

        size_t size = s_strlen(str);
        char* res = (char*)this->allocatebytesp2(size + 1);
        memcpy(res, str, size);
        res[size] = '\0';

        return res;
    }

    void freebytesp2(uint8_t* ptr, size_t size)
    {
        if(ptr == nullptr) {
            return;
        }
        
        size_t bin = s_binidx(size);

        FREE_LIST_SET_NEXT(ptr, this->m_allocs[bin]);
        this->m_allocs[bin] = ptr;
    }
};

class AIOAllocator
{
private:
    void** m_allocs;
    std::mutex g_pages_mutex;

    void* m_allocate_slow();
public:
    AIOAllocator(): m_allocs(nullptr), g_pages_mutex()
    { 
        ; 
    }
    
    ~AIOAllocator()
    { 
        void* current = this->m_allocs;
        while(current != nullptr) {
            void* next = FREE_LIST_GET_NEXT(current);
            free(current);
            current = next;
        }
    }

    uint8_t* allocAIOBuffer()
    {
        std::lock_guard<std::mutex> lock(this->g_pages_mutex);

        void* res = this->m_allocs;
        if(res != nullptr) {
            *this->m_allocs = FREE_LIST_GET_NEXT(res);
        }
        else {
            res = this->m_allocate_slow();
        }

        return (uint8_t*)res;
    }

    void freeAIOBuffer(uint8_t* ptr)
    {
        if(ptr == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(this->g_pages_mutex);

        FREE_LIST_SET_NEXT(ptr, *this->m_allocs);
        *this->m_allocs = ptr;
    }
};

extern AIOAllocator s_aio_allocator;
extern ServerAllocator s_allocator;

