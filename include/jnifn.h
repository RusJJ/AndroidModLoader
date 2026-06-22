#ifndef __JNIFN_H
#define __JNIFN_H

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <unordered_map>

extern JavaVM *g_pJavaVM;

JNIEnv* GetCurrentJNI();
jobject GetCurrentContext();
extern std::unordered_map<std::string, jobject> g_InjectedInstances;

inline jobject GetGlobalContext(JNIEnv *env)
{
    if(!env) return NULL;
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    if(!activityThread) { if(env->ExceptionCheck()) env->ExceptionClear(); return NULL; }
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if(!currentActivityThread) { if(env->ExceptionCheck()) env->ExceptionClear(); env->DeleteLocalRef(activityThread); return NULL; }
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    if(env->ExceptionCheck()) env->ExceptionClear();
    if(!at) { env->DeleteLocalRef(activityThread); return NULL; }
    jmethodID getApplication = env->GetMethodID(activityThread, "getApplication", "()Landroid/app/Application;");
    if(!getApplication) { if(env->ExceptionCheck()) env->ExceptionClear(); env->DeleteLocalRef(at); env->DeleteLocalRef(activityThread); return NULL; }
    jobject context = env->CallObjectMethod(at, getApplication);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(at);
    env->DeleteLocalRef(activityThread);
    return context;
}

inline jstring GetPackageName(JNIEnv *env, jobject jActivity)
{
    if(!env || !jActivity) return NULL;
    jclass activityClass = env->GetObjectClass(jActivity);
    if(!activityClass) { if(env->ExceptionCheck()) env->ExceptionClear(); return NULL; }
    jmethodID method = env->GetMethodID(activityClass, "getPackageName", "()Ljava/lang/String;");
    if(!method) { if(env->ExceptionCheck()) env->ExceptionClear(); env->DeleteLocalRef(activityClass); return NULL; }
    jstring ret = (jstring)env->CallObjectMethod(jActivity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(activityClass);
    return ret;
}

inline jobject GetFilesDir(JNIEnv *env, jobject jActivity)
{
    if(!env || !jActivity) return NULL;
    jclass activityClass = env->GetObjectClass(jActivity);
    if(!activityClass) { if(env->ExceptionCheck()) env->ExceptionClear(); return NULL; }
    jmethodID method = env->GetMethodID(activityClass, "getFilesDir", "()Ljava/io/File;");
    if(!method) { if(env->ExceptionCheck()) env->ExceptionClear(); env->DeleteLocalRef(activityClass); return NULL; }
    jobject ret = env->CallObjectMethod(jActivity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(activityClass);
    return ret;
}

inline jstring GetAbsolutePath(JNIEnv *env, jobject jFile)
{
    if(!env || !jFile) return NULL;
    jclass fileClass = env->GetObjectClass(jFile);
    if(!fileClass) { if(env->ExceptionCheck()) env->ExceptionClear(); return NULL; }
    jmethodID method = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    if(!method) { if(env->ExceptionCheck()) env->ExceptionClear(); env->DeleteLocalRef(fileClass); return NULL; }
    jstring ret = (jstring)env->CallObjectMethod(jFile, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(fileClass);
    return ret;
}

inline jstring GetAndroidPermission(JNIEnv* env, const char* szPermissionName)
{
    jclass ClassManifestPermission = env->FindClass("android/Manifest$permission");
    jfieldID lid_PERM = env->GetStaticFieldID(ClassManifestPermission, szPermissionName, "Ljava/lang/String;");
    jstring ret = (jstring)env->GetStaticObjectField(ClassManifestPermission, lid_PERM);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

inline bool HasPermissionGranted(JNIEnv* env, jobject jActivity, const char* szPermissionName)
{
    jclass ClassPackageManager = env->FindClass("android/content/pm/PackageManager");
    //bool result = false;
    jstring ls_PERM = GetAndroidPermission(env, szPermissionName);
    jfieldID lid_PERMISSION_GRANTED = env->GetStaticFieldID(ClassPackageManager, "PERMISSION_GRANTED", "I");
    jint PERMISSION_GRANTED = jint(-1);

    PERMISSION_GRANTED = env->GetStaticIntField(ClassPackageManager, lid_PERMISSION_GRANTED);
    jint int_result = env->CallIntMethod(jActivity, env->GetMethodID(env->FindClass("android/content/Context"), "checkSelfPermission", "(Ljava/lang/String;)I"), ls_PERM);
    if (env->ExceptionCheck()) env->ExceptionClear();
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
    jclass activityClass = env->GetObjectClass(jActivity);
    jmethodID method = env->GetMethodID(activityClass, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    jobject ret = env->CallObjectMethod(jActivity, method, NULL);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(activityClass);
    return ret;
}

// fastman92
#ifdef FASTMAN92_CODE
inline bool GetExternalFilesDir_FLA(JNIEnv* env, jobject context, char* strPath, size_t bufferSize)
{
    jobject objectFile = NULL;
    bool bReadFromF92launcher = false;
    jmethodID methodIDgetExternalFilesDir;

    jclass classF92launcherSettings = env->FindClass("com/fastman92/main_activity_launcher/Settings");

    if (classF92launcherSettings)
    {
        methodIDgetExternalFilesDir = env->GetStaticMethodID(classF92launcherSettings, "getExternalFilesDir", "(Landroid/content/Context;)Ljava/io/File;");
        
        if (methodIDgetExternalFilesDir)
        {
            objectFile = env->CallStaticObjectMethod(classF92launcherSettings, methodIDgetExternalFilesDir, context);
            bReadFromF92launcher = true;
        }
    }

    if (!bReadFromF92launcher)
    {
        if (env->ExceptionCheck()) env->ExceptionClear();

        jclass android_content_Context = env->GetObjectClass(context);

        methodIDgetExternalFilesDir = env->GetMethodID(android_content_Context, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");

        objectFile = (jstring)env->CallObjectMethod(context, methodIDgetExternalFilesDir, nullptr);
        env->DeleteLocalRef(android_content_Context);
    }

    if(!objectFile)
    {
        if(classF92launcherSettings) env->DeleteLocalRef(classF92launcherSettings);
        return false;
    }
        
    jclass classFile = env->GetObjectClass(objectFile);

    jmethodID methodIDgetAbsolutePath = env->GetMethodID(classFile, "getAbsolutePath", "()Ljava/lang/String;");
    jstring stringPath = (jstring)env->CallObjectMethod(objectFile, methodIDgetAbsolutePath);

    if (stringPath)
    {
        const char* strPathValueStr = env->GetStringUTFChars(stringPath, NULL);

        strxcpy(strPath, strPathValueStr, bufferSize);
        env->ReleaseStringUTFChars(stringPath, strPathValueStr);
        env->DeleteLocalRef(stringPath);
    }
    env->DeleteLocalRef(classFile);
    env->DeleteLocalRef(objectFile);
    if(classF92launcherSettings) env->DeleteLocalRef(classF92launcherSettings);
    return bReadFromF92launcher;
}
#endif

inline jobject GetStorageDir(JNIEnv* env) // /storage/emulated/0 instead of /sdcard (example)
{
    jclass classEnvironment = env->FindClass("android/os/Environment");
    jobject ret = env->CallStaticObjectMethod(classEnvironment, env->GetStaticMethodID(classEnvironment, "getExternalStorageDirectory", "()Ljava/io/File;"));
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(classEnvironment);
    return ret;
}

inline void ShowToastMessage(JNIEnv* env, jobject jActivity, const char* txt, int msDuration)
{
    jclass ToastClass = env->FindClass("android/widget/Toast");
    jmethodID makeTextMethodID = env->GetStaticMethodID(ToastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
    jmethodID showMethodID = env->GetMethodID(ToastClass, "show", "()V");

    jstring message = env->NewStringUTF(txt);
    jint duration = msDuration; // can be Toast.LENGTH_SHORT or Toast.LENGTH_LONG

    jobject toast = env->CallStaticObjectMethod(ToastClass, makeTextMethodID, jActivity, message, duration);
    env->CallVoidMethod(toast, showMethodID);
    
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(toast);
    env->DeleteLocalRef(ToastClass);

    if (env->ExceptionCheck()) env->ExceptionClear();
}

inline void ShowToastMessage2(JNIEnv* env, jobject jActivity, const char* txt, jint duration)
{
    jclass ToastClass = env->FindClass("android/widget/Toast");
    jmethodID makeTextMethodID = env->GetStaticMethodID(ToastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
    jmethodID showMethodID = env->GetMethodID(ToastClass, "show", "()V");
    jmethodID setDurationMethodID = env->GetMethodID(ToastClass, "setDuration", "(I)V");

    jstring message = env->NewStringUTF(txt);
    jobject toast = env->CallStaticObjectMethod(ToastClass, makeTextMethodID, jActivity, message, duration);
    env->CallVoidMethod(toast, setDurationMethodID, duration);
    env->CallVoidMethod(toast, showMethodID);

    env->DeleteLocalRef(message);
    env->DeleteLocalRef(toast);
    env->DeleteLocalRef(ToastClass);

    if (env->ExceptionCheck()) env->ExceptionClear();
}

inline jstring GetNativeLibDir(JNIEnv* env)
{
    jclass contextClass = env->GetObjectClass(::GetCurrentContext());

    jmethodID getAppInfoId = env->GetMethodID(contextClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    jobject appInfo = env->CallObjectMethod(::GetCurrentContext(), getAppInfoId);

    jclass appInfoClass = env->GetObjectClass(appInfo);
    jfieldID nativeLibDirField = env->GetFieldID(appInfoClass, "nativeLibraryDir", "Ljava/lang/String;");
    jstring nativeLibDir = (jstring)env->GetObjectField(appInfo, nativeLibDirField);

    env->DeleteLocalRef(appInfo);
    env->DeleteLocalRef(appInfoClass);
    env->DeleteLocalRef(contextClass);

    if (env->ExceptionCheck()) env->ExceptionClear();

    return nativeLibDir;
}

inline AAssetManager* GetAssetManager(JNIEnv* env)
{
    jclass contextClass = env->GetObjectClass(::GetCurrentContext());
    jmethodID getAssetsMethod = env->GetMethodID(contextClass, "getAssets", "()Landroid/content/res/AssetManager;");
    
    jobject javaAssetManager = env->CallObjectMethod(::GetCurrentContext(), getAssetsMethod);
    
    if (env->ExceptionCheck()) env->ExceptionClear();

    AAssetManager* assetManager = AAssetManager_fromJava(env, javaAssetManager);
    env->DeleteLocalRef(javaAssetManager);
    env->DeleteLocalRef(contextClass);
    return assetManager;
}

inline jobject LoadSmaliDEX(JNIEnv* env, const uint8_t* dexBytes, size_t dexSize)
{
    static int dexCount = 0;
    
    jobject context = ::GetCurrentContext();
    if (!context) return NULL;

    jclass contextClass = env->GetObjectClass(context);

    jmethodID getClassLoaderMethod = env->GetMethodID(contextClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject appClassLoader = env->CallObjectMethod(context, getClassLoaderMethod);
    if (!appClassLoader) return NULL;

    jmethodID getCodeCacheDirMethod = env->GetMethodID(contextClass, "getCodeCacheDir", "()Ljava/io/File;");
    jobject fileObj = env->CallObjectMethod(context, getCodeCacheDirMethod);
    if (!fileObj) return NULL;

    jclass fileClass = env->GetObjectClass(fileObj);
    jmethodID getPathMethod = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring pathStr = (jstring)env->CallObjectMethod(fileObj, getPathMethod);
    
    const char* nativePath = env->GetStringUTFChars(pathStr, NULL);
    std::string cacheDir = nativePath;
    std::string dexFilePath = cacheDir + "/amlInject_" + std::to_string(++dexCount) + ".dex";
    env->ReleaseStringUTFChars(pathStr, nativePath);

    FILE* fp = fopen(dexFilePath.c_str(), "wb");
    if (!fp) return NULL;
    fwrite(dexBytes, 1, dexSize, fp);
    fclose(fp);

    jclass dexLoaderClass = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID dexLoaderCtor = env->GetMethodID(dexLoaderClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    
    jstring dexPathJStr = env->NewStringUTF(dexFilePath.c_str());
    jstring optPathJStr = env->NewStringUTF(cacheDir.c_str());
    
    jobject dexLoader = env->NewObject(dexLoaderClass, dexLoaderCtor, dexPathJStr, optPathJStr, NULL, appClassLoader);

    env->DeleteLocalRef(contextClass);
    env->DeleteLocalRef(appClassLoader);
    env->DeleteLocalRef(fileObj);
    env->DeleteLocalRef(fileClass);
    env->DeleteLocalRef(pathStr);
    env->DeleteLocalRef(dexLoaderClass);
    env->DeleteLocalRef(dexPathJStr);
    env->DeleteLocalRef(optPathJStr);

    return dexLoader;
}

inline jobject InjectSmaliDEX(JNIEnv* env, const uint8_t* dexBytes, size_t dexSize, const char* classToInit)
{
    if(!env || !dexBytes || dexSize < 1 || !classToInit) return NULL;

    static int dexCount = 0;
    
    jobject context = ::GetCurrentContext();
    if (!context) return NULL;

    jclass contextClass = env->GetObjectClass(context);

    jmethodID getClassLoaderMethod = env->GetMethodID(contextClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject appClassLoader = env->CallObjectMethod(context, getClassLoaderMethod);
    if (!appClassLoader) return NULL;

    jmethodID getCodeCacheDirMethod = env->GetMethodID(contextClass, "getCodeCacheDir", "()Ljava/io/File;");
    jobject fileObj = env->CallObjectMethod(context, getCodeCacheDirMethod);
    if (!fileObj) return NULL;

    jclass fileClass = env->GetObjectClass(fileObj);
    jmethodID getPathMethod = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring pathStr = (jstring)env->CallObjectMethod(fileObj, getPathMethod);
    
    const char* nativePath = env->GetStringUTFChars(pathStr, NULL);
    std::string cacheDir = nativePath;
    std::string dexFilePath = cacheDir + "/amlInject_" + std::to_string(++dexCount) + ".dex";
    env->ReleaseStringUTFChars(pathStr, nativePath);

    FILE* fp = fopen(dexFilePath.c_str(), "wb");
    if (!fp) return NULL;
    fwrite(dexBytes, 1, dexSize, fp);
    fclose(fp);

    jclass dexLoaderClass = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID dexLoaderCtor = env->GetMethodID(dexLoaderClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    
    jstring dexPathJStr = env->NewStringUTF(dexFilePath.c_str());
    jstring optPathJStr = env->NewStringUTF(cacheDir.c_str());
    
    jobject dexLoader = env->NewObject(dexLoaderClass, dexLoaderCtor, dexPathJStr, optPathJStr, NULL, appClassLoader);
    jobject instance = NULL;

    if (/*classToInit != NULL*/ dexLoader)
    {
        jmethodID loadClassMethod = env->GetMethodID(dexLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring classNameJStr = env->NewStringUTF(classToInit);
        
        jclass loadedClass = (jclass)env->CallObjectMethod(dexLoader, loadClassMethod, classNameJStr);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (loadedClass)
        {
            jmethodID classCtor = env->GetMethodID(loadedClass, "<init>", "()V");
            if (classCtor)
            {
                instance = env->NewGlobalRef( env->NewObject(loadedClass, classCtor) );
                g_InjectedInstances[classToInit] = instance;
            }
            env->DeleteLocalRef(loadedClass);
        }
        env->DeleteLocalRef(classNameJStr);
    }

    env->DeleteLocalRef(contextClass);
    env->DeleteLocalRef(appClassLoader);
    env->DeleteLocalRef(fileObj);
    env->DeleteLocalRef(fileClass);
    env->DeleteLocalRef(pathStr);
    env->DeleteLocalRef(dexLoaderClass);
    env->DeleteLocalRef(dexPathJStr);
    env->DeleteLocalRef(optPathJStr);
    env->DeleteLocalRef(dexLoader);
    
    return instance;
}

#endif // __JNIFN_H
