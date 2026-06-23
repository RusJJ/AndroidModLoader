#ifndef __JNIFN_H
#define __JNIFN_H

#include <string>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <unordered_map>
#include <mod/localref.h>
#include <mod/scopeguard.h>

extern JavaVM *g_pJavaVM;

JNIEnv* GetCurrentJNI();
jobject GetCurrentContext();
extern std::unordered_map<std::string, jobject> g_InjectedInstances;

template<typename T>
inline LocalRef<T> MakeJniLocalRef(JNIEnv* env, T ref)
{
    return LocalRef<T>(ref, [env](T localRef)
    {
        if(localRef) env->DeleteLocalRef(localRef);
    });
}

inline void ClearJNIException(JNIEnv* env)
{
    if(env && env->ExceptionCheck()) env->ExceptionClear();
}

inline jobject GetGlobalContext(JNIEnv *env)
{
    if(!env) return NULL;
    auto activityThread = MakeJniLocalRef(env, env->FindClass("android/app/ActivityThread"));
    if(!activityThread.Get()) { ClearJNIException(env); return NULL; }
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread.Get(), "currentActivityThread", "()Landroid/app/ActivityThread;");
    if(!currentActivityThread) { ClearJNIException(env); return NULL; }
    auto at = MakeJniLocalRef(env, env->CallStaticObjectMethod(activityThread.Get(), currentActivityThread));
    ClearJNIException(env);
    if(!at.Get()) return NULL;
    jmethodID getApplication = env->GetMethodID(activityThread.Get(), "getApplication", "()Landroid/app/Application;");
    if(!getApplication) { ClearJNIException(env); return NULL; }
    jobject context = env->CallObjectMethod(at.Get(), getApplication);
    ClearJNIException(env);
    return context;
}

inline jstring GetPackageName(JNIEnv *env, jobject jActivity)
{
    if(!env || !jActivity) return NULL;
    auto activityClass = MakeJniLocalRef(env, env->GetObjectClass(jActivity));
    if(!activityClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID method = env->GetMethodID(activityClass.Get(), "getPackageName", "()Ljava/lang/String;");
    if(!method) { ClearJNIException(env); return NULL; }
    jstring ret = (jstring)env->CallObjectMethod(jActivity, method);
    ClearJNIException(env);
    return ret;
}

inline jobject GetFilesDir(JNIEnv *env, jobject jActivity)
{
    if(!env || !jActivity) return NULL;
    auto activityClass = MakeJniLocalRef(env, env->GetObjectClass(jActivity));
    if(!activityClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID method = env->GetMethodID(activityClass.Get(), "getFilesDir", "()Ljava/io/File;");
    if(!method) { ClearJNIException(env); return NULL; }
    jobject ret = env->CallObjectMethod(jActivity, method);
    ClearJNIException(env);
    return ret;
}

inline jstring GetAbsolutePath(JNIEnv *env, jobject jFile)
{
    if(!env || !jFile) return NULL;
    auto fileClass = MakeJniLocalRef(env, env->GetObjectClass(jFile));
    if(!fileClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID method = env->GetMethodID(fileClass.Get(), "getAbsolutePath", "()Ljava/lang/String;");
    if(!method) { ClearJNIException(env); return NULL; }
    jstring ret = (jstring)env->CallObjectMethod(jFile, method);
    ClearJNIException(env);
    return ret;
}

inline jstring GetAndroidPermission(JNIEnv* env, const char* szPermissionName)
{
    auto ClassManifestPermission = MakeJniLocalRef(env, env->FindClass("android/Manifest$permission"));
    if(!ClassManifestPermission.Get()) { ClearJNIException(env); return NULL; }
    jfieldID lid_PERM = env->GetStaticFieldID(ClassManifestPermission.Get(), szPermissionName, "Ljava/lang/String;");
    if(!lid_PERM) { ClearJNIException(env); return NULL; }
    jstring ret = (jstring)env->GetStaticObjectField(ClassManifestPermission.Get(), lid_PERM);
    ClearJNIException(env);
    return ret;
}

inline bool HasPermissionGranted(JNIEnv* env, jobject jActivity, const char* szPermissionName)
{
    auto ClassPackageManager = MakeJniLocalRef(env, env->FindClass("android/content/pm/PackageManager"));
    //bool result = false;
    auto ls_PERM = MakeJniLocalRef(env, GetAndroidPermission(env, szPermissionName));
    if(!ClassPackageManager.Get() || !ls_PERM.Get()) { ClearJNIException(env); return false; }
    jfieldID lid_PERMISSION_GRANTED = env->GetStaticFieldID(ClassPackageManager.Get(), "PERMISSION_GRANTED", "I");
    jint PERMISSION_GRANTED = jint(-1);

    if(lid_PERMISSION_GRANTED) PERMISSION_GRANTED = env->GetStaticIntField(ClassPackageManager.Get(), lid_PERMISSION_GRANTED);
    auto contextClass = MakeJniLocalRef(env, env->FindClass("android/content/Context"));
    jmethodID checkSelfPermission = contextClass.Get() ? env->GetMethodID(contextClass.Get(), "checkSelfPermission", "(Ljava/lang/String;)I") : NULL;
    jint int_result = checkSelfPermission ? env->CallIntMethod(jActivity, checkSelfPermission, ls_PERM.Get()) : jint(-1);
    ClearJNIException(env);
    return (int_result == PERMISSION_GRANTED);
}

/*inline void RequestPermissions(JNIEnv* env, jobject jActivity)
{
    jobjectArray perm_array = env->NewObjectArray(2, env->FindClass("java/lang/String"), env->NewStringUTF(""));
    env->SetObjectArrayElement(perm_array, 0, GetAndroidPermission(env, "READ_EXTERNAL_STORAGE"));
    env->SetObjectArrayElement(perm_array, 1, GetAndroidPermission(env, "WRITE_EXTERNAL_STORAGE"));
    env->CallVoidMethod(jActivity, env->GetMethodID(env->FindClass("android/app/Activity"), "requestPermissions", "([Ljava/lang/String;I)V"), perm_array, 0);
}*/

inline jobject GetExternalFilesDir(JNIEnv* env, jobject jActivity) // getExternalFilesDir creates directory in Android/data, lol
{
    auto activityClass = MakeJniLocalRef(env, env->GetObjectClass(jActivity));
    if(!activityClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID method = env->GetMethodID(activityClass.Get(), "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    if(!method) { ClearJNIException(env); return NULL; }
    jobject ret = env->CallObjectMethod(jActivity, method, NULL);
    ClearJNIException(env);
    return ret;
}

// fastman92
#ifdef FASTMAN92_CODE
inline bool GetExternalFilesDir_FLA(JNIEnv* env, jobject context, char* strPath, size_t bufferSize)
{
    jobject objectFile = NULL;
    bool bReadFromF92launcher = false;
    jmethodID methodIDgetExternalFilesDir;

    auto classF92launcherSettings = MakeJniLocalRef(env, env->FindClass("com/fastman92/main_activity_launcher/Settings"));

    if (classF92launcherSettings.Get())
    {
        methodIDgetExternalFilesDir = env->GetStaticMethodID(classF92launcherSettings.Get(), "getExternalFilesDir", "(Landroid/content/Context;)Ljava/io/File;");
        
        if (methodIDgetExternalFilesDir)
        {
            objectFile = env->CallStaticObjectMethod(classF92launcherSettings.Get(), methodIDgetExternalFilesDir, context);
            bReadFromF92launcher = true;
        }
    }

    if (!bReadFromF92launcher)
    {
        ClearJNIException(env);

        auto android_content_Context = MakeJniLocalRef(env, env->GetObjectClass(context));
        if(!android_content_Context.Get()) { ClearJNIException(env); return false; }

        methodIDgetExternalFilesDir = env->GetMethodID(android_content_Context.Get(), "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        if(!methodIDgetExternalFilesDir) { ClearJNIException(env); return false; }

        objectFile = (jstring)env->CallObjectMethod(context, methodIDgetExternalFilesDir, nullptr);
    }

    auto objectFileRef = MakeJniLocalRef(env, objectFile);
    if(!objectFileRef.Get())
    {
        ClearJNIException(env);
        return false;
    }
        
    auto classFile = MakeJniLocalRef(env, env->GetObjectClass(objectFileRef.Get()));
    if(!classFile.Get()) { ClearJNIException(env); return bReadFromF92launcher; }

    jmethodID methodIDgetAbsolutePath = env->GetMethodID(classFile.Get(), "getAbsolutePath", "()Ljava/lang/String;");
    auto stringPath = MakeJniLocalRef(env, methodIDgetAbsolutePath ? (jstring)env->CallObjectMethod(objectFileRef.Get(), methodIDgetAbsolutePath) : NULL);

    if (stringPath.Get())
    {
        const char* strPathValueStr = env->GetStringUTFChars(stringPath.Get(), NULL);
        if(!strPathValueStr) { ClearJNIException(env); return bReadFromF92launcher; }
        DEFER(env->ReleaseStringUTFChars(stringPath.Get(), strPathValueStr));

        strxcpy(strPath, strPathValueStr, bufferSize);
    }
    ClearJNIException(env);
    return bReadFromF92launcher;
}
#endif

inline jobject GetStorageDir(JNIEnv* env) // /storage/emulated/0 instead of /sdcard (example)
{
    auto classEnvironment = MakeJniLocalRef(env, env->FindClass("android/os/Environment"));
    if(!classEnvironment.Get()) { ClearJNIException(env); return NULL; }
    jmethodID getExternalStorageDirectory = env->GetStaticMethodID(classEnvironment.Get(), "getExternalStorageDirectory", "()Ljava/io/File;");
    jobject ret = getExternalStorageDirectory ? env->CallStaticObjectMethod(classEnvironment.Get(), getExternalStorageDirectory) : NULL;
    ClearJNIException(env);
    return ret;
}

inline void ShowToastMessage(JNIEnv* env, jobject jActivity, const char* txt, int msDuration)
{
    auto ToastClass = MakeJniLocalRef(env, env->FindClass("android/widget/Toast"));
    if(!ToastClass.Get()) { ClearJNIException(env); return; }
    jmethodID makeTextMethodID = env->GetStaticMethodID(ToastClass.Get(), "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
    jmethodID showMethodID = env->GetMethodID(ToastClass.Get(), "show", "()V");

    auto message = MakeJniLocalRef(env, env->NewStringUTF(txt));
    jint duration = msDuration; // can be Toast.LENGTH_SHORT or Toast.LENGTH_LONG

    auto toast = MakeJniLocalRef(env, (makeTextMethodID && message.Get()) ? env->CallStaticObjectMethod(ToastClass.Get(), makeTextMethodID, jActivity, message.Get(), duration) : NULL);
    if(toast.Get() && showMethodID) env->CallVoidMethod(toast.Get(), showMethodID);

    ClearJNIException(env);
}

inline void ShowToastMessage2(JNIEnv* env, jobject jActivity, const char* txt, jint duration)
{
    auto ToastClass = MakeJniLocalRef(env, env->FindClass("android/widget/Toast"));
    if(!ToastClass.Get()) { ClearJNIException(env); return; }
    jmethodID makeTextMethodID = env->GetStaticMethodID(ToastClass.Get(), "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
    jmethodID showMethodID = env->GetMethodID(ToastClass.Get(), "show", "()V");
    jmethodID setDurationMethodID = env->GetMethodID(ToastClass.Get(), "setDuration", "(I)V");

    auto message = MakeJniLocalRef(env, env->NewStringUTF(txt));
    auto toast = MakeJniLocalRef(env, (makeTextMethodID && message.Get()) ? env->CallStaticObjectMethod(ToastClass.Get(), makeTextMethodID, jActivity, message.Get(), duration) : NULL);
    if(toast.Get() && setDurationMethodID) env->CallVoidMethod(toast.Get(), setDurationMethodID, duration);
    if(toast.Get() && showMethodID) env->CallVoidMethod(toast.Get(), showMethodID);

    ClearJNIException(env);
}

inline jstring GetNativeLibDir(JNIEnv* env)
{
    jobject context = ::GetCurrentContext();
    if(!env || !context) return NULL;

    auto contextClass = MakeJniLocalRef(env, env->GetObjectClass(context));
    if(!contextClass.Get()) { ClearJNIException(env); return NULL; }

    jmethodID getAppInfoId = env->GetMethodID(contextClass.Get(), "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    auto appInfo = MakeJniLocalRef(env, getAppInfoId ? env->CallObjectMethod(context, getAppInfoId) : NULL);
    if(!appInfo.Get()) { ClearJNIException(env); return NULL; }

    auto appInfoClass = MakeJniLocalRef(env, env->GetObjectClass(appInfo.Get()));
    if(!appInfoClass.Get()) { ClearJNIException(env); return NULL; }
    jfieldID nativeLibDirField = env->GetFieldID(appInfoClass.Get(), "nativeLibraryDir", "Ljava/lang/String;");
    jstring nativeLibDir = nativeLibDirField ? (jstring)env->GetObjectField(appInfo.Get(), nativeLibDirField) : NULL;

    ClearJNIException(env);

    return nativeLibDir;
}

inline AAssetManager* GetAssetManager(JNIEnv* env)
{
    jobject context = ::GetCurrentContext();
    if(!env || !context) return NULL;

    auto contextClass = MakeJniLocalRef(env, env->GetObjectClass(context));
    if(!contextClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID getAssetsMethod = env->GetMethodID(contextClass.Get(), "getAssets", "()Landroid/content/res/AssetManager;");
    
    auto javaAssetManager = MakeJniLocalRef(env, getAssetsMethod ? env->CallObjectMethod(context, getAssetsMethod) : NULL);
    
    ClearJNIException(env);

    AAssetManager* assetManager = javaAssetManager.Get() ? AAssetManager_fromJava(env, javaAssetManager.Get()) : NULL;
    return assetManager;
}

inline jobject LoadSmaliDEX(JNIEnv* env, const uint8_t* dexBytes, size_t dexSize)
{
    static int dexCount = 0;
    
    jobject context = ::GetCurrentContext();
    if (!context) return NULL;

    auto contextClass = MakeJniLocalRef(env, env->GetObjectClass(context));
    if(!contextClass.Get()) { ClearJNIException(env); return NULL; }

    jmethodID getClassLoaderMethod = env->GetMethodID(contextClass.Get(), "getClassLoader", "()Ljava/lang/ClassLoader;");
    auto appClassLoader = MakeJniLocalRef(env, getClassLoaderMethod ? env->CallObjectMethod(context, getClassLoaderMethod) : NULL);
    if (!appClassLoader.Get()) { ClearJNIException(env); return NULL; }

    jmethodID getCodeCacheDirMethod = env->GetMethodID(contextClass.Get(), "getCodeCacheDir", "()Ljava/io/File;");
    auto fileObj = MakeJniLocalRef(env, getCodeCacheDirMethod ? env->CallObjectMethod(context, getCodeCacheDirMethod) : NULL);
    if (!fileObj.Get()) { ClearJNIException(env); return NULL; }

    auto fileClass = MakeJniLocalRef(env, env->GetObjectClass(fileObj.Get()));
    if(!fileClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID getPathMethod = env->GetMethodID(fileClass.Get(), "getAbsolutePath", "()Ljava/lang/String;");
    auto pathStr = MakeJniLocalRef(env, getPathMethod ? (jstring)env->CallObjectMethod(fileObj.Get(), getPathMethod) : NULL);
    if(!pathStr.Get()) { ClearJNIException(env); return NULL; }
    
    const char* nativePath = env->GetStringUTFChars(pathStr.Get(), NULL);
    if(!nativePath) { ClearJNIException(env); return NULL; }
    DEFER(env->ReleaseStringUTFChars(pathStr.Get(), nativePath));
    std::string cacheDir = nativePath;
    std::string dexFilePath = cacheDir + "/amlInject_" + std::to_string(++dexCount) + ".dex";

    LocalRef<FILE*> fp(fopen(dexFilePath.c_str(), "wb"), [](FILE* file){ if(file) fclose(file); });
    if (!fp.Get()) return NULL;
    fwrite(dexBytes, 1, dexSize, fp.Get());
    fp.Release();

    auto dexLoaderClass = MakeJniLocalRef(env, env->FindClass("dalvik/system/DexClassLoader"));
    if(!dexLoaderClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID dexLoaderCtor = env->GetMethodID(dexLoaderClass.Get(), "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    
    auto dexPathJStr = MakeJniLocalRef(env, env->NewStringUTF(dexFilePath.c_str()));
    auto optPathJStr = MakeJniLocalRef(env, env->NewStringUTF(cacheDir.c_str()));
    
    auto dexLoader = MakeJniLocalRef(env, (dexLoaderCtor && dexPathJStr.Get() && optPathJStr.Get()) ? env->NewObject(dexLoaderClass.Get(), dexLoaderCtor, dexPathJStr.Get(), optPathJStr.Get(), NULL, appClassLoader.Get()) : NULL);

    ClearJNIException(env);

    return dexLoader.Detach();
}

inline jobject InjectSmaliDEX(JNIEnv* env, const uint8_t* dexBytes, size_t dexSize, const char* classToInit)
{
    if(!env || !dexBytes || dexSize < 1 || !classToInit) return NULL;

    static int dexCount = 0;
    
    jobject context = ::GetCurrentContext();
    if (!context) return NULL;

    auto contextClass = MakeJniLocalRef(env, env->GetObjectClass(context));
    if(!contextClass.Get()) { ClearJNIException(env); return NULL; }

    jmethodID getClassLoaderMethod = env->GetMethodID(contextClass.Get(), "getClassLoader", "()Ljava/lang/ClassLoader;");
    auto appClassLoader = MakeJniLocalRef(env, getClassLoaderMethod ? env->CallObjectMethod(context, getClassLoaderMethod) : NULL);
    if (!appClassLoader.Get()) { ClearJNIException(env); return NULL; }

    jmethodID getCodeCacheDirMethod = env->GetMethodID(contextClass.Get(), "getCodeCacheDir", "()Ljava/io/File;");
    auto fileObj = MakeJniLocalRef(env, getCodeCacheDirMethod ? env->CallObjectMethod(context, getCodeCacheDirMethod) : NULL);
    if (!fileObj.Get()) { ClearJNIException(env); return NULL; }

    auto fileClass = MakeJniLocalRef(env, env->GetObjectClass(fileObj.Get()));
    if(!fileClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID getPathMethod = env->GetMethodID(fileClass.Get(), "getAbsolutePath", "()Ljava/lang/String;");
    auto pathStr = MakeJniLocalRef(env, getPathMethod ? (jstring)env->CallObjectMethod(fileObj.Get(), getPathMethod) : NULL);
    if(!pathStr.Get()) { ClearJNIException(env); return NULL; }
    
    const char* nativePath = env->GetStringUTFChars(pathStr.Get(), NULL);
    if(!nativePath) { ClearJNIException(env); return NULL; }
    DEFER(env->ReleaseStringUTFChars(pathStr.Get(), nativePath));
    std::string cacheDir = nativePath;
    std::string dexFilePath = cacheDir + "/amlInject_" + std::to_string(++dexCount) + ".dex";

    LocalRef<FILE*> fp(fopen(dexFilePath.c_str(), "wb"), [](FILE* file){ if(file) fclose(file); });
    if (!fp.Get()) return NULL;
    fwrite(dexBytes, 1, dexSize, fp.Get());
    fp.Release();

    auto dexLoaderClass = MakeJniLocalRef(env, env->FindClass("dalvik/system/DexClassLoader"));
    if(!dexLoaderClass.Get()) { ClearJNIException(env); return NULL; }
    jmethodID dexLoaderCtor = env->GetMethodID(dexLoaderClass.Get(), "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    
    auto dexPathJStr = MakeJniLocalRef(env, env->NewStringUTF(dexFilePath.c_str()));
    auto optPathJStr = MakeJniLocalRef(env, env->NewStringUTF(cacheDir.c_str()));
    
    auto dexLoader = MakeJniLocalRef(env, (dexLoaderCtor && dexPathJStr.Get() && optPathJStr.Get()) ? env->NewObject(dexLoaderClass.Get(), dexLoaderCtor, dexPathJStr.Get(), optPathJStr.Get(), NULL, appClassLoader.Get()) : NULL);
    jobject instance = NULL;

    if (/*classToInit != NULL*/ dexLoader.Get())
    {
        jmethodID loadClassMethod = env->GetMethodID(dexLoaderClass.Get(), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        auto classNameJStr = MakeJniLocalRef(env, env->NewStringUTF(classToInit));
        
        auto loadedClass = MakeJniLocalRef(env, (loadClassMethod && classNameJStr.Get()) ? (jclass)env->CallObjectMethod(dexLoader.Get(), loadClassMethod, classNameJStr.Get()) : NULL);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (loadedClass.Get())
        {
            jmethodID classCtor = env->GetMethodID(loadedClass.Get(), "<init>", "()V");
            if (classCtor)
            {
                auto localInstance = MakeJniLocalRef(env, env->NewObject(loadedClass.Get(), classCtor));
                instance = localInstance.Get() ? env->NewGlobalRef(localInstance.Get()) : NULL;
                g_InjectedInstances[classToInit] = instance;
            }
        }
    }

    ClearJNIException(env);
    
    return instance;
}

#endif // __JNIFN_H
