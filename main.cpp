#include <jni.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <filesystem>
namespace fs = std::filesystem;

#include <defines.h>
#include <logger/logger.h>

#include <interfaces.h>
#include <iaml.h>
#include <amlmod.h>

MYMOD(AMLLoader, 1.0.0, RusJJ aka [-=KILL MAN=-]);

std::string g_szAppName;
std::string g_szModsDir;
std::string g_szDataModsDir;

void LoadMods()
{
    logger->Info("Starting loading mods...");

    std::filesystem::path filepath;
    std::filesystem::path datapath = g_szDataModsDir + "libmodcopy.so";
	for (const auto& file : fs::recursive_directory_iterator(g_szModsDir.c_str()))
	{
		filepath = file.path();
		if (filepath.extension() == ".so")
		{
            fs::remove(datapath.string()); // Fix crash of fs::copy

            fs::copy(filepath.string(), datapath.string());
            chmod(datapath.string().c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);

            logger->Info("Working with %s...", filepath.filename().string().c_str());
            void* handle = dlopen(datapath.string().c_str(), RTLD_NOW);
            
            GetModInfoFn modInfo = (GetModInfoFn)dlsym(handle, "GetModInfo");
            if(modInfo != nullptr)
            {
                ModInfo* pModInfo = modInfo();
                logger->Info("Loaded: %s by %s", pModInfo->GetName(), pModInfo->GetAuthor());
                logger->Info("Version: %d.%d.%d.%d", pModInfo->Major(), pModInfo->Minor(), pModInfo->Revision(), pModInfo->Build());
            }
            else
            {
                logger->Info("%s has no ModInfo, stopped loading!", filepath.filename().string().c_str());
                dlclose(handle);
            }

            fs::remove(datapath.string());
		}
	}

    logger->Info("Finished!");
}

jobject GetGlobalContext(JNIEnv *env)
{
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);
    return context;
}

jstring GetPackageName(JNIEnv *env, jobject jActivity)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getPackageName", "()Ljava/lang/String;");
    return (jstring) env->CallObjectMethod(jActivity, method);
}

jobject GetFilesDir(JNIEnv *env, jobject jActivity)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getFilesDir", "()Ljava/io/File;");
    return (jstring) env->CallObjectMethod(jActivity, method);
}

jstring GetAbsolutePath(JNIEnv *env, jobject jFile)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jFile), "getAbsolutePath", "()Ljava/lang/String;");
    return (jstring) env->CallObjectMethod(jFile, method);
}

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    logger->SetTag("AndroidModLoader");
    interfaces->Register("AMLInterface", aml);

    /* JNI Environment */
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        logger->Error("Cannot get JNI Environment!");
        return -1;
    }
    logger->Info("JNI Environment is ready!");

    /* Application Context */
    logger->Info("Getting context...");
    jobject appContext = GetGlobalContext(env);
    logger->Info("Got context!");

    /* Package Name */
    logger->Info("Getting package name...");
    g_szAppName = env->GetStringUTFChars(GetPackageName(env, appContext), NULL);
    g_szModsDir = "/sdcard/Android/data/";
    g_szModsDir += g_szAppName;
    g_szModsDir += "/files/aml/";
    fs::create_directories(g_szModsDir.c_str());
    logger->Info("Got package name! Im inside \"%s\"!", g_szAppName.c_str());

    /* Data/Data Folder */
    logger->Info("Getting data folder...");
    g_szDataModsDir = env->GetStringUTFChars(GetAbsolutePath(env, GetFilesDir(env, appContext)), NULL);
    g_szDataModsDir += "/aml/";
    logger->Info("Got data folder! Folder is: %s", g_szDataModsDir.c_str());
    fs::create_directories(g_szDataModsDir.c_str());

    /* Mods? */
    LoadMods();

    return JNI_VERSION_1_6;
}