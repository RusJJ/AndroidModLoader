#include <modslist.h>
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

    modinfo->dependencies = nullptr;
    if(modhandle != nullptr)
    {
        GetDependenciesListFn getDepList = (GetDependenciesListFn)dlsym(modhandle, "__GetDepsList");
        if(getDepList != nullptr)
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
    ModInfo* pInfo = nullptr;
    while( it != end )
    {
        pInfo = *it;
        if(!strcmp(pInfo->szGUID, szGUID))
        {
            if(pInfo->major > major) return true;
            if(pInfo->major == major)
            {
                if(pInfo->minor > minor) return true;
                if(pInfo->minor == minor)
                {
                    if(pInfo->revision > revision) return true;
                    if(pInfo->revision == revision && pInfo->build >= build) return true;
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
    ModInfoDependency* depList = nullptr;
    ModInfo* pInfo = nullptr;
    int i;

    while( bRepeatDependencies && m_vecModInfo.size() > 1 )
    {
        bRepeatDependencies = false;
        auto it = modlist->m_vecModInfo.begin();
        auto end = modlist->m_vecModInfo.end();
        while( it != end )
        {
            //logger->Info("Mods count: %d", m_vecModInfo.size());
            pInfo = *it;
            if(pInfo->dependencies != nullptr)
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
                pInfo->dependencies = nullptr; // Dont check that mod again if everything is ok...
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
            if(onModLoadFn != nullptr) onModLoadFn();
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
            if(onModLoadFn != nullptr) onModLoadFn();
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
            if(onModLoadFn != nullptr) onModLoadFn();
        }
        ++it;
    }
}

static ModsList modlistLocal;
ModsList* modlist = &modlistLocal;