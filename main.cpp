#include <jnifn.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <filesystem>
namespace fs = std::filesystem;

#include <defines.h>
#include <mod/amlmod.h>
#include <logger/logger.h>

#include <interfaces.h>
#include <modslist.h>

std::string g_szAppName;
std::string g_szModsDir;
std::string g_szDataModsDir;

static ModInfo modinfoLocal("net.rusjj.aml", "AML Core", "1.0.0.0", "RusJJ aka [-=KILL MAN=-]");
ModInfo* modinfo = &modinfoLocal;

void LoadMods()
{
    std::filesystem::path filepath;
    std::filesystem::path datapath = g_szDataModsDir + "libmodcopy.so";
    ModInfo* pModInfo = nullptr;
	for (const auto& file : fs::recursive_directory_iterator(g_szModsDir.c_str()))
	{
		filepath = file.path();
		if (filepath.extension() == ".so")
		{
            fs::remove(datapath.string()); // Fix crash of fs::copy

            fs::copy(filepath.string(), datapath.string());
            chmod(datapath.string().c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);

            void* handle = dlopen(datapath.string().c_str(), RTLD_NOW); // Load it to RAM!
            
            GetModInfoFn modInfoFn = (GetModInfoFn)dlsym(handle, "GetModInfo");
            if(modInfoFn != nullptr)
            {
                pModInfo = modInfoFn();
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

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    logger->SetTag("AndroidModLoader");
    interfaces->Register("AMLInterface", aml);
    modlist->AddMod(modinfo, 0);

    /* JNI Environment */
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        logger->Error("Cannot get JNI Environment!");
        return -1;
    }

    logger->Info("Determining app info...");

    /* Application Context */
    jobject appContext = GetGlobalContext(env);

    /* Package Name */
    g_szAppName = env->GetStringUTFChars(GetPackageName(env, appContext), NULL);
    g_szModsDir = "/sdcard/Android/data/";
    g_szModsDir += g_szAppName;
    g_szModsDir += "/files/aml/";
    fs::create_directories(g_szModsDir.c_str());

    /* Data/Data Folder */
    g_szDataModsDir = env->GetStringUTFChars(GetAbsolutePath(env, GetFilesDir(env, appContext)), NULL);
    g_szDataModsDir += "/aml/";
    fs::create_directories(g_szDataModsDir.c_str());

    /* Mods? */
    logger->Info("Working with mods...");
    LoadMods();

    /* All mods loaded. We should check for dependencies! */
    logger->Info("Checking for dependencies...");
    modlist->ProcessDependencies();
    modlist->ProcessPreLoading();
    modlist->ProcessLoading();

    logger->Info("Mods were launched!");

    return JNI_VERSION_1_6;
}