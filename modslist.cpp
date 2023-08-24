#include <modslist.h>
#include <modpaks.h>
#include <mod/logger.h>
#include <dlfcn.h>

bool ModsList::AddMod(ModInfo* modinfo, void* modhandle, const char* path)
{
    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    while( it != end )
    {
        if(!strcmp((*it)->m_pInfo->szGUID, modinfo->szGUID)) return false;
        ++it;
    }

    modinfo->handle = modhandle;
    modinfo->dependencies = NULL;
    if(modhandle != NULL)
    {
        GetDependenciesListFn getDepList = (GetDependenciesListFn)dlsym(modhandle, "__GetDepsList");
        if(getDepList != NULL)
            modinfo->dependencies = getDepList();
    }
    
    ModDesc* d = new ModDesc();
    d->m_pInfo = modinfo;
    d->m_pHandle = modhandle;
    snprintf(d->m_szLibPath, 256, "%s", path);
    m_vecModInfo.push_back(d);
    return true;
}

bool ModsList::RemoveMod(ModInfo* modinfo)
{
    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    ModInfo* found = NULL;
    while( it != end )
    {
        found = (*it)->m_pInfo;
        if(found == modinfo)
        {
            dlclose(found->handle);
            delete found;
            m_vecModInfo.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}

bool ModsList::RemoveMod(const char* szGUID)
{
    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    ModInfo* found = NULL;
    while( it != end )
    {
        found = (*it)->m_pInfo;
        if(!strcmp(found->szGUID, szGUID))
        {
            dlclose(found->handle);
            delete found;
            m_vecModInfo.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}

bool ModsList::HasMod(const char* szGUID)
{
    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    while( it != end )
    {
        if(!strcmp((*it)->m_pInfo->szGUID, szGUID))
        {
            return true;
        }
        ++it;
    }
    return false;
}

bool ModsList::HasModOfVersion(const char* szGUID, const char* szVersion)
{
    if(szVersion[0] == '\0') return HasMod(szGUID);
    unsigned short major, minor, revision, build;
    if(sscanf(szVersion, "%hu.%hu.%hu.%hu", &major, &minor, &revision, &build) < 4)
    {
        if(sscanf(szVersion, "%hu.%hu.%hu", &major, &minor, &revision) < 3)
        {
            if(sscanf(szVersion, "%hu.%hu", &major, &minor) < 2)
            {
                major = (unsigned short)atoi(szVersion);
            }
            revision = 0;
        }
        build = 0;
    }

    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    ModInfo* pInfo = NULL;
    while( it != end )
    {
        pInfo = (*it)->m_pInfo;
        if(!strcmp(pInfo->szGUID, szGUID))
        {
            if(pInfo->version.major > major) return true;
            if(pInfo->version.major == major)
            {
                if(pInfo->version.minor > minor) return true;
                if(pInfo->version.minor == minor)
                {
                    if(pInfo->version.revision > revision) return true;
                    if(pInfo->version.revision == revision && pInfo->version.build >= build) return true;
                }
            }
            return false;
        }
        ++it;
    }
    return false;
}

bool ModsList::HasModOfBiggerVersion(const char* szGUID, const char* szVersion)
{
    if(szVersion[0] == '\0') return HasMod(szGUID);
    unsigned short major, minor, revision, build;
    if(sscanf(szVersion, "%hu.%hu.%hu.%hu", &major, &minor, &revision, &build) < 4)
    {
        if(sscanf(szVersion, "%hu.%hu.%hu", &major, &minor, &revision) < 3)
        {
            if(sscanf(szVersion, "%hu.%hu", &major, &minor) < 2)
            {
                major = (unsigned short)atoi(szVersion);
            }
            revision = 0;
        }
        build = 0;
    }

    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    ModInfo* pInfo = NULL;
    while( it != end )
    {
        pInfo = (*it)->m_pInfo;
        if(!strcmp(pInfo->szGUID, szGUID))
        {
            if(pInfo->version.major > major) return true;
            if(pInfo->version.major == major)
            {
                if(pInfo->version.minor > minor) return true;
                if(pInfo->version.minor == minor)
                {
                    if(pInfo->version.revision > revision) return true;
                    if(pInfo->version.revision == revision && pInfo->version.build > build) return true;
                }
            }
            return false;
        }
        ++it;
    }
    return false;
}

void ModsList::ProcessDependencies()
{
    bool bRepeatDependencies = true;
    int i;
    ModInfoDependency* depList = NULL;
    ModInfo* pInfo = NULL;

    while( bRepeatDependencies && m_vecModInfo.size() >= 1 )
    {
        bRepeatDependencies = false;
        auto it = modlist->m_vecModInfo.begin();
        auto end = modlist->m_vecModInfo.end();
        while( it != end )
        {
            pInfo = (*it)->m_pInfo;
            if(pInfo->dependencies != NULL)
            {
                depList = pInfo->dependencies;
                for(i = 0; depList[i].szGUID[0] != '\0'; ++i)
                {
                    if(!HasModOfVersion(depList[i].szGUID, depList[i].szVersion))
                    {
                        logger->Error("Mod (GUID %s) requires a mod %s %s", pInfo->szGUID, depList[i].szGUID, depList[i].szVersion);
                        RemoveMod(pInfo);

                        bRepeatDependencies = true;
                        break;
                    }
                }
                if( bRepeatDependencies ) break;
                pInfo->dependencies = NULL; // Dont check that mod again if everything is ok...
            }
            ++it;
        }
    }
}

void ModsList::ProcessPreLoading()
{
    OnModLoadFn onModPreLoadFn;
    auto end = modlist->m_vecModInfo.end();
    ModDesc* desc = NULL;
    for( auto it = modlist->m_vecModInfo.begin(); it != end; ++it )
    {
        desc = *it;
        void* handle = desc->m_pHandle;
        if(handle != 0)
        {
            onModPreLoadFn = (OnModLoadFn)dlsym(handle, "OnModPreLoad");
            //if(onModPreLoadFn == NULL) onModPreLoadFn = (OnModLoadFn)dlsym(handle, "_Z12OnModPreLoadv");
            if(onModPreLoadFn != NULL) onModPreLoadFn();

            desc->m_fnOnModLoaded = (OnModLoadFn)dlsym(handle, "OnModLoad");
            //if(desc->m_fnOnModLoaded == NULL) desc->m_fnOnModLoaded = (OnModLoadFn)dlsym(handle, "_Z9OnModLoadv");

            desc->m_fnOnModUnloaded = (OnModLoadFn)dlsym(handle, "OnModUnload");
            //if(desc->m_fnOnModUnloaded == NULL) desc->m_fnOnModUnloaded = (OnModLoadFn)dlsym(handle, "_Z11OnModUnloadv");

            desc->m_fnRequestUpdaterURL = (GetUpdaterURLFn)dlsym(handle, "OnUpdaterURLRequested");
            //if(desc->m_fnRequestUpdaterURL == NULL) desc->m_fnRequestUpdaterURL = (GetUpdaterURLFn)dlsym(handle, "_Z21OnUpdaterURLRequestedv");

            desc->m_fnInterfaceAddedCB = (OnInterfaceAddedFn)dlsym(handle, "OnInterfaceAdded");
            //if(desc->m_fnInterfaceAddedCB == NULL) desc->m_fnInterfaceAddedCB = (OnInterfaceAddedFn)dlsym(handle, "_Z16OnInterfaceAddedPKcPKv");

            desc->m_fnOnAllModsLoaded = (OnModLoadFn)dlsym(handle, "OnAllModsLoaded");
            //if(desc->m_fnOnAllModsLoaded == NULL) desc->m_fnOnAllModsLoaded = (OnModLoadFn)dlsym(handle, "_Z15OnAllModsLoadedv");

            desc->m_fnGameCrashedCB = (OnGameCrashedFn)dlsym(handle, "OnGameCrash");
            //if(desc->m_fnGameCrashedCB == NULL) desc->m_fnGameCrashedCB = (OnGameCrashedFn)dlsym(handle, "_Z11OnGameCrashv");
        }
    }
    logger->Info("Mods were preloaded!");
}
void ModsList::ProcessLoading()
{
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    ModDesc* desc = NULL;
    while( it != end )
    {
        desc = *it;
        if(desc->m_fnOnModLoaded != NULL) desc->m_fnOnModLoaded();
        ++it;
    }
    logger->Info("Mods were loaded!");
}
void ModsList::ProcessUnloading()
{
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    ModDesc* desc = NULL;
    while( it != end )
    {
        desc = *it;
        if(desc->m_fnOnModUnloaded != NULL) desc->m_fnOnModUnloaded();
        ++it;
    }
}
void ModsList::ProcessUpdater()
{
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    ModDesc* desc = NULL;
    while( it != end )
    {
        desc = *it;
        if(desc->m_fnRequestUpdaterURL != NULL)
        {
            const char* url = desc->m_fnRequestUpdaterURL();
            CURLcode res = DownloadFileToData(url);
            if(res != CURLE_OK)
            {
                logger->Error("Updater failed to determine an update info for %s, err %d", desc->m_pInfo->GUID(), res);
            }
            else
            {
                ProcessData(*it);
            }
        }
        ++it;
    }
}
void ModsList::ProcessCrash(const char* szLibName, int sig, int code, uintptr_t libaddr, mcontext_t* mcontext)
{
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    ModDesc* desc = NULL;
    while( it != end )
    {
        desc = *it;
        if(desc->m_fnGameCrashedCB != NULL) desc->m_fnGameCrashedCB(szLibName, sig, code, libaddr, mcontext);
        ++it;
    }
}
void ModsList::OnInterfaceAdded(const char* name, const void* ptr)
{
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    ModDesc* desc = NULL;
    while( it != end )
    {
        desc = *it;
        if(desc->m_fnInterfaceAddedCB != NULL) desc->m_fnInterfaceAddedCB(name, ptr);
        ++it;
    }
}

void ModsList::OnAllModsLoaded()
{
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    ModDesc* desc = NULL;
    while( it != end )
    {
        desc = *it;
        if(desc->m_fnOnAllModsLoaded != NULL) desc->m_fnOnAllModsLoaded();
        ++it;
    }
    logger->Info("Mods were postloaded!");
}

static ModsList modlistLocal;
ModsList* modlist = &modlistLocal;
