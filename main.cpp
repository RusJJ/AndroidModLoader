#ifndef DONT_USE_STB
    #include <mod/thirdparty/stb_sprintf.h>
    #define sprintf stbsp_sprintf
    #define snprintf stbsp_snprintf
#endif
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h> // mkdir
#include <sys/sendfile.h> // sendfile
#include <fcntl.h> // "open" flags
#include <dlfcn.h>

#include <aml.h>
#include <defines.h>
#include <modpaks.h>
#include <mls.h>
#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

#include <jnifn.h>

#ifdef __IL2CPPUTILS
    #include <il2cpp/functions.h>
#endif

// Should be after config.h in main.cpp
#include <icfg_desc.h>
// Should be after config.h in main.cpp

#include <interfaces.h>
#include <modslist.h>

bool g_bShowUpdatedToast, g_bShowUpdateFailedToast, g_bEnableFileDownloads;
bool g_bCrashAML, g_bNoMods, g_bSimplerCrashLog = false, g_bNoSPInLog, g_bNoModsInLog, g_bMLSOnlyManualSaves, g_bDumpAllThreads, g_bEHUnwind, g_bMoreRegsInfo, g_bUnixBacktrace = false;
int g_nEnableNews, g_nDownloadTimeout;
int g_nAndroidSDKVersion = 0;
ConfigEntry* g_pLastNewsId;
char g_szInternalStoragePath[256],
     g_szAppName[256],
     g_szFakeAppName[256],
     g_szModsDir[256],
     g_szInternalModsDir[256],
     g_szAndroidDataRootDir[256],
     g_szAndroidDataDir[256],
     g_szCfgPath[256],
     g_szFastman92Android[256];
const char* g_szDataDir;
char g_szUserAgent[256];

jobject appContext;
JNIEnv* env;

// Main
static ModInfo modinfoLocal("net.rusjj.aml", "AML Core", "1.3.1", "RusJJ aka [-=KILL MAN=-]");
ModInfo* amlmodinfo = &modinfoLocal;
static Config cfgLocal("ModLoaderCore");
Config* cfg = &cfgLocal;
static CFG icfgLocal; ICFG* icfg = &icfgLocal;

inline size_t __strlen(const char *str)
{
    const char* s = str;
    while(*s) ++s;
    return (s - str);
}

inline bool __ispathdel(char s)
{
    return (s == '\\' || s == '/');
}

inline void __pathback(char *str)
{
    const char* s = str;
    uint16_t i = 0;
    while(*s) ++s;
    while(s != str)
    {
        if(!__ispathdel(*(--s))) break;
    }
    while(s != str)
    {
        if(__ispathdel(*(--s)))
        {
            i = (uint16_t)(s - str);
        }
        else if(i != 0) break;
    }
    if(i > 0) str[i] = 0;
}

inline bool EndsWith(const char* base, const char* str)
{
    static int blen, slen;
    blen = strlen(base);
    slen = strlen(str);
    return (blen >= slen) && (!strcmp(base + blen - slen, str));
}

inline bool EndsWithSO(const char* base)
{
    static int blen;
    blen = strlen(base);
    return (blen >= 3) && (!strcmp(base + blen - 3, ".so"));
}

// Is this actually faster, bruh?
// P.S. Yeah, it is!
inline bool CopyFileFaster(const char* file, const char* dest)
{
    int inFd = open(file, O_RDONLY);
    if(inFd < 0) return false;
    struct stat statBuf;
    fstat(inFd, &statBuf);
    int outFd = open(dest, O_WRONLY | O_CREAT, statBuf.st_mode);
    if(outFd < 0)
    {
        close(inFd);
        return false;
    }
    if(sendfile(outFd, inFd, NULL, statBuf.st_size) < 0)
    {
        close(inFd);
        close(outFd);
        return false;
    }
    close(inFd);
    close(outFd);
    return true;
}
// Slower version (because it copies the file contents byte-by-byte)
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

bool AML_CopyFile(const char* file, const char* dest)
{
    return (!CopyFileFaster(file, dest) && !CopyFile(file, dest));
}

inline bool HasFakeAppName()
{
    return (g_szFakeAppName[0] != 0 && strlen(g_szFakeAppName) > 5);
}

typedef const char* (*SpecificGameFn)();
void LoadMods(const char* path)
{
    ModInfo* pModInfo = NULL;
    SpecificGameFn maybeINeedAGame = NULL;
    GetModInfoFn modInfoFn = NULL;

    char buf[256], dataBuf[256];
    DIR* dir = opendir(path);
    if (dir != NULL)
    {
        logger->Info("Loading mods from %s", path);
        struct dirent *diread; void* handle;
        const char* gameName = HasFakeAppName() ? g_szFakeAppName : g_szAppName;
        while ((diread = readdir(dir)) != NULL)
        {
            if(diread->d_name[0] == '.') continue; // Skip . and ..
            if(!EndsWithSO(diread->d_name))
            {
                // Useless info for us
                //logger->Error("File %s is not a mod, atleast it is NOT .SO file!", diread->d_name);
                continue;
            }
            snprintf(buf, sizeof(buf), "%s/%s", path, diread->d_name);
            snprintf(dataBuf, sizeof(dataBuf), "%s/%s", g_szDataDir, diread->d_name);
            //unlink(dataBuf);
            chmod(dataBuf, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP); // XMDS
            int removeStatus = remove(dataBuf);
            //if(removeStatus != 0) logger->Error("Failed to remove temp mod file! This might break the mod loading. Error %d", removeStatus);
            if(!CopyFileFaster(buf, dataBuf) && !CopyFile(buf, dataBuf))
            {
                logger->Error("File %s is failed to be copied! :(", diread->d_name);
                continue;
            }
            chmod(dataBuf, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);

            handle = dlopen(dataBuf, RTLD_NOW); // Load it to RAM
            modInfoFn = (GetModInfoFn)dlsym(handle, "__GetModInfo");
            if(modInfoFn != NULL)
            {
                pModInfo = modInfoFn();
                maybeINeedAGame = (SpecificGameFn)dlsym(handle, "__INeedASpecificGame");
                if(maybeINeedAGame != NULL && strcmp(maybeINeedAGame(), gameName) != 0)
                {
                    logger->Error("Mod (GUID %s) built for the game %s!", pModInfo->GUID(), maybeINeedAGame());
                    goto nextMod;
                }
                if(!modlist->AddMod(pModInfo, handle, buf))
                {
                    logger->Error("Mod (GUID %s) is already loaded!", pModInfo->GUID());
                    goto nextMod;
                }
                
                logger->Info("Mod (GUID %s) has been preprocessed.", pModInfo->GUID());
            }
            else
            {
              nextMod:
                dlclose(handle);
            }
            //unlink(dataBuf);
            removeStatus = remove(dataBuf);
            if(removeStatus != 0) logger->Error("Failed to remove temp mod file! This might break the mod loading. Error %d", removeStatus);
        }
        closedir(dir);
    }
    else
    {
        logger->Error("Failed to load mods: unable to open directory");
    }
}

extern ModDesc* pLastModProcessed;

void StartSignalHandler();
void HookALog();
JavaVM *g_pJavaVM = NULL;
void *g_pJavaReserved = NULL;

extern bool bAndroidLog_OnlyImportant, bAndroidLog_NoAfter, bAML_HasFastmanModified;
bool g_bAMLStarted = false;

pthread_key_t g_JNIThreadKey;
JNIEnv* GetCurrentJNI()
{
    JNIEnv* env = NULL;
    if(g_JNIThreadKey)
    {
        env = (JNIEnv*)pthread_getspecific(g_JNIThreadKey);
        if(env) return env;
    }
    else
    {
        pthread_key_create(&g_JNIThreadKey, NULL);
    }

    if(g_pJavaVM)
    {
        jint result = g_pJavaVM->AttachCurrentThread(&env, NULL);
        if(result == 0 && env)
        {
            pthread_setspecific(g_JNIThreadKey, env);
            return env;
        }
    }
    return NULL;
}

// https://stackoverflow.com/questions/46869901/how-to-get-the-android-context-instance-when-calling-jni-method
jobject g_GlobalContext = 0;
jobject GetCurrentContext()
{
    if(g_GlobalContext) return g_GlobalContext;

    JNIEnv* env = GetCurrentJNI();
    if(!env) return 0;

    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject activityThreadObj = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(activityThread, "getApplication", "()Landroid/app/Application;");

    g_GlobalContext = env->CallObjectMethod(activityThreadObj, getApplication);
    if(g_GlobalContext) g_GlobalContext = env->NewGlobalRef(g_GlobalContext);
    return g_GlobalContext;
}

void StartAMLRightNow(const char* libName1 = NULL, const char* libName2 = NULL)
{
    if(g_bAMLStarted)
    {
        logger->Error("Something was trying to boot-up AML again.");
        return;
    }

    void* lib1 = libName1 ? dlopen(libName1, RTLD_NOW) : NULL;
    void* lib2 = libName2 ? dlopen(libName2, RTLD_NOW) : NULL;

    logger->SetTag("AndroidModLoader");
    const char* szTmp; jstring jTmp;

    /* JNI Environment */
    if (g_pJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        logger->Error("Cannot get JNI Environment!");
        return;
    }

    /* Application Context */
    appContext = GetGlobalContext(env);
    if(appContext == NULL)
    {
        logger->Error("AML Library should be loaded in \"onCreate\" or by injecting it directly into the main game library!");
        return;
    }

    /* Must Have for mods */
    modlist->AddMod(amlmodinfo, 0, "localpath (core)");
    interfaces->Register("AMLInterface", aml);
    interfaces->Register("AMLConfig", icfg);
    InitCURL();

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
    snprintf(g_szInternalStoragePath, sizeof(g_szInternalStoragePath), "%s", szTmp);
    env->ReleaseStringUTFChars(jTmp, szTmp);

    /* Package Name */
    char i = 0;
    jTmp = GetPackageName(env, appContext);
    szTmp = env->GetStringUTFChars(jTmp, NULL);
    while(szTmp[i] != 0 && i < sizeof(g_szAppName)-1)
    {
        g_szAppName[i] = tolower(szTmp[i]);
        ++i;
    } g_szAppName[i] = 0;
    env->ReleaseStringUTFChars(jTmp, szTmp);
    logger->Info("Determined app info: %s", g_szAppName);

  #ifdef FASTMAN92_CODE
    /* Fastman92 Part */
    bAML_HasFastmanModified = GetExternalFilesDir_FLA(env, appContext, g_szFastman92Android, sizeof(g_szFastman92Android));
    __pathback(g_szFastman92Android);

    // Android/data/... dir
    snprintf(g_szAndroidDataRootDir, sizeof(g_szAndroidDataRootDir), "%s/", g_szFastman92Android);
    
    // Android/data/.../mods dir
    snprintf(g_szModsDir, sizeof(g_szModsDir), "%s/mods/", g_szFastman92Android);
    mkdir(g_szModsDir, 0777);

    // Android/data/.../files dir
    snprintf(g_szAndroidDataDir, sizeof(g_szAndroidDataDir), "%s/files/", g_szFastman92Android);
    mkdir(g_szAndroidDataDir, 0777);
    
    // Android/data/.../configs dir
    snprintf(g_szCfgPath, sizeof(g_szCfgPath), "%s/configs/", g_szFastman92Android);
    mkdir(g_szCfgPath, 0777);
  #else
    /* Create a folder in /Android/data/.../ */
    snprintf(g_szAndroidDataRootDir, sizeof(g_szAndroidDataRootDir), "%s/Android/data/%s/", g_szInternalStoragePath, g_szAppName);
    DIR* dir = opendir(g_szAndroidDataRootDir);
    if(dir != NULL) closedir(dir);
    else GetExternalFilesDir(env, appContext);

    /* Create "mods" folder in /Android/data/.../ */
    snprintf(g_szModsDir, sizeof(g_szModsDir), "%s/Android/data/%s/mods/", g_szInternalStoragePath, g_szAppName);
    mkdir(g_szModsDir, 0777);

    /* Create "files" folder in /Android/data/.../ */
    snprintf(g_szAndroidDataDir, sizeof(g_szAndroidDataDir), "%s/Android/data/%s/files/", g_szInternalStoragePath, g_szAppName);
    mkdir(g_szAndroidDataDir, 0777); // Who knows, right?

    /* Create "configs" folder in /Android/data/.../ */
    snprintf(g_szCfgPath, sizeof(g_szCfgPath), "%s/Android/data/%s/configs/", g_szInternalStoragePath, g_szAppName);
    mkdir(g_szCfgPath, 0777);
  #endif

    /* root/data/data Folder */
    g_szDataDir = env->GetStringUTFChars(GetAbsolutePath(env, GetFilesDir(env, appContext)), NULL);

    /* AML Config */
    logger->Info("Reading config...");
    cfg->Init();
    cfg->Bind("Author", "")->SetString("RusJJ aka [-=KILL MAN=-]"); cfg->ClearLast();
    cfg->Bind("Discord", "")->SetString("https://discord.gg/2MY7W39kBg"); cfg->ClearLast();
    bool bHasChangedCfgAuthor = cfg->IsValueChanged();
    cfg->Bind("Version", "")->SetString(amlmodinfo->VersionString()); cfg->ClearLast();
    cfg->Bind("LaunchedTimeStamp", 0)->SetInt((int)time(NULL)); cfg->ClearLast();
    cfg->Bind("FakePackageName", "")->GetString(g_szFakeAppName, sizeof(g_szFakeAppName)); cfg->ClearLast();
    snprintf(g_szInternalModsDir, sizeof(g_szInternalModsDir), "%s/%s/%s", g_szInternalStoragePath, cfg->Bind("InternalModsFolder", "AMLMods")->GetString(), g_szAppName); cfg->ClearLast();
    bool internalModsPriority = cfg->GetBool("InternalModsFirst", true);
    logger->ToggleOutput(cfg->GetBool("EnableLogcats", true));
    bool bEnableUpdater = cfg->GetBool("EnableUpdater", true);
    g_bShowUpdatedToast = cfg->GetBool("ShowUpdaterToast", true);
    g_bShowUpdateFailedToast = cfg->GetBool("ShowUpdaterFailedToast", true);
    g_bEnableFileDownloads = cfg->GetBool("EnableModFileDownloads", true);
    g_nEnableNews = cfg->GetInt("ShowNewsForFewTimes", 3);
    g_pLastNewsId = cfg->Bind("LastNewsIdShowed", 0, "Savings");
    g_nDownloadTimeout = cfg->GetInt("DownloadTimeout", 2);

    g_bCrashAML = cfg->GetBool("CrashAML", false, "DevTools");
    g_bNoMods = cfg->GetBool("DontLoadMods", false, "DevTools");
    //g_bSimplerCrashLog = cfg->GetBool("SimplerCrashLogs", g_bSimplerCrashLog, "DevTools");
    g_bNoSPInLog = cfg->GetBool("NoStackInCrashLog", false, "DevTools");
    g_bNoModsInLog = cfg->GetBool("NoModsInCrashLog", false, "DevTools");
    g_bMLSOnlyManualSaves = cfg->GetBool("MLSOnlyManualSaves", false, "DevTools");
    g_bDumpAllThreads = cfg->GetBool("CrashLogFromAllThreads", true, "DevTools");
    g_bEHUnwind = cfg->GetBool("EHUnwindCrashLog", false, "DevTools");
    g_bMoreRegsInfo = cfg->GetBool("MoreRegistersInfo", true, "DevTools");

    if(g_nDownloadTimeout < 1) g_nDownloadTimeout = 1;
    else if(g_nDownloadTimeout > 5) g_nDownloadTimeout = 5;
    cfg->Save();

    /* Android version */
    char sdk_ver_str[92]; // PROPERTY_VALUE_MAX
    if(__system_property_get("ro.build.version.sdk", sdk_ver_str))
    {
        g_nAndroidSDKVersion = atoi(sdk_ver_str);
    }
    else if(g_nAndroidSDKVersion == 0)
    {
        if(__system_property_get("ro.build.version.release", sdk_ver_str))
        {
            g_nAndroidSDKVersion = atoi(sdk_ver_str);
        }
        else
        {
            jclass versionClass = env->FindClass("android/os/Build$VERSION");
            jfieldID sdkIntFieldID = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
            g_nAndroidSDKVersion = env->GetStaticIntField(versionClass, sdkIntFieldID);
        }
    }

    /* Catch the fish! */
    if(cfg->GetBool("SignalHandler", true))
    {
        g_pAML->AddFeature("SIGNAL");
        StartSignalHandler();
    }

    /* Catch another fish! */
    bAndroidLog_OnlyImportant = !cfg->GetBool("PrintLogsToFile_Verbose", false);
    bAndroidLog_NoAfter = cfg->GetBool("PrintLogsToFile_NoLogCat", false);
    if(cfg->GetBool("PrintLogsToFile", false))
    {
        g_pAML->AddFeature("LOGHOOK");
        HookALog();
    }

    /* Mods? */
    logger->Info("Working with mods...");
    #ifdef __IL2CPPUTILS
        logger->Info("IL2CPP: Attempting to initialize IL2CPP-Utils");
        IL2CPP::Func::HookFunctions();
    #endif
    if(!g_bNoMods)
    {
        MLS::LoadFile();
        if(g_szInternalModsDir[0] != 0)
        {
            LoadMods(internalModsPriority ? g_szInternalModsDir : g_szModsDir);
            LoadMods(internalModsPriority ? g_szModsDir : g_szInternalModsDir);
        }
        else
        {
            LoadMods(g_szModsDir);
        }
    }

    /* All mods are loaded now. We should check for dependencies! */
    logger->Info("Checking for dependencies...");
    modlist->ProcessDependencies();
    
    /* Process features */
    #ifdef __XDL
        g_pAML->AddFeature("XDL");
    #endif
    #ifdef __IL2CPPUTILS
        g_pAML->AddFeature("IL2CPP");
    #endif
    if(g_pAML->IsGameFaked()) g_pAML->AddFeature("FAKEGAME");
    if(bHasChangedCfgAuthor) g_pAML->AddFeature("STEALER"); // For fun
    if(!logger->HasOutput()) g_pAML->AddFeature("NOLOGGING");
    
    /* Load news first! */
    if(g_nEnableNews > 0)
    {
        char newsBuf[512]; memset(newsBuf, 0, sizeof(newsBuf));
        
        if(aml->DownloadFileToData("https://raw.githubusercontent.com/RusJJ/AndroidModLoader/main/news.txt", newsBuf, sizeof(newsBuf)) && newsBuf[0])
        {
            if(strncmp(g_pLastNewsId->GetString(), newsBuf, 16) != 0)
            {
                for(int i = 0; i < g_nEnableNews; ++i)
                {
                    aml->ShowToast(true, newsBuf);
                }

                newsBuf[16] = 0;
                g_pLastNewsId->SetString(newsBuf);
                cfg->Save();
            }
            delete g_pLastNewsId;
        }
    }
    
    /* All mods are sorted and should be loaded! */
    if(bEnableUpdater)
    {
        g_pAML->AddFeature("UPDATER");
        modlist->ProcessUpdater();
        logger->Info("Mods were updated!");
    }
    if(!g_bNoMods)
    {
        modlist->ProcessPreLoading();
        modlist->ProcessLoading();
        modlist->OnAllModsLoaded();
        logger->Info("Mods were launched!");
    }
    pLastModProcessed = NULL;

    #ifdef AML32
        snprintf(g_szUserAgent, sizeof(g_szUserAgent), "AndroidModLoader/%s (Android; ARM; ARM32)", amlmodinfo->VersionString());
    #else
        snprintf(g_szUserAgent, sizeof(g_szUserAgent), "AndroidModLoader/%s (Android; ARM; ARM64)", amlmodinfo->VersionString());
    #endif

    /* Fake crash for crash handler testing (does not work?) */
    //if(g_bCrashAML) __builtin_trap(); // Dont let really weird guys to use this...

    // TODO: should be loaded in a different thread..?
    if(lib1)
    {
        auto libEntry = (void(*)(JavaVM*, void*))dlsym(lib1, "JNI_OnLoad");
        if(libEntry) libEntry(g_pJavaVM, g_pJavaReserved);
    }
    if(lib2)
    {
        auto libEntry = (void(*)(JavaVM*, void*))dlsym(lib2, "JNI_OnLoad");
        if(libEntry) libEntry(g_pJavaVM, g_pJavaReserved);
    }

    g_bAMLStarted = true;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    g_pJavaVM = vm;
    g_pJavaReserved = reserved;
    
    /* For the delayed start-up (later) */
    StartAMLRightNow();
    
    /* Return the value it needs */
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    /* Not sure if it'll work... */
    /* It worked once, lol */
    modlist->ProcessUnloading();
}