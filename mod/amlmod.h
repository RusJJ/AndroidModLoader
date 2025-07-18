#ifndef _AMLMOD
#define _AMLMOD

#include <stdio.h>
#include <ctype.h>
#include <cstring>
#include <stdlib.h>
#include <signal.h>

#ifdef __arm__
    #define AML32
    #define BYBIT(__32val, __64val) (__32val)
#elif defined __aarch64__
    #define AML64
    #define BYBIT(__32val, __64val) (__64val)
#else
    #error This lib is supposed to work on ARM only!
#endif

#ifdef __clang__
    #define TARGET_ARM __attribute__((target("no-thumb-mode")))
    #define TARGET_THUMB  __attribute__((target("thumb-mode")))
#endif

#ifdef __GNUC__
    #define ASM_NAKED __attribute__((naked))
    #define EXPORT __attribute__((visibility("default")))
#else
    #define ASM_NAKED __declspec(naked)
    #define EXPORT
#endif

#define MYMOD(_guid, _name, _version, _author)                          \
    static ModInfo modinfoLocal(#_guid, #_name, #_version, #_author);   \
    ModInfo* modinfo = &modinfoLocal;                                   \
    extern "C" ModInfo* __GetModInfo() { return modinfo; }              \
    IAML* aml = (IAML*)GetInterface("AMLInterface");

#define MYMODCFG(_guid, _name, _version, _author)                       \
    MYMOD(_guid, _name, _version, _author);                             \
    static Config cfgLocal(#_guid);                                     \
    Config* cfg = &cfgLocal;

#define MYMODCFGNAME(_guid, _name, _version, _author, _cfgname)         \
    MYMOD(_guid, _name, _version, _author);                             \
    static Config cfgLocal(#_cfgname);                                  \
    Config* cfg = &cfgLocal;

#define NEEDGAME(_pkg_name)                                             \
    extern "C" const char* __INeedASpecificGame() {return #_pkg_name;}

#define MYMODDECL()                                                     \
    extern ModInfo* modinfo; // Just in case if you need to use that somewhere else in your mod

/* Dependencies! */
#define BEGIN_DEPLIST()                                                 \
    static ModInfoDependency g_listDependencies[] = {

#define ADD_DEPENDENCY(_guid)                                           \
    {#_guid, ""},

#define ADD_DEPENDENCY_VER(_guid, _version)                             \
    {#_guid, #_version},

#define END_DEPLIST()                                                   \
    {"", ""} };                                                         \
    extern "C" ModInfoDependency* __GetDepsList() { return &g_listDependencies[0]; }

/* Macros to stop forgetting stuff! */
#define ON_MOD_PRELOAD()                                                \
    extern "C" void OnModPreLoad()

#define ON_MOD_LOAD()                                                   \
    extern "C" void OnModLoad()

#define ON_ALL_MODS_LOAD()                                              \
    extern "C" void OnAllModsLoaded()

#define ON_MOD_UNLOAD()                                                 \
    extern "C" void OnModUnload() /*Not guaranteed*/

#define ON_GAME_CRASH()                                                 \
    extern "C" void OnGameCrash(const char* library, int sig, int code, uintptr_t libaddr, mcontext_t* mcontext) /*Not guaranteed*/

#define UPDATER_URL()                                                   \
    extern "C" const char* OnUpdaterURLRequested()

#define ON_NEW_INTERFACE()                                              \
    extern "C" void OnInterfaceAdded(const char* name, const void* ptr)

    

#define MINIMUM_MD5_BUF_SIZE ( 32 + 1 )

struct MemChunk_t
{
    char* out;
    size_t out_len;
};
    
struct ModInfoDependency
{
    const char* szGUID;
    const char* szVersion;
};

struct ModVersion
{
    unsigned short major;
    unsigned short minor;
    unsigned short revision;
    unsigned short build;
};

// Should be faster than strncpy?
inline char *strxcpy(char* __restrict__ dst, const char* __restrict__ src, int len)
{
    if (!len) return NULL;
    while (--len && (*dst++ = *src++));
    if (!len)
    {
        *dst++ = '\0';
        return *src ? NULL : dst;
    }
    else
    {
        return dst;
    }
}

inline int clampint(int min, int max, int v)
{
    if(v < min) return min;
    else if(v > max) return max;
    return v;
}
inline void clampint(int min, int max, int* v)
{
    if(*v < min) *v = min;
    else if(*v > max) *v = max;
}
inline float clampfloat(float min, float max, float v)
{
    if(v < min) return min;
    else if(v > max) return max;
    return v;
}
inline void clampfloat(float min, float max, float* v)
{
    if(*v < min) *v = min;
    else if(*v > max) *v = max;
}

class ModInfo
{
public:
    ModInfo(const char* szGUID, const char* szName, const char* szVersion, const char* szAuthor)
    {
        /* No buffer overflow! */
        strxcpy(this->szGUID, szGUID, sizeof(ModInfo::szGUID)); this->szGUID[sizeof(ModInfo::szGUID) - 1] = '\0';
        strxcpy(this->szName, szName, sizeof(ModInfo::szName)); this->szName[sizeof(ModInfo::szName) - 1] = '\0';
        strxcpy(this->szVersion, szVersion, sizeof(ModInfo::szVersion)); this->szVersion[sizeof(ModInfo::szVersion) - 1] = '\0';
        strxcpy(this->szAuthor, szAuthor, sizeof(ModInfo::szAuthor)); this->szAuthor[sizeof(ModInfo::szAuthor) - 1] = '\0';

        /* GUID should be lowcase */
        for(int i = 0; this->szGUID[i] != '\0'; ++i)
        {
            this->szGUID[i] = tolower((int)(this->szGUID[i]));
        }

        /* Parse version string */
        if(sscanf(this->szVersion, "%hu.%hu.%hu.%hu", &version.major, &version.minor, &version.revision, &version.build) < 4)
        {
            if(sscanf(this->szVersion, "%hu.%hu.%hu", &version.major, &version.minor, &version.revision) < 3)
            {
                if(sscanf(this->szVersion, "%hu.%hu", &version.major, &version.minor) < 2)
                {
                    version.major = (unsigned short)atoi(this->szVersion);
                }
                version.revision = 0;
            }
            version.build = 0;
        }
    }
    inline const char* GUID() { return szGUID; }
    inline const char* Name() { return szName; }
    inline const char* VersionString() { return szVersion; }
    inline const char* Author() { return szAuthor; }
    inline unsigned short Major() { return version.major; }
    inline unsigned short Minor() { return version.minor; }
    inline unsigned short Revision() { return version.revision; }
    inline unsigned short Build() { return version.build; }

private:
    char szGUID[48];
    char szName[48];
    char szVersion[24];
    char szAuthor[48];
    ModVersion version;

    friend class ModsList;
    friend class Mods;
};

typedef ModInfo* (*GetModInfoFn)();



#include "iaml.h"

#endif // _AMLMOD