#ifndef _IAML
#define _IAML

// Usage: place 3 lines somewhere in the code AFTER #include <mod/amlmod.h>
// #if !defined(IAML_VER) && IAML_VER < 01020100
//     #error "You need to update your MOD folder to 1.2.1!"
// #endif
#define IAML_VER 01020100

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
    virtual const char* GetCurrentGame();
    virtual const char* GetConfigPath();
    virtual bool        HasMod(const char* szGUID);
    virtual bool        HasModOfVersion(const char* szGUID, const char* szVersion);
    virtual uintptr_t   GetLib(const char* szLib);
    virtual uintptr_t   GetSym(void* handle, const char* sym);
    virtual bool        Hook(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    virtual bool        HookPLT(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    virtual int         Unprot(uintptr_t handle, size_t len = PAGE_SIZE);
    virtual void        Write(uintptr_t dest, uintptr_t src, size_t size);
    virtual void        Read(uintptr_t src, uintptr_t dest, size_t size);
    virtual int         PlaceNOP(uintptr_t addr, size_t count = 1);
    virtual int         PlaceJMP(uintptr_t addr, uintptr_t dest);
    virtual int         PlaceRET(uintptr_t addr);

    /* AML 1.0.0.4 */
    virtual const char* GetDataPath(); // /data/data/.../*

    /* AML 1.0.0.5 */
    virtual const char* GetAndroidDataPath(); // /sdcard/Android/data/.../files/*
    virtual uintptr_t   GetSym(uintptr_t libAddr, const char* sym); // An additional func but it uses ADDRESS instead of a HANDLE

    /* AML 1.0.0.6 */
    virtual uintptr_t   GetLibLength(const char* szLib);
    virtual int         Redirect(uintptr_t addr, uintptr_t to); // Move directly to "to" from "addr" with the same stack and registers
    virtual void        PlaceBL(uintptr_t addr, uintptr_t dest);
    virtual void        PlaceBLX(uintptr_t addr, uintptr_t dest);
    virtual uintptr_t   PatternScan(const char* pattern, const char* soLib);
    virtual uintptr_t   PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
    
    /* AML 1.0.1 */
    virtual void        PatchForThumb(bool forThumb);
    virtual const char* GetFeatures();
    virtual void        HookVtableFunc(void* ptr, unsigned int funcNum, void* fnAddress, void** orgFnAddress = NULL, bool instantiate = false); // unsafe
    virtual bool        IsGameFaked();
    virtual const char* GetRealCurrentGame();
    virtual void*       GetLibHandle(const char* soLib);
    virtual void*       GetLibHandle(uintptr_t addr);
    // xDL (will return 0 if xDL is not used)
    // These functions always exists
    // So no need to check for their availability
    virtual bool        IsCorrectXDLHandle(void* ptr);
    virtual uintptr_t   GetLibXDL(void* ptr);
    virtual uintptr_t   GetAddrBaseXDL(uintptr_t addr);
    virtual size_t      GetSymSizeXDL(void* ptr);
    virtual const char* GetSymNameXDL(void* ptr);
    
    /* AML 1.0.2 */
    virtual void        ShowToast(bool longerDuration, const char* fmt, ...);
    virtual bool        DownloadFile(const char* url, const char* saveto);
    virtual bool        DownloadFileToData(const char* url, char* out, size_t outLen);
    virtual void        FileMD5(const char* path, char* out, size_t out_len);
    virtual int         GetModsLoadedCount();
    virtual JNIEnv*     GetJNIEnvironment();
    virtual jobject     GetAppContextObject();
    
    /* AML 1.0.2.1 */
    virtual bool        HasModOfBiggerVersion(const char* szGUID, const char* szVersion);
    
    /* AML 1.0.4 */
    virtual void        HookVtableFunc(void* ptr, unsigned int funcNum, unsigned int count, void* fnAddress, void** orgFnAddress = NULL, bool instantiate = false);
    virtual int         PlaceNOP4(uintptr_t addr, size_t count = 1);
    virtual const char* GetAndroidDataRootPath(); // /sdcard/Android/data/.../* (not /files/ !!!)
    virtual bool        HookB(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    virtual bool        HookBL(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    virtual bool        HookBLX(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    
    /* AML 1.2 */
    virtual void        MLSSaveFile();
    virtual bool        MLSHasValue(const char* key);
    virtual void        MLSDeleteValue(const char* key);
    virtual void        MLSSetInt(const char* key, int32_t val);
    virtual void        MLSSetFloat(const char* key, float val);
    virtual void        MLSSetInt64(const char* key, int64_t val);
    virtual void        MLSSetStr(const char* key, const char *val);
    virtual bool        MLSGetInt(const char* key, int32_t *val);
    virtual bool        MLSGetFloat(const char* key, float *val);
    virtual bool        MLSGetInt64(const char* key, int64_t *val);
    virtual bool        MLSGetStr(const char* key, char *val, size_t len);
    
    /* AML 1.2.1 */
    virtual bool        IsThumbAddr(uintptr_t addr);
    virtual uintptr_t   GetBranchDest(uintptr_t addr);



    // Inlines (shortcuts for you!)
    inline void         Write(uintptr_t dest, const char* str, size_t size) { Write(dest, (uintptr_t)str, size); } // Inline
    inline void         Write(uintptr_t dest, const char* str) { Write(dest, (uintptr_t)str, strlen(str)); } // Inline
    inline void         Write8(uintptr_t dest, uint8_t v) { uint8_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 1); } // Inline
    inline void         Write16(uintptr_t dest, uint16_t v) { uint16_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 2); } // Inline
    inline void         Write32(uintptr_t dest, uint32_t v) { uint32_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 4); } // Inline
    inline void         WriteFloat(uintptr_t dest, float v) { float vPtr = v; Write(dest, (uintptr_t)&vPtr, 4); } // Inline
    inline void         WriteAddr(uintptr_t dest, uintptr_t addr) { uintptr_t addrPtr = addr; Write(dest, (uintptr_t)&addrPtr, sizeof(uintptr_t)); } // Inline
    inline void         WriteAddr(uintptr_t dest, void* addr) { uintptr_t addrPtr = (uintptr_t)addr; Write(dest, (uintptr_t)&addrPtr, sizeof(uintptr_t)); } // Inline
    // Can be used with HookVtableFunc to not to instantiate vtable for 1000 times!
    inline void**       GetVtable(void* ptr) { return *(void***)ptr; }
    inline void         SetVtable(void* ptr, void** vtable) { *(void***)ptr = vtable; }
};

extern IAML* aml;
inline IAML* GetAMLInterface() { return aml; }

/* Do not use big conversions */
#define SET_TO(__a1, __a2)  *(void**)&(__a1) = (void*)(__a2)
#define SETSYM_TO(__a1, __a2, __a3)  *(void**)&(__a1) = (void*)(aml->GetSym(__a2, __a3))
#define AS_ADDR(__a1)       *(uintptr_t*)&(__a1)

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
/* Just a hook declaration with return type = bool */
#define DECL_HOOKb(_name, ...)                                  \
    bool (*_name)(__VA_ARGS__);	                                \
	bool HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = int */
#define DECL_HOOKi(_name, ...)                                  \
    int (*_name)(__VA_ARGS__);	                                \
	int HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = void* */
#define DECL_HOOKp(_name, ...)                                  \
    void* (*_name)(__VA_ARGS__);	                            \
	void* HookOf_##_name(__VA_ARGS__)

/* Just a hook of a function */
#define HOOK(_name, _fnAddr)                                    \
    aml->Hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a function (but simpler usage) */
#define HOOKSYM(_name, _libHndl, _fnSym)                        \
    aml->Hook((void*)(aml->GetSym(_libHndl, _fnSym)), (void*)(&HookOf_##_name), (void**)(&_name));
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
#define HOOKBLX(_name, _fnAddr)                                 \
    aml->HookBLX((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a function hidden behind IL2CPP */
#define HOOK_IL2CPP(_name, _methodInfo)                         \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&HookOf_##_name), (void**)(&_name))

#endif // _IAML