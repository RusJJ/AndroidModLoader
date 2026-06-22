#include "vtable_hooker.h"
#include <string.h> // memcpy
#include <unistd.h>
#include <sys/mman.h>
#include <mod/icfg.h>
#include <aml.h>

void* vtablez[MAX_VTABLE_FUNCS] = {NULL};
size_t vtablez_offset = 0;

static bool ReserveVtableSlots(size_t count)
{
    return count > 0 && count <= MAX_VTABLE_FUNCS && vtablez_offset <= MAX_VTABLE_FUNCS - count;
}

// This function is not gonna work correctly if vtable has "holes" (incomplete/abstract virtual class)
void HookVtableFunc(void* ptr, unsigned int funcNum, void* func, void** original, bool instantiate)
{
    if(!ptr || !func) return;
    if(ptr == aml || ptr == icfg) return; // you aint doin thiz dirty boy
    void** vtableArray = *(void***)ptr;
    
    size_t count = 0;
    while(count < MAX_VTABLE_FUNCS && vtableArray[count] != NULL) ++count;
    if(count == MAX_VTABLE_FUNCS || funcNum >= count) return;
    
    if(instantiate)
    {
        if(!ReserveVtableSlots(count)) return;
        memcpy(&vtablez[vtablez_offset], vtableArray, count * sizeof(void*));
        *(void***)ptr = &vtablez[vtablez_offset];
        vtableArray = *(void***)ptr;
        vtablez_offset += count;
    }
    else
    {
        g_pAML->Unprot((uintptr_t)&vtableArray[funcNum], sizeof(void*));
    }
    
    if(original != NULL) *original = vtableArray[funcNum];
    vtableArray[funcNum] = func;
}

// Better func but you need to know how much funcs it has
void HookVtableFunc(void* ptr, unsigned int funcNum, unsigned int count, void* func, void** original, bool instantiate)
{
    if(!ptr || !func) return;
    if(ptr == aml || ptr == icfg) return; // you aint doin thiz dirty boy
    void** vtableArray = *(void***)ptr;
    
    if(funcNum >= count) return;
    
    if(instantiate)
    {
        if(!ReserveVtableSlots(count)) return;
        memcpy(&vtablez[vtablez_offset], vtableArray, count * sizeof(void*));
        *(void***)ptr = &vtablez[vtablez_offset];
        vtableArray = *(void***)ptr;
        vtablez_offset += count;
    }
    else
    {
        g_pAML->Unprot((uintptr_t)&vtableArray[funcNum], sizeof(void*));
    }
    
    if(original != NULL) *original = vtableArray[funcNum];
    vtableArray[funcNum] = func;
}
