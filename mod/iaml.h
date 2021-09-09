#ifndef _IAML
#define _IAML

#include "interface.h"
#include <stdint.h>

#ifndef PAGE_SIZE
    #define PAGE_SIZE 4096
#endif

class IAML
{
public:
    /* AML 1.0.0.0 */
    virtual const char* GetCurrentGame() = 0;
    virtual const char* GetConfigPath() = 0;
    virtual bool HasMod(const char* szGUID) = 0;
    virtual bool HasModOfVersion(const char* szGUID, const char* szVersion) = 0;
    virtual uintptr_t GetLib(const char* szLib) = 0;
    virtual uintptr_t GetSym(void* handle, const char* sym) = 0;
    virtual void Hook(void* handle, void* fnAddress, void** orgFnAddress = nullptr) = 0;
    virtual void HookPLT(void* handle, void* fnAddress, void** orgFnAddress = nullptr) = 0;
    virtual int Unprot(uintptr_t handle, size_t len = PAGE_SIZE) = 0;
    virtual void Write(uintptr_t dest, uintptr_t src, size_t size) = 0;
    virtual void Read(uintptr_t src, uintptr_t dest, size_t size) = 0;
    virtual void PlaceNOP(uintptr_t addr, size_t count = 1) = 0; // Untested on ARMv8
    virtual void PlaceJMP(uintptr_t addr, uintptr_t dest) = 0; // Untested on ARMv8
    virtual void PlaceRET(uintptr_t addr) = 0; // Untested on ARMv8

    /* AML 1.0.0.4 */
    virtual const char* GetDataPath() = 0;
};

extern IAML* aml;
inline IAML* GetAMLInterface()
{
    return aml;
}


/* Unprotect that memory chunk for making changes */
#define UNPROT(_addr, ...)                                      \
    aml->Unprot((uintptr_t)(_addr), ( __VA_ARGS__ ));
/* Just write own info to the memory */
#define WRITE(_addr, _whatToWrite, _size)                       \
    aml->Write(_addr, _whatToWrite, _size);

/* Just a hook declaration */
#define DECL_HOOK(_ret, _name, ...)                             \
    _ret (*_name)(__VA_ARGS__);	                                \
	_ret HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = void */
#define DECL_HOOKv(_name, ...)                                  \
    void (*_name)(__VA_ARGS__);	                                \
	void HookOf_##_name(__VA_ARGS__)
/* Just a hook of a function */
#define HOOK(_name, _fnAddr)                                    \
    aml->Hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));
/* Just a hook of a function located in PLT section (by address!) */
#define HOOKPLT(_name, _fnAddr)                                 \
    aml->HookPLT((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));
/* Just a hook of a function hidden behind IL2CPP */
#define HOOK_IL2CPP(_name, _methodInfo)                         \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&HookOf_##_name), (void**)(&_name));
/* Unhook a function (unsafe, actually) */
#define UNHOOK(_name, _fnAddr)                                  \
    aml->Hook((void*)(_fnAddr), (void*)(&_name), (void**)0);
/* Unhook an IL2CPP function (unsafe, actually) */
#define UNHOOK_IL2CPP(_name, _methodInfo)                       \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&_name), (void**)0);

/* Just a hook decl with saveable original function pointer */
#define DECL_HOOK2(_ret, _name, ...)                            \
    _ret (*_name)(__VA_ARGS__);	                                \
    static void* fnLocated##_name = 0;                          \
	_ret HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = void and saveable original function pointer */
#define DECL_HOOK2v(_name, ...)                                 \
    void (*_name)(__VA_ARGS__);	                                \
    static void* fnLocated##_name = 0;                          \
	void HookOf_##_name(__VA_ARGS__)
/* Just a hook of a function and save original function pointer */
#define HOOK2(_name, _fnAddr)                                   \
    fnLocated##_name = (void*)_fnAddr;                          \
    aml->Hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));
/* Just a hook of a function located in PLT section (by address!) and save original function pointer */
#define HOOK2PLT(_name, _fnAddr)                                \
    fnLocated##_name = (void*)_fnAddr;                          \
    aml->HookPLT((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));
/* Just a hook of a function hidden behind IL2CPP and save original function pointer */
#define HOOK2_IL2CPP(_name, _methodInfo)                        \
    fnLocated##_name = (void*)_methodInfo->methodPointer;       \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&HookOf_##_name), (void**)(&_name));
/* Unhook a function (unsafe, actually) that was saved before */
#define UNHOOK2(_name)                                          \
    aml->Hook(fnLocated##_name, (void*)(&_name), (void**)0);

#endif // _IAML