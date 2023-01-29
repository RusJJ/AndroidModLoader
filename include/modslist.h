#ifndef __MODSLIST_H
#define __MODSLIST_H

#include <vector>
#include <mod/amlmod.h>

class ModsList
{
// Functions
public:
    bool AddMod(ModInfo* modinfo, void* modhandle);
    bool RemoveMod(ModInfo* modinfo);
    bool RemoveMod(const char* szGUID);
    bool HasMod(const char* szGUID);
    bool HasModOfVersion(const char* szGUID, const char* szVersion);
    void ProcessDependencies();
    void ProcessPreLoading();
    void ProcessLoading();
    void ProcessUnloading();
    inline int GetModsNum() { return m_vecModInfo.size(); }

// Callbacks
public:
    void OnInterfaceAdded(const char* name, const void* ptr);
    void OnAllModsLoaded();

private:
    std::vector<ModInfo*> m_vecModInfo;
};

extern ModsList* modlist;

#endif // __MODSLIST_H
