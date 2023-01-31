#include <include/modslist.h>
#include <mod/logger.h>
#include <dlfcn.h>

typedef ModInfoDependency* (*GetDependenciesListFn)();
bool ModsList::AddMod(ModInfo* modinfo, void* modhandle)
{
    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    while( it != end )
    {
        if(!strcmp((*it)->szGUID, modinfo->szGUID)) return false;
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
    
    m_vecModInfo.push_back(modinfo);
    return true;
}

bool ModsList::RemoveMod(ModInfo* modinfo)
{
    auto it = m_vecModInfo.begin();
    auto end = m_vecModInfo.end();
    while( it != end )
    {
        if(*it == modinfo)
        {
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
    while( it != end )
    {
        if(!strcmp((*it)->szGUID, szGUID))
        {
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
        if(!strcmp((*it)->szGUID, szGUID))
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
        pInfo = *it;
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
            pInfo = *it;
            if(pInfo->dependencies != NULL)
            {
                depList = pInfo->dependencies;
                for(i = 0; depList[i].szGUID[0] != '\0'; ++i)
                {
                    if(!HasModOfVersion(depList[i].szGUID, depList[i].szVersion))
                    {
                        logger->Error("Mod (GUID %s) requires a mod %s %s", pInfo->szGUID, depList[i].szGUID, depList[i].szVersion);
                        dlclose((void*)(pInfo->handle));
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

typedef void (*OnModLoadFn)();
void ModsList::ProcessPreLoading()
{
    OnModLoadFn onModLoadFn;
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    while( it != end )
    {
        if((*it)->handle != 0)
        {
            onModLoadFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "OnModPreLoad");
            if(onModLoadFn == NULL) onModLoadFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "_Z12OnModPreLoadv");
            if(onModLoadFn != NULL) onModLoadFn();
        }
        ++it;
    }
}
void ModsList::ProcessLoading()
{
    OnModLoadFn onModLoadFn;
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    while( it != end )
    {
        if((*it)->handle != 0)
        {
            onModLoadFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "OnModLoad");
            if(onModLoadFn == NULL) onModLoadFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "_Z9OnModLoadv");
            if(onModLoadFn != NULL) onModLoadFn();
        }
        ++it;
    }
}
void ModsList::ProcessUnloading()
{
    OnModLoadFn onModLoadFn;
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    while( it != end )
    {
        if((*it)->handle != 0)
        {
            onModLoadFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "OnModUnload");
            if(onModLoadFn == NULL) onModLoadFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "_Z11OnModUnloadv");
            if(onModLoadFn != NULL) onModLoadFn();
        }
        ++it;
    }
}
typedef void (*OnInterfaceAddedFn)(const char*, const void*);
void ModsList::OnInterfaceAdded(const char* name, const void* ptr)
{
    OnInterfaceAddedFn onInterfaceAddedFn;
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    while( it != end )
    {
        if((*it)->handle != 0)
        {
            onInterfaceAddedFn = (OnInterfaceAddedFn)dlsym((void*)((*it)->handle), "OnInterfaceAdded");
            if(onInterfaceAddedFn == NULL) onInterfaceAddedFn = (OnInterfaceAddedFn)dlsym((void*)((*it)->handle), "_Z16OnInterfaceAddedPKcPKv");
            if(onInterfaceAddedFn != NULL) onInterfaceAddedFn(name, ptr);
        }
        ++it;
    }
}

void ModsList::OnAllModsLoaded()
{
    OnModLoadFn onModsLoadedFn;
    auto it = modlist->m_vecModInfo.begin();
    auto end = modlist->m_vecModInfo.end();
    while( it != end )
    {
        if((*it)->handle != 0)
        {
            onModsLoadedFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "OnAllModsLoaded");
            if(onModsLoadedFn == NULL) onModsLoadedFn = (OnModLoadFn)dlsym((void*)((*it)->handle), "_Z15OnAllModsLoadedv");
            if(onModsLoadedFn != NULL) onModsLoadedFn();
        }
        ++it;
    }
}

static ModsList modlistLocal;
ModsList* modlist = &modlistLocal;
