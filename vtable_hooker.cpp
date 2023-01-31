#include "vtable_hooker.h"
#include <string.h> // memcpy
#include <unistd.h>
#include <sys/mman.h>
#include <mod/logger.h>

void* vtablez[MAX_VTABLE_FUNCS];
int vtablez_offset = 0;

void HookVtableFunc(void* ptr, unsigned int funcNum, void* func, void** original, bool instantiate)
{
    if(!ptr || !func || funcNum <= 0) return;
    
    void** vtableTemp = *(void***)ptr;
    while(vtableTemp - 1 != NULL) --vtableTemp;
    
    int count = 0;
    while(vtableTemp + count != NULL) ++count;
    
    if(count <= 0 || count > funcNum) return;
    
    if(instantiate)
    {
        memcpy(&vtablez[vtablez_offset], vtableTemp, count * sizeof(void*));
        *(void***)ptr = &vtablez[vtablez_offset];
        vtablez_offset += count;
        vtableTemp = *(void***)ptr;
    }
    else
    {
        mprotect((void*)((uintptr_t)&vtableTemp[funcNum - 1] & 0xFFFFF000), sizeof(void*), PROT_READ | PROT_WRITE | PROT_EXEC);
    }
    
    if(original != NULL) *((uintptr_t*)original) = (uintptr_t)vtableTemp[funcNum - 1];
    vtableTemp[funcNum - 1] = (void*)func;
}
