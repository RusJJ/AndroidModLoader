#ifndef __MODSLIST_H
#define __MODSLIST_H

#include <fstream>
#include <mod/amlmod.h>

class Mods;
extern Mods* listMods;

typedef void (*OnInterfaceAddedFn)(const char*, const void*);
typedef const char* (*GetUpdaterURLFn)();
typedef void (*OnModLoadFn)();
typedef ModInfoDependency* (*GetDependenciesListFn)();
typedef void (*OnGameCrashedFn)(const char*, int, int, uintptr_t, mcontext_t*);
struct ModDesc
{
    ModInfo*            m_pInfo;
    void*               m_pHandle;
    ModInfoDependency*  m_aDependencies;
    char                m_szLibPath[256];

    OnModLoadFn         m_fnOnModLoaded;
    OnModLoadFn         m_fnOnModUnloaded;
    OnModLoadFn         m_fnOnAllModsLoaded;
    GetUpdaterURLFn     m_fnRequestUpdaterURL;
    OnInterfaceAddedFn  m_fnInterfaceAddedCB;
    OnGameCrashedFn     m_fnGameCrashedCB;

    ModDesc()
    {
        m_szLibPath[0] = 0;
        m_fnOnModLoaded = NULL;
        m_fnOnModUnloaded = NULL;
        m_fnOnAllModsLoaded = NULL;
        m_fnRequestUpdaterURL = NULL;
        m_fnInterfaceAddedCB = NULL;
        m_fnGameCrashedCB = NULL;
    }
};

class ModsList
{
// Functions
public:
    bool AddMod(ModInfo* modinfo, void* modhandle, const char* path);
    bool RemoveMod(ModInfo* modinfo);
    bool RemoveMod(const char* szGUID);
    bool HasMod(const char* szGUID);
    bool HasModOfVersion(const char* szGUID, const char* szVersion);
    bool HasModOfBiggerVersion(const char* szGUID, const char* szVersion);
    void ProcessDependencies();
    void ProcessPreLoading();
    void ProcessLoading();
    void ProcessUnloading();
    void ProcessUpdater();
    void ProcessCrash(const char* szLibName, int sig, int code, uintptr_t libaddr, mcontext_t* mcontext);
    int  GetModsNum();
    void PrintModsList(std::ofstream& logfile);

// Callbacks
public:
    void OnInterfaceAdded(const char* name, const void* ptr);
    void OnAllModsLoaded();
};

extern ModsList* modlist;

#endif // __MODSLIST_H
