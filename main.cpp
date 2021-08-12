#include <jnifn.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <filesystem>
namespace fs = std::filesystem;

#include <defines.h>
#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

#ifdef __IL2CPPUTILS
    #include <il2cpp/functions.h>
#endif

/* Should be after config.h in main.cpp */
#include <icfg_desc.h>
/* Should be after config.h in main.cpp */

#include <interfaces.h>
#include <modslist.h>

std::string g_szAppName;
std::string g_szModsDir;
std::string g_szDataModsDir;
std::string g_szCfgPath;

static ModInfo modinfoLocal("net.rusjj.aml", "AML Core", "1.0.0.0", "RusJJ aka [-=KILL MAN=-]");
ModInfo* modinfo = &modinfoLocal;
static Config cfgLocal("ModLoaderCore");
Config* cfg = &cfgLocal;
static CFG icfgLocal;
ICFG* icfg = &icfgLocal;

typedef const char* (*SpecificGameFn)();
void LoadMods()
{
    std::filesystem::path filepath;
    std::filesystem::path datapath = g_szDataModsDir + "/libmodcopy.so";
    ModInfo* pModInfo = nullptr;
    SpecificGameFn maybeINeedAGame = nullptr;
	for (const auto& file : fs::recursive_directory_iterator(g_szModsDir.c_str()))
	{
		filepath = file.path();
		if (filepath.extension() == ".so")
		{
            fs::remove(datapath.string()); // Fix crash of fs::copy

            fs::copy(filepath.string(), datapath.string());
            chmod(datapath.string().c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);

            void* handle = dlopen(datapath.string().c_str(), RTLD_NOW); // Load it to RAM!
            
            GetModInfoFn modInfoFn = (GetModInfoFn)dlsym(handle, "__GetModInfo");
            if(modInfoFn != nullptr)
            {
                pModInfo = modInfoFn();
                maybeINeedAGame = (SpecificGameFn)dlsym(handle, "__INeedASpecificGame");
                if(maybeINeedAGame != nullptr && strcmp(maybeINeedAGame(), g_szAppName.c_str()) != 0)
                {
                    logger->Error("Mod (GUID %s) built for the game %s!", pModInfo->GUID(), maybeINeedAGame());
                    dlclose(handle);
                    goto nextMod;
                }
                if(dlsym(handle, "OnModPreLoad") == nullptr && dlsym(handle, "OnModLoad") == nullptr)
                {
                    logger->Error("Mod (GUID %s) has no EntryPoint!", pModInfo->GUID());
                    dlclose(handle);
                    goto nextMod;
                }
                if(!modlist->AddMod(pModInfo, (uintptr_t)handle))
                {
                    logger->Error("Mod (GUID %s) is already loaded!", pModInfo->GUID());
                    dlclose(handle);
                    goto nextMod;
                }
            }
            else
            {
                dlclose(handle);
            }

            nextMod:
            fs::remove(datapath.string());
		}
	}
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    logger->SetTag("AndroidModLoader");
    interfaces->Register("AMLInterface", aml);
    interfaces->Register("AMLConfig", icfg);
    modlist->AddMod(modinfo, 0);

    /* JNI Environment */
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        logger->Error("Cannot get JNI Environment!");
        return -1;
    }

    /* Application Context */
    jobject appContext = GetGlobalContext(env);

    /* Permissions! We really need them for configs! */
    /*if(!HasPermissionGranted(env, appContext, "READ_EXTERNAL_STORAGE") ||
       !HasPermissionGranted(env, appContext, "WRITE_EXTERNAL_STORAGE"))
    {
        RequestPermissions(env, appContext); // Instead of appContext should be !!!ACTIVITY!!! <- hard to get without SMALI-Inject (just a smali hand-rewritten, lol)
    }*/

    /* Package Name */
    g_szAppName = env->GetStringUTFChars(GetPackageName(env, appContext), NULL);
    std::transform(g_szAppName.begin(), g_szAppName.end(), g_szAppName.begin(), [](unsigned char c) { return std::tolower(c); });
    logger->Info("Determined app info: %s", g_szAppName.c_str());

    g_szModsDir = "/sdcard/Android/data/";
    g_szModsDir += g_szAppName;
    g_szModsDir += "/mods/";
    fs::create_directories(g_szModsDir.c_str());

    g_szCfgPath = "/sdcard/Android/data/";
    g_szCfgPath += g_szAppName;
    g_szCfgPath += "/configs/";
    fs::create_directories(g_szCfgPath.c_str());

    /* Data/Data Folder */
    g_szDataModsDir = env->GetStringUTFChars(GetAbsolutePath(env, GetFilesDir(env, appContext)), NULL);
    //g_szDataModsDir += "/aml/";
    fs::create_directories(g_szDataModsDir.c_str());

    cfg->Init();
    cfg->Bind("Author", "RusJJ aka [-=KILL MAN=-]");
    cfg->Save();

    /* Mods? */
    logger->Info("Working with mods...");
    #ifdef __IL2CPPUTILS
        IL2CPP::Func::HookFunctions();
    #endif
    LoadMods();

    /* All mods loaded. We should check for dependencies! */
    logger->Info("Checking for dependencies...");
    modlist->ProcessDependencies();
    modlist->ProcessPreLoading();
    modlist->ProcessLoading();

    logger->Info("Mods were launched!");


    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    /* Not sure if it'll work... */
    modlist->ProcessUnloading();
}