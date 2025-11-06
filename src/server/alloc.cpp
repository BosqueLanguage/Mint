#include "alloc.h"

void* ServerAllocator::m_allocatep2_slow(size_t size)
{
    /** 
     * Temp using malloc for slow path until we implement custom backing allocation 
     **/
    return aligned_alloc(16, size);
}
