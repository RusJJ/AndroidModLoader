#ifndef _IAML
#define _IAML

#include "interface.h"
#include <jni.h>
#include <stdint.h>

// Because the name was changed to be correct
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
    enum GAME_ID : char\
    {
		UN_GAME,
		GTA3,
		GTAVC,
		GTASA,
		GTALCS,
		GTACTW
    };
    
    enum GAME_VER : char
    {
		UN_VER,
		// 32
		III_1_8,
		VC_1_09,
		SA_2_00,
		LCS_2_4,
		CTW_1_04,
		// hign ver
		III_1_9,
		VC_1_12,
		SA_2_10,
		// 64
		III_1_9_64,
		VC_1_12_64,
		SA_2_10_64
    };
    /* AML 1.0.0.0 */
    virtual const char* GetCurrentGame() = 0;
    virtual const char* GetConfigPath() = 0;
    virtual bool        HasMod(const char* szGUID) = 0;
    virtual bool        HasModOfVersion(const char* szGUID, const char* szVersion) = 0;
    virtual uintptr_t   GetLib(const char* szLib) = 0;
    virtual uintptr_t   GetSym(void* handle, const char* sym) = 0;
    virtual void*       Hook(void* addr, void* fnAddress, void** orgFnAddress = NULL) = 0;
    virtual void*       HookPLT(void* addr, void* fnAddress, void** orgFnAddress = NULL) = 0;
    virtual int         Unprot(uintptr_t addr, size_t len = PAGE_SIZE) = 0;
    virtual void        Write(uintptr_t dest, uintptr_t src, size_t size) = 0;
    inline void         Write(uintptr_t dest, const char* str, size_t size) { Write(dest, (uintptr_t)str, size); } // Inline
    inline void         Write(uintptr_t dest, const char* str) { Write(dest, (uintptr_t)str, strlen(str)); } // Inline
    inline void         Write8(uintptr_t dest, uint8_t v) { uint8_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 1); } // Inline
    inline void         Write16(uintptr_t dest, uint16_t v) { uint16_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 2); } // Inline
    inline void         Write32(uintptr_t dest, uint32_t v) { uint32_t vPtr = v; Write(dest, (uintptr_t)&vPtr, 4); } // Inline
    inline void         WriteAddr(uintptr_t dest, uintptr_t addr) { uintptr_t addrPtr = addr; Write(dest, (uintptr_t)&addrPtr, sizeof(uintptr_t)); } // Inline
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
    virtual int         Redirect(uintptr_t addr, uintptr_t to) = 0; // Move directly to "to" from "addr" with the same stack
    virtual void        PlaceBL(uintptr_t addr, uintptr_t dest) = 0;
    virtual void        PlaceBLX(uintptr_t addr, uintptr_t dest) = 0;
    virtual uintptr_t   PatternScan(const char* pattern, const char* soLib) = 0;
    virtual uintptr_t   PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen) = 0;
    
    /* AML 1.0.1 */
    virtual void        PatchForThumb(bool forThumb) = 0;
    virtual const char* GetFeatures() = 0;
    virtual void        HookVtableFunc(void* ptr, unsigned int funcNum, void* fnAddress, void** orgFnAddress = (void**)0, bool instantiate = false) = 0;
    virtual bool        IsGameFaked() = 0;
    virtual const char* GetRealCurrentGame() = 0;
    virtual void*       GetLibHandle(const char* soLib) = 0;
    virtual void*       GetLibHandle(uintptr_t addr) = 0;
    // xDL (will return 0 if xDL is not used)
    // These functions always exists
    // So no need to check for their availability
    // AML 1.0.4 //fix name
    virtual bool        IsCorrectXDLHandle(void* xdl_handle) = 0;
    virtual uintptr_t   GetLibXDL(void* xdl_handle) = 0;
    virtual uintptr_t   GetSymAddrXDL(uintptr_t addr) = 0;
    virtual size_t      GetSymSizeXDL(uintptr_t addr) = 0;
    virtual const char* GetSymNameXDL(uintptr_t addr) = 0;
    
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
    virtual uintptr_t   PatternScan(const char* pattern, const char* soLib, const char* section_name) = 0;
    virtual void*       HookB(void* addr, void* fnAddress, void** orgFnAddress) = 0; // not all
    virtual void*       HookBL(void* addr, void* fnAddress, void** orgFnAddress) = 0;
    virtual void*       HookBLX(void* addr, void* fnAddress, void** orgFnAddress) = 0;
    virtual GAME_ID     GetGameID() = 0;
    virtual GAME_VER    GetGameVersion() = 0;
    virtual const char* GetGameName() = 0;
    virtual const char* GetGameLibName() = 0;
};

extern IAML* aml;
inline IAML* GetAMLInterface() { return aml; }

/* Do not use big conversions */
#define SET_TO(__a1, __a2) *(void**)&(__a1) = (void*)(__a2)

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
    aml->Hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a function located in PLT section (by address!) */
#define HOOKPLT(_name, _fnAddr)                                 \
    aml->HookPLT((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a function hidden behind IL2CPP */
#define HOOK_IL2CPP(_name, _methodInfo)                         \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&HookOf_##_name), (void**)(&_name))
/* Unhook a function (unsafe, actually) */
#define UNHOOK(_name, _fnAddr)                                  \
    aml->Hook((void*)(_fnAddr), (void*)(&_name), (void**)0)
/* Unhook an IL2CPP function (unsafe, actually) */
#define UNHOOK_IL2CPP(_name, _methodInfo)                       \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&_name), (void**)0)

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
    aml->Hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a function located in PLT section (by address!) and save original function pointer */
#define HOOK2PLT(_name, _fnAddr)                                \
    fnLocated##_name = (void*)_fnAddr;                          \
    aml->HookPLT((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name))
/* Just a hook of a function hidden behind IL2CPP and save original function pointer */
#define HOOK2_IL2CPP(_name, _methodInfo)                        \
    fnLocated##_name = (void*)_methodInfo->methodPointer;       \
    aml->Hook((void*)_methodInfo->methodPointer, (void*)(&HookOf_##_name), (void**)(&_name))
/* Unhook a function (unsafe, actually) that was saved before */
#define UNHOOK2(_name)                                          \
    aml->Hook(fnLocated##_name, (void*)(&_name), (void**)0)

#endif // _IAML
