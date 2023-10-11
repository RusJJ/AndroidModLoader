#include <modslist.h>
#include <modpaks.h>
#include <mod/logger.h>
#include <mod/listitem.h>
#include <dlfcn.h>


Mods* listMods = NULL;
LIST_START(Mods)

    LIST_INITSTART(Mods)
        pModInfo = NULL;
        pModDesc = NULL;
        pHandle = NULL;
    LIST_INITEND()

    static void AddNew(ModInfo* info, void* libhandle, const char* path)
    {
        Mods* newItem = new Mods;
        newItem->pModInfo = info;
        newItem->pHandle = libhandle;

        ModDesc* d = new ModDesc();
        d->m_pInfo = info;
        d->m_pHandle = libhandle;
        d->m_aDependencies = NULL;
        if(path) snprintf(d->m_szLibPath, 256, "%s", path);
        else d->m_szLibPath[0] = 0;
        if(libhandle != NULL)
        {
            GetDependenciesListFn getDepList = (GetDependenciesListFn)dlsym(libhandle, "__GetDepsList");
            if(getDepList != NULL) d->m_aDependencies = getDepList();
        }
        newItem->pModDesc = d;
        
        newItem->Push(&listMods);
    }
    static Mods* Get(const char* guid)
    {
        LIST_FOR(listMods)
        {
            if (!strcmp(item->pModInfo->szGUID, guid)) return item;
        }
        return NULL;
    }

    ModInfo* pModInfo;
    ModDesc* pModDesc;
    void* pHandle;
LIST_END()

bool ModsList::AddMod(ModInfo* modinfo, void* modhandle, const char* path)
{
    if(Mods::Get(modinfo->szGUID) != NULL) return false;
    Mods::AddNew(modinfo, modhandle, path);
    return true;
}

bool ModsList::RemoveMod(ModInfo* modinfo)
{
    LIST_FOR(listMods)
    {
        if(item->pModInfo == modinfo)
        {
            dlclose(item->pHandle);
            if(item->Remove(&listMods))
            {
                delete item->pModDesc;
                delete item;
            }
            return true;
        }
    }
    return false;
}

bool ModsList::RemoveMod(const char* szGUID)
{
    LIST_FOR(listMods)
    {
        if(!strcmp(item->pModInfo->szGUID, szGUID))
        {
            dlclose(item->pHandle);
            if(item->Remove(&listMods))
            {
                delete item->pModDesc;
                delete item;
            }
            return true;
        }
    }
    return false;
}

bool ModsList::HasMod(const char* szGUID)
{
    LIST_FOR(listMods)
    {
        if(!strcmp(item->pModInfo->szGUID, szGUID))
        {
            return true;
        }
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

    ModInfo* pInfo = NULL;
    LIST_FOR(listMods)
    {
        pInfo = item->pModInfo;
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

    ModInfo* pInfo = NULL;
    LIST_FOR(listMods)
    {
        pInfo = item->pModInfo;
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
    }
    return false;
}

void ModsList::ProcessDependencies()
{
    ModInfoDependency* depList;
    ModInfo* info;

  label_run_dependencies_check:
    //logger->Info("Checking dependencies from the start! Mods count: %d", modlist->GetModsNum());
    LIST_FOR(listMods)
    {
        // If the mod is already ok or doesnt require check, depList = NULL
        depList = item->pModDesc->m_aDependencies;
        if(depList)
        {
            info = item->pModInfo;
            for(int i = 0; depList[i].szGUID && depList[i].szGUID[0] != 0; ++i)
            {
                if(depList[i].szVersion)
                {
                    if(!HasModOfVersion(depList[i].szGUID, depList[i].szVersion))
                    {
                        logger->Error("Mod (GUID %s) requires a mod %s of version %s+", info->szGUID, depList[i].szGUID, depList[i].szVersion);
                        ModsList::RemoveMod(info);
                        goto label_run_dependencies_check;
                    }
                }
                else
                {
                    if(!HasMod(depList[i].szGUID))
                    {
                        logger->Error("Mod (GUID %s) requires a mod %s of any veesion", info->szGUID, depList[i].szGUID);
                        ModsList::RemoveMod(info);
                        goto label_run_dependencies_check;
                    }
                }
            }

            // Everything is okay, we dont need to check it again!
            item->pModDesc->m_aDependencies = NULL;
        }
    }
}

void ModsList::ProcessPreLoading()
{
    OnModLoadFn onModPreLoadFn;
    ModDesc* desc;
    void* handle;
    LIST_FOR(listMods)
    {
        handle = item->pHandle;
        if(handle != NULL)
        {
            desc = item->pModDesc;

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
    ModDesc* desc = NULL;
    LIST_FOR(listMods)
    {
        desc = item->pModDesc;
        if(desc->m_fnOnModLoaded) desc->m_fnOnModLoaded();
    }
    logger->Info("Mods were loaded!");
}

void ModsList::ProcessUnloading()
{
    ModDesc* desc = NULL;
    LIST_FOR(listMods)
    {
        desc = item->pModDesc;
        if(desc->m_fnOnModUnloaded) desc->m_fnOnModUnloaded();
    }
}

void ModsList::ProcessUpdater()
{
    ModDesc* desc = NULL;
    LIST_FOR(listMods)
    {
        desc = item->pModDesc;
        if(desc->m_fnRequestUpdaterURL)
        {
            const char* url = desc->m_fnRequestUpdaterURL();
            CURLcode res = DownloadFileToData(url);
            if(res != CURLE_OK)
            {
                logger->Error("Updater failed to determine an update info for %s, err %d", desc->m_pInfo->GUID(), res);
            }
            else
            {
                ProcessData(desc);
            }
        }
    }
}

void ModsList::ProcessCrash(const char* szLibName, int sig, int code, uintptr_t libaddr, mcontext_t* mcontext)
{
    ModDesc* desc = NULL;
    LIST_FOR(listMods)
    {
        desc = item->pModDesc;
        if(desc->m_fnGameCrashedCB) desc->m_fnGameCrashedCB(szLibName, sig, code, libaddr, mcontext);
    }
}

int ModsList::GetModsNum()
{
    return listMods->Count();
}

void ModsList::PrintModsList(std::ofstream& logfile)
{
    logfile << "List of loaded mods (count=" << listMods->Count() << "):\n";

    ModInfo* info = NULL;
    ModDesc* desc = NULL;
    LIST_FOR(listMods)
    {
        info = item->pModInfo;
        desc = item->pModDesc;
        
        logfile << info->Name() << " (" << info->Author() << ", version " << info->VersionString() << ")\n";
        logfile << " - Loaded from: " << desc->m_szLibPath << "\n";
    }
}

void ModsList::OnInterfaceAdded(const char* name, const void* ptr)
{
    ModDesc* desc = NULL;
    LIST_FOR(listMods)
    {
        desc = item->pModDesc;
        if(desc->m_fnInterfaceAddedCB) desc->m_fnInterfaceAddedCB(name, ptr);
    }
}

void ModsList::OnAllModsLoaded()
{
    ModDesc* desc = NULL;
    LIST_FOR(listMods)
    {
        desc = item->pModDesc;
        if(desc->m_fnOnAllModsLoaded) desc->m_fnOnAllModsLoaded();
    }
    logger->Info("Mods were postloaded!");
}

static ModsList modlistLocal;
ModsList* modlist = &modlistLocal;
