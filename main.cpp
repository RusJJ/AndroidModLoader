#include <jnifn.h>
#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <link.h>
#include <dlfcn.h>

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

char g_szInternalStoragePath[0xFF] = {0};
char g_szAppName[0xFF];
char g_szModsDir[0xFF];
char g_szAndroidDataDir[0xFF];
const char* g_szDataDir;
char g_szCfgPath[0xFF];

static ModInfo modinfoLocal("net.rusjj.aml", "AML Core", "1.0.0.5", "RusJJ aka [-=KILL MAN=-]");
ModInfo* modinfo = &modinfoLocal;
static Config cfgLocal("ModLoaderCore");
Config* cfg = &cfgLocal;
static CFG icfgLocal;
ICFG* icfg = &icfgLocal;

inline bool EndsWith(const char* base, const char* str)
{
    int blen = strlen(base);
    int slen = strlen(str);
    return (blen >= slen) && (0 == strcmp(base + blen - slen, str));
}

inline bool CopyFile(const char* file, const char* dest)
{
    FILE* source = fopen(file, "r");
    if(source == NULL) return false;
    FILE* target = fopen(dest, "w");
    if(target == NULL) 
    {
        fclose(source);
        return false;
    }
    while(!feof(source)) fputc(fgetc(source), target);
    fclose(source);
    fclose(target);
    return true;
}

typedef const char* (*SpecificGameFn)();
void LoadMods()
{
    ModInfo* pModInfo = NULL;
    SpecificGameFn maybeINeedAGame = NULL;

    char buf[0xFF], dataBuf[0xFF];
    DIR* dir; struct dirent *diread;
    if ((dir = opendir(g_szModsDir)) != NULL)
    {
        while ((diread = readdir(dir)) != NULL)
        {
            if(!EndsWith(diread->d_name, ".so")) continue;

            sprintf(buf, "%s/%s", g_szModsDir, diread->d_name);
            sprintf(dataBuf, "%s/%s", g_szDataDir, diread->d_name);
            remove(dataBuf);
            if(!CopyFile(buf, dataBuf)) continue;

            void* handle = dlopen(dataBuf, RTLD_NOW); // Load it to RAM!
            GetModInfoFn modInfoFn = (GetModInfoFn)dlsym(handle, "__GetModInfo");
            if(modInfoFn != NULL)
            {
                pModInfo = modInfoFn();
                maybeINeedAGame = (SpecificGameFn)dlsym(handle, "__INeedASpecificGame");
                if(maybeINeedAGame != NULL && strcmp(maybeINeedAGame(), g_szAppName) != 0)
                {
                    logger->Error("Mod (GUID %s) built for the game %s!", pModInfo->GUID(), maybeINeedAGame());
                    goto nextMod;
                }
                if(!modlist->AddMod(pModInfo, handle))
                {
                    logger->Error("Mod (GUID %s) is already loaded!", pModInfo->GUID());
                    goto nextMod;
                }
            }
            else
            {
              nextMod:
                dlclose(handle);
            }
            remove(dataBuf);
        }
        closedir(dir);
    }
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    logger->SetTag("AndroidModLoader");
    const char* szTmp; jstring jTmp;

    /* JNI Environment */
    JNIEnv* env = NULL;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        logger->Error("Cannot get JNI Environment!");
        return -1;
    }

    /* Must Have for mods */    
    interfaces->Register("AMLInterface", aml);
    interfaces->Register("AMLConfig", icfg);
    modlist->AddMod(modinfo, 0);

    /* Application Context */
    jobject appContext = GetGlobalContext(env);

    /* Permissions! We really need them for configs! */
    /*if(!HasPermissionGranted(env, appContext, "READ_EXTERNAL_STORAGE") ||
       !HasPermissionGranted(env, appContext, "WRITE_EXTERNAL_STORAGE"))
    {
        // Instead of appContext should be !!!ACTIVITY!!! <- Hard to get without SMALI-Inject (just a smali hand-rewritten, lol)
        RequestPermissions(env, appContext);
    }*/

    /* Internal Storage */
    jTmp = GetAbsolutePath(env, GetStorageDir(env));
    szTmp = env->GetStringUTFChars(jTmp, NULL);
    sprintf(g_szInternalStoragePath, "%s", szTmp);
    env->ReleaseStringUTFChars(jTmp, szTmp);


    /* Package Name */
    char i = 0;
    jTmp = GetPackageName(env, appContext);
    szTmp = env->GetStringUTFChars(jTmp, NULL);
    while(szTmp[i] != 0 && i < 0xFF)
    {
        g_szAppName[i] = tolower(szTmp[i]);
        ++i;
    } g_szAppName[i] = 0;
    env->ReleaseStringUTFChars(jTmp, szTmp);
    logger->Info("Determined app info: %s", g_szAppName);

    /* Create a folder in /Android/data/.../ */
    char szBuf[0xFF];
    sprintf(szBuf, "%s/Android/data/%s", g_szInternalStoragePath, g_szAppName);
    DIR* dir = opendir(szBuf);
    if(dir != NULL) closedir(dir);
    else GetExternalFilesDir(env, appContext);

    /* Create "mods" folder in /Android/data/.../ */
    sprintf(g_szModsDir, "%s/Android/data/%s/mods/", g_szInternalStoragePath, g_szAppName);
    mkdir(g_szModsDir, 0700);

    /* Create "files" folder in /Android/data/.../ */
    sprintf(g_szAndroidDataDir, "%s/Android/data/%s/files/", g_szInternalStoragePath, g_szAppName);
    mkdir(g_szAndroidDataDir, 0700); // Who knows, right?

    /* Create "configs" folder in /Android/data/.../ */
    sprintf(g_szCfgPath, "%s/Android/data/%s/configs/", g_szInternalStoragePath, g_szAppName);
    mkdir(g_szCfgPath, 0700);

    /* root/data/data Folder */
    g_szDataDir = env->GetStringUTFChars(GetAbsolutePath(env, GetFilesDir(env, appContext)), NULL);

    /* AML Config (unused currently) */
    cfg->Init();
    cfg->Bind("Author", "")->SetString("RusJJ aka [-=KILL MAN=-]");
    cfg->Bind("Version", "")->SetString(modinfo->VersionString());
    cfg->Bind("LaunchedTimeStamp", 0)->SetInt((int)time(NULL));
    cfg->Save();

    /* Mods? */
    logger->Info("Working with mods...");
    #ifdef __IL2CPPUTILS
        logger->Info("IL2CPP: Attempting to initialize IL2CPP-Utils");
        IL2CPP::Func::HookFunctions();
    #endif
    LoadMods();

    /* All mods are loaded now. We should check for dependencies! */
    logger->Info("Checking for dependencies...");
    modlist->ProcessDependencies();

    /* All mods are sorted and should be loaded! */
    modlist->ProcessPreLoading();
    modlist->ProcessLoading();
    modlist->OnAllModsLoaded();
    logger->Info("Mods were launched!");

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    /* Not sure if it'll work... */
    modlist->ProcessUnloading();
}