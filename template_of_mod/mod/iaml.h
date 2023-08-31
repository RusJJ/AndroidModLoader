#ifndef _IAML
#define _IAML

#include "interface.h"
#include <jni.h>
#include <stdint.h>

// Because the name was changed to be more understandable
#define PlaceB PlaceJMP

#ifndef PAGE_SIZE
    #define PAGE_SIZE 4096
#endif

enum eManifestPermissions
{
    P_READ_EXTERNAL_STORAGE = 0,
    P_WRITE_EXTERNAL_STORAGE,
}; // Unused
#if defined(__cplusplus)
    extern "C"
#endif
size_t strlen(char const*);

class IAML
{
public:
    /* AML 1.0.0.0 */
    virtual const char* GetCurrentGame() = 0;
    virtual const char* GetConfigPath() = 0;
    virtual bool        HasMod(const char* szGUID) = 0;
    virtual bool        HasModOfVersion(const char* szGUID, const char* szVersion) = 0;
    virtual uintptr_t   GetLib(const char* szLib) = 0;
    virtual uintptr_t   GetSym(void* handle, const char* sym) = 0;
    virtual bool        Hook(void* handle, void* fnAddress, void** orgFnAddress = NULL) = 0;
    virtual bool        HookPLT(void* handle, void* fnAddress, void** orgFnAddress = NULL) = 0;
    virtual int         Unprot(uintptr_t handle, size_t len = PAGE_SIZE) = 0;
    virtual void        Write(uintptr_t dest, uintptr_t src, size_t size) = 0;
    virtual void        Read(uintptr_t src, uintptr_t dest, size_t size) = 0;
    virtual int         PlaceNOP(uintptr_t addr, size_t count = 1) = 0;
    virtual int         PlaceJMP(uintptr_t addr, uintptr_t dest) = 0;
    virtual int         PlaceRET(uintptr_t addr) = 0;

    /* AML 1.0.0.4 */
    virtual const char* GetDataPath() = 0; // /data/data/.../*

    /* AML 1.0.0.5 */
    virtual const char* GetAndroidDataPath() = 0; // /sdcard/Android/data/.../files/*
    virtual uintptr_t   GetSym(uintptr_t libAddr, const char* sym) = 0; // An additional func but it uses ADDRESS instead of a HANDLE

    /* AML 1.0.0.6 */
    virtual uintptr_t   GetLibLength(const char* szLib) = 0;
    virtual int         Redirect(uintptr_t addr, uintptr_t to) = 0; // Move directly to "to" from "addr" with the same stack and registers
    virtual void        PlaceBL(uintptr_t addr, uintptr_t dest) = 0;
    virtual void        PlaceBLX(uintptr_t addr, uintptr_t dest) = 0;
    virtual uintptr_t   PatternScan(const char* pattern, const char* soLib) = 0;
    virtual uintptr_t   PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen) = 0;
    
    /* AML 1.0.1 */
    virtual void        PatchForThumb(bool forThumb) = 0;
    virtual const char* GetFeatures() = 0;
    virtual void        HookVtableFunc(void* ptr, unsigned int funcNum, void* fnAddress, void** orgFnAddress = NULL, bool instantiate = false) = 0; // unsafe
    virtual bool        IsGameFaked() = 0;
    virtual const char* GetRealCurrentGame() = 0;
    virtual void*       GetLibHandle(const char* soLib) = 0;
    virtual void*       GetLibHandle(uintptr_t addr) = 0;
    // xDL (will return 0 if xDL is not used)
    // These functions always exists
    // So no need to check for their availability
    virtual bool        IsCorrectXDLHandle(void* ptr) = 0;
    virtual uintptr_t   GetLibXDL(void* ptr) = 0;
    virtual uintptr_t   GetAddrBaseXDL(uintptr_t addr) = 0;
    virtual size_t      GetSymSizeXDL(void* ptr) = 0;
    virtual const char* GetSymNameXDL(void* ptr) = 0;
    
    /* AML 1.0.2 */
    virtual void        ShowToast(bool longerDuration, const char* fmt, ...) = 0;
    virtual bool        DownloadFile(const char* url, const char* saveto) = 0;
    virtual bool        DownloadFileToData(const char* url, char* out, size_t outLen) = 0;
    virtual void        FileMD5(const char* path, char* out, size_t out_len) = 0;
    virtual int         GetModsLoadedCount() = 0;
    virtual JNIEnv*     GetJNIEnvironment() = 0;
    virtual jobject     GetAppContextObject() = 0;
    
    /* AML 1.0.2.1 */
    virtual bool        HasModOfBiggerVersion(const char* szGUID, const char* szVersion) = 0;
    
    /* AML 1.0.4 */
    virtual void        HookVtableFunc(void* ptr, unsigned int funcNum, unsigned int count, void* fnAddress, void** orgFnAddress = NULL, bool instantiate = false) = 0;
    virtual int         PlaceNOP4(uintptr_t addr, size_t count = 1) = 0;
    virtual const char* GetAndroidDataRootPath() = 0; // /sdcard/Android/data/.../* (not /files/ !!!)
    virtual bool        HookB(void* handle, void* fnAddress, void** orgFnAddress = NULL) = 0;
    virtual bool        HookBL(void* handle, void* fnAddress, void** orgFnAddress = NULL) = 0;
    virtual bool        HookBLX(void* handle, void* fnAddress, void** orgFnAddress = NULL) = 0;

    // Inlines (shortcuts for you!)
    inline void         Write(uintptr_t dest, const char* str, size_t size) { Write(dest, (uintptr_t)str, size); } // Inline
    inline void         Write(uintptr_t dest, const char* str) { Write(dest, (uintptr_t)str, strlen(str)); } // Inline
    inline void         Write8(uintptr_t dest, uint8_t v) { uint8_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 1); } // Inline
    inline void         Write16(uintptr_t dest, uint16_t v) { uint16_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 2); } // Inline
    inline void         Write32(uintptr_t dest, uint32_t v) { uint32_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 4); } // Inline
    inline void         WriteAddr(uintptr_t dest, uintptr_t addr) { uintptr_t addrPtr = addr; Write(dest, (uintptr_t)&addrPtr, sizeof(uintptr_t)); } // Inline
    inline void         WriteAddr(uintptr_t dest, void* addr) { uintptr_t addrPtr = (uintptr_t)addr; Write(dest, (uintptr_t)&addrPtr, sizeof(uintptr_t)); } // Inline
    // Can be used with HookVtableFunc to not to instantiate vtable for 1000 times!
    inline void**       GetVtable(void* ptr) { return *(void***)ptr; }
    inline void         SetVtable(void* ptr, void** vtable) { *(void***)ptr = vtable; }
};

extern IAML* aml;
inline IAML* GetAMLInterface() { return aml; }

/* Do not use big conversions */
#define SET_TO(__a1, __a2) *(void**)&(__a1) = (void*)(__a2)

/* Unprotect that memory chunk for making changes */
#define UNPROT(_addr, _count)                                   \
    aml->Unprot((uintptr_t)(_addr), ( _count ));
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
    aml->Hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a function located in PLT section (by address!) */
#define HOOKPLT(_name, _fnAddr)                                 \
    aml->HookPLT((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a branch */
#define HOOKB(_name, _fnAddr)                                   \
    aml->HookB((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a branch with link */
#define HOOKBL(_name, _fnAddr)                                  \
    aml->HookBL((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a branch with link (and registers exchange) */
#ifdef AML32
    #define HOOKBLX(_name, _fnAddr)                             \
        aml->HookBLX((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
#endif
/* Just a hook of a function hidden behind IL2CPP */
#define HOOK_IL2CPP(_name, _methodInfo)                         \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&HookOf_##_name), (void**)(&_name))

#endif // _IAML
