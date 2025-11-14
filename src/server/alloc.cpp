#include "alloc.h"

constexpr size_t s_alignedsize(size_t size)
{
    size_t rsize = std::max<size_t>(16, size);
    size_t pv = std::ceil(std::log2(rsize));

    return (size_t)std::pow(2, pv);
}

void* ServerAllocator::m_allocatep2_slow(size_t size)
{
    /** 
     * Temp using malloc for slow path until we implement custom backing allocation 
     **/
    size_t processed_size = s_alignedsize(size);
    return aligned_alloc(16, processed_size);
}

void* AIOAllocator::m_allocate_slow()
{
    /** 
     * Temp using malloc for slow path until we implement custom backing allocation 
     **/
    return aligned_alloc(16, AIO_BUFFER_SIZE); //default aio buffer size
}


AIOAllocator s_aio_allocator;
ServerAllocator s_allocator;
