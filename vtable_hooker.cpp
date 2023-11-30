#include "vtable_hooker.h"
#include <string.h> // memcpy
#include <unistd.h>
#include <sys/mman.h>
#include <mod/icfg.h>
#include <aml.h>

void* vtablez[MAX_VTABLE_FUNCS] = {NULL};
int vtablez_offset = 0;

// This function is not gonna work correctly if vtable has "holes" (incomplete virtual class)
void HookVtableFunc(void* ptr, unsigned int funcNum, void* func, void** original, bool instantiate)
{
    if(!ptr || !func || funcNum < 0) return;
    if(ptr == aml || ptr == icfg) return; // you aint doin thiz dirty boy
    void** vtableArray = *(void***)ptr;
    
    int count = 0;
    while(vtableArray[count] != NULL) ++count;
    if(funcNum > count) return;
    
    if(instantiate)
    {
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
    if(!ptr || !func || funcNum < 0) return;
    if(ptr == aml || ptr == icfg) return; // you aint doin thiz dirty boy
    void** vtableArray = *(void***)ptr;
    
    if(funcNum > count) return;
    
    if(instantiate)
    {
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