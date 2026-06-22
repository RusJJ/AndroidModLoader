/* DO NOT CHANGE IT */

#ifndef __GETINTERFACE_H
#define __GETINTERFACE_H

#if defined(_WIN32) || defined(_WIN64)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif
#define DEFAULT_LIB_NAME    "AML"

#define WRAP_INTERFACE(__interface_name, __interface_var)	RegisterInterface(#__interface_name, __interface_var)

typedef void* (*GetInterfaceFn)(const char*);
typedef void* (*RegInterfaceFn)(const char*, void*);

inline void* GetInterface(const char* szInterfaceName)
{
#if defined(_WIN32) || defined(_WIN64)
    HMODULE module = GetModuleHandleA(DEFAULT_LIB_NAME ".dll");
    if(!module) return NULL;
    GetInterfaceFn _GetInterface = (GetInterfaceFn)GetProcAddress(module, "GetInterface");
#else
    void* module = dlopen("lib" DEFAULT_LIB_NAME ".so", RTLD_NOW);
    if(!module) return NULL;
    GetInterfaceFn _GetInterface = (GetInterfaceFn)dlsym(module, "GetInterface");
#endif
    if(!_GetInterface) return NULL;
    return _GetInterface(szInterfaceName);
}

inline void RegisterInterface(const char* szInterfaceName, void* pInterface)
{
#if defined(_WIN32) || defined(_WIN64)
    HMODULE module = GetModuleHandleA(DEFAULT_LIB_NAME ".dll");
    if(!module) return;
    RegInterfaceFn _RegInterface = (RegInterfaceFn)GetProcAddress(module, "CreateInterface");
#else
    void* module = dlopen("lib" DEFAULT_LIB_NAME ".so", RTLD_NOW);
    if(!module) return;
    RegInterfaceFn _RegInterface = (RegInterfaceFn)dlsym(module, "CreateInterface");
#endif
    if(!_RegInterface) return;
    _RegInterface(szInterfaceName, pInterface);
}

#endif // __GETINTERFACE_H
