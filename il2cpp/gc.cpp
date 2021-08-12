#include <cstdio>
#include <string>

namespace IL2CPP::GC
{
    void Free(void* ptr)
    {

    }
    void* Alloc(size_t size)
    {
        void* ptr;

        return ptr;
    }
    void* Realloc(void* ptr, size_t size)
    {
        void* ptr_new;
        memcpy(ptr_new, ptr, size);
        Free(ptr);
        return ptr_new;
    }
}