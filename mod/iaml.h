#ifndef _IAML
#define _IAML

// Usage: place 3 lines somewhere in the code AFTER #include <mod/amlmod.h>
// #if !defined(IAML_VER) && IAML_VER < 01030000
//     #error "You need to update your MOD folder to 1.3.0!"
// #endif
#define IAML_VER 01030000

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

// AML 1.3.0 stuff (Vibration patterns, examples)
static jlong DEFAULT_VIBRATE_PATTERN[4] = {0, 250, 250, 250};
static jlong g_VibroPattern_Weak[7] = { 0, 20, 80, 20, 80, 20, 80 };
static jlong g_VibroPattern_Alert[6] = { 0, 200, 100, 200, 100, 400 };

// I`m redoing this because i dont want to include additional file
// Thanks @XMDS, maybe someone will use it
struct GlossRegisters
{
#ifdef AML32
    enum e_reg
    {
        R0 = 0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, FP = R11, R12, IP = R12, R13, SP = R13, R14, LR = R14, R15, PC = R15, CPSR
    };

    union
    {
        uint32_t reg[17];
        struct
        {
            uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr, pc, cpsr;
        } regs;
    };
#else
    enum e_reg
    {
        X0 = 0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, FP = X29,
        Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15, Q16, Q17, Q18, Q19, Q20, Q21, Q22, Q23, Q24, Q25, Q26, Q27, Q28, Q29, Q30, Q31,
        X30, LR = X30, X31, SP = X31, PC, CPSR
    };

    union
    {
        uint64_t reg[66];
        struct
        {
            uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;
            double q0, q1, q2, q3, q4, q5, q6, q7, q8, q9, q10, q11, q12, q13, q14, q15, q16, q17, q18, q19, q20, q21, q22, q23, q24, q25, q26, q27, q28, q29, q30, q31;
            uint64_t lr, sp, pc, cpsr;
        } regs;
    };
#endif
};
typedef void* PHookHandle;
typedef void (*HookWithRegistersFn)(GlossRegisters* regs, PHookHandle hook);

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
    
    /* AML 1.2.2 */
    virtual int         GetAndroidVersion();
    virtual bool        CopyFile(const char* file, const char* dest);
    // Gloss things
  #ifdef AML32
    virtual void        RedirectReg(...);
  #else
    virtual void        RedirectReg(uintptr_t addr, uintptr_t to, bool doShortHook = false, GlossRegisters::e_reg targetReg = GlossRegisters::e_reg::X16); // Move directly to "to" from "addr" with the same stack and registers (X16 is the same as "Redirect")
  #endif  
    virtual bool        HasAddrExecFlag(uintptr_t addr);
    virtual void        ToggleHook(PHookHandle hook, bool enable);
    virtual void        DeHook(PHookHandle hook);
    virtual PHookHandle HookInline(void* fnAddress, HookWithRegistersFn newFn, bool doShortHook = false);
    
    /* AML 1.2.3 */
    virtual bool        HasFastmanAPKModified();
    virtual const char* GetInternalPath(); // /sdcard/
    virtual const char* GetInternalModsPath(); // /sdcard/AMLMods/*game*/ (by default)
    
    /* AML 1.3.0 */
    virtual JavaVM*     GetJavaVM();
    virtual jobject     GetCurrentContext();
    virtual void        DoVibro(int msTime); // Pretty strong feedback... If you need a small vibro, do it for like ~20ms, it's gonna be enough
    virtual void        DoVibro(jlong* pattern, int patternItems); // Patterns might give you more control
    virtual void        CancelVibro();
    virtual float       GetBatteryLevel(); // returns a float from 0.0 to 100.0


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
#define SET_TO_PTR(__a1, __a2)  *(void**)&(__a1) = *(void**)(__a2)
#define SETSYM_TO(__a1, __a2, __a3)  *(void**)&(__a1) = (void*)(aml->GetSym(__a2, __a3))
#define SETSYM_TO_PTR(__a1, __a2, __a3)  *(void**)&(__a1) = *(void**)(aml->GetSym(__a2, __a3))
#define AS_ADDR(__a1)       *(uintptr_t*)&(__a1)

/* Unprotect that memory chunk for making changes */
#define UNPROT(_addr, _count)                                   \
    aml->Unprot((uintptr_t)(_addr), ( _count ));
/* Just write own info to the memory */
#define WRITE(_addr, _whatToWrite, _size)                       \
    aml->Write(_addr, _whatToWrite, _size);

/* Just a hook declaration */
#define DECL_HOOK(_ret, _name, ...)                             \
    _ret (*_name)(__VA_ARGS__);                                    \
    _ret HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = void */
#define DECL_HOOKv(_name, ...)                                  \
    void (*_name)(__VA_ARGS__);                                    \
    void HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = bool */
#define DECL_HOOKb(_name, ...)                                  \
    bool (*_name)(__VA_ARGS__);                                    \
    bool HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = int */
#define DECL_HOOKi(_name, ...)                                  \
    int (*_name)(__VA_ARGS__);                                    \
    int HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = void* */
#define DECL_HOOKp(_name, ...)                                  \
    void* (*_name)(__VA_ARGS__);                                \
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