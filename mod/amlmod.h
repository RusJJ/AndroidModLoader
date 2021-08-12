#ifndef _AMLMOD
#define _AMLMOD

#include "iaml.h"
#include <stdio.h>
#include <string>

#ifdef __clang__
    #define TARGET_ARM __attribute__((target("no-thumb-mode")))
    #define TARGET_THUMB  __attribute__((target("thumb-mode")))
#endif

#ifdef __GNUC__
    #define ASM_NAKED __attribute__((naked))
#else
    #define ASM_NAKED __declspec(naked)
#endif

#define MYMOD(_guid, _name, _version, _author)                          \
    static ModInfo modinfoLocal(#_guid, #_name, #_version, #_author);   \
    ModInfo* modinfo = &modinfoLocal;                                   \
    extern "C" ModInfo* __GetModInfo() { return modinfo; }              \
    IAML* aml = (IAML*)GetInterface("AMLInterface");

#define MYMODCFG(_guid, _name, _version, _author)                       \
    static ModInfo modinfoLocal(#_guid, #_name, #_version, #_author);   \
    ModInfo* modinfo = &modinfoLocal;                                   \
    extern "C" ModInfo* __GetModInfo() { return modinfo; }              \
    IAML* aml = (IAML*)GetInterface("AMLInterface");                    \
    static Config cfgLocal(#_guid);                                     \
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



struct ModInfoDependency
{
    const char* szGUID;
    const char* szVersion;
};

class ModInfo
{
public:
    ModInfo(const char* szGUID, const char* szName, const char* szVersion, const char* szAuthor)
    {
        /* No buffer overflow! */
        strncpy(this->szGUID, szGUID, sizeof(ModInfo::szGUID)); this->szGUID[sizeof(ModInfo::szGUID) - 1] = '\0';
        strncpy(this->szName, szName, sizeof(ModInfo::szName)); this->szName[sizeof(ModInfo::szName) - 1] = '\0';
        strncpy(this->szVersion, szVersion, sizeof(ModInfo::szVersion)); this->szVersion[sizeof(ModInfo::szVersion) - 1] = '\0';
        strncpy(this->szAuthor, szAuthor, sizeof(ModInfo::szAuthor)); this->szAuthor[sizeof(ModInfo::szAuthor) - 1] = '\0';

        /* GUID should be lowcase */
        for(int i = 0; this->szGUID[i] != '\0'; ++i)
        {
            this->szGUID[i] = tolower((int)(this->szGUID[i]));
        }

        /* Parse version string */
        if(sscanf(this->szVersion, "%hu.%hu.%hu.%hu", &major, &minor, &revision, &build) < 4)
        {
            if(sscanf(this->szVersion, "%hu.%hu.%hu", &major, &minor, &revision) < 3)
            {
                if(sscanf(this->szVersion, "%hu.%hu", &major, &minor) < 2)
                {
                    major = (unsigned short)atoi(this->szVersion);
                }
                revision = 0;
            }
            build = 0;
        }
    }
    inline const char* GUID() { return szGUID; }
    inline const char* Name() { return szName; }
    inline const char* VersionString() { return szVersion; }
    inline const char* Author() { return szAuthor; }
    inline unsigned short Major() { return major; }
    inline unsigned short Minor() { return minor; }
    inline unsigned short Revision() { return revision; }
    inline unsigned short Build() { return build; }
    inline uintptr_t Handle() { return handle; }
private:
    char szGUID[48];
    char szName[48];
    char szVersion[24];
    char szAuthor[48];
    unsigned short major;
    unsigned short minor;
    unsigned short revision;
    unsigned short build;
    uintptr_t handle;
    ModInfoDependency* dependencies;

    friend class ModsList;
};

typedef ModInfo* (*GetModInfoFn)();

#endif // _AMLMOD