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
    GetInterfaceFn _GetInterface = (GetInterfaceFn)GetProcAddress(GetModuleHandleA(DEFAULT_LIB_NAME ".dll"), "GetInterface");
#else
    GetInterfaceFn _GetInterface = (GetInterfaceFn)dlsym((void*)dlopen("lib" DEFAULT_LIB_NAME ".so", RTLD_NOW), "GetInterface");
#endif
    return _GetInterface(szInterfaceName);
}

inline void RegisterInterface(const char* szInterfaceName, void* pInterface)
{
#if defined(_WIN32) || defined(_WIN64)
    RegInterfaceFn _RegInterface = (RegInterfaceFn)GetProcAddress(GetModuleHandleA(DEFAULT_LIB_NAME ".dll"), "CreateInterface");
#else
    RegInterfaceFn _RegInterface = (RegInterfaceFn)dlsym((void*)dlopen("lib" DEFAULT_LIB_NAME ".so", RTLD_NOW), "CreateInterface");
#endif
    _RegInterface(szInterfaceName, pInterface);
}

#endif // __GETINTERFACE_H