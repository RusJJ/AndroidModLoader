#ifndef _IAML
#define _IAML

#include "interface.h"
#include <stdint.h>

class IAML
{
public:
    virtual bool HasMod(const char* szGUID) = 0;
    virtual bool HasModOfVersion(const char* szGUID, const char* szVersion) = 0;
    virtual uintptr_t GetLib(const char* szLib) = 0;
    virtual void Hook(void* handle, void* fnAddress, void** orgFnAddress = nullptr) = 0;
};

extern IAML* aml;
inline IAML* GetAMLInterface()
{
    return aml;
}

/* Just a hook. */
#define DECL_HOOK(_ret, _name, ...)                             \
    _ret (*_name)(__VA_ARGS__);	                                \
	_ret HookOf_##_name(__VA_ARGS__)
#define DECL_HOOKv(_name, ...)                                  \
    void (*_name)(__VA_ARGS__);	                                \
	void HookOf_##_name(__VA_ARGS__)
#define HOOK(_name, _fnAddr)                                    \
    aml->Hook((void*)(_fnAddr), (void*)(HookOf_##_name), (void**)(&_name));
#define HOOK_IL2CPP(_name, _methodInfo)                         \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(HookOf_##_name), (void**)(&_name));
#define UNHOOK(_name, _fnAddr)                                  \
    aml->Hook((void*)(_fnAddr), (void*)(_name), (void**)0);
#define UNHOOK_IL2CPP(_name, _methodInfo)                       \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(_name), (void**)0);

/* Just a hook with saveable original function pointer */
/* Only usable with UNHOOk */
#define DECL_HOOK2(_ret, _name, ...)                            \
    _ret (*_name)(__VA_ARGS__);	                                \
    static void* fnLocated##_name = 0;                          \
	_ret HookOf_##_name(__VA_ARGS__)
#define DECL_HOOK2v(_name, ...)                                 \
    void (*_name)(__VA_ARGS__);	                                \
    static void* fnLocated##_name = 0;                          \
	void HookOf_##_name(__VA_ARGS__)
#define HOOK2(_name, _fnAddr)                                   \
    fnLocated##_name = (void*)_fnAddr;                          \
    aml->Hook((void*)(_fnAddr), (void*)(HookOf_##_name), (void**)(&_name));
#define HOOK2_IL2CPP(_name, _methodInfo)                        \
    fnLocated##_name = (void*)_methodInfo->methodPointer;       \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(HookOf_##_name), (void**)(&_name));
#define UNHOOK2(_name)                                          \
    aml->Hook(fnLocated##_name, (void*)(_name), (void**)0);



#endif // _IAML