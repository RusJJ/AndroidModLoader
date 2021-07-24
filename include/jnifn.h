#include <jni.h>

inline jobject GetGlobalContext(JNIEnv *env)
{
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);
    return context;
}

inline jobject GetGlobalActivity(JNIEnv *env)
{
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);
    return context;
}

inline jstring GetPackageName(JNIEnv *env, jobject jActivity)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getPackageName", "()Ljava/lang/String;");
    return (jstring)env->CallObjectMethod(jActivity, method);
}

inline jobject GetFilesDir(JNIEnv *env, jobject jActivity)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getFilesDir", "()Ljava/io/File;");
    return (jstring)env->CallObjectMethod(jActivity, method);
}

inline jstring GetAbsolutePath(JNIEnv *env, jobject jFile)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jFile), "getAbsolutePath", "()Ljava/lang/String;");
    return (jstring)env->CallObjectMethod(jFile, method);
}

inline jstring GetAndroidPermission(JNIEnv* env, const char* szPermissionName)
{
    jclass ClassManifestPermission = env->FindClass("android/Manifest$permission");
    jfieldID lid_PERM = env->GetStaticFieldID(ClassManifestPermission, szPermissionName, "Ljava/lang/String;");
    return (jstring)env->GetStaticObjectField(ClassManifestPermission, lid_PERM);
}

inline bool HasPermissionGranted(JNIEnv* env, jobject jActivity, const char* szPermissionName)
{
    bool result = false;

    jstring ls_PERM = GetAndroidPermission(env, szPermissionName);

    jint PERMISSION_GRANTED = jint(-1);

    jclass ClassPackageManager = env->FindClass("android/content/pm/PackageManager");
    jfieldID lid_PERMISSION_GRANTED = env->GetStaticFieldID(ClassPackageManager, "PERMISSION_GRANTED", "I");
    PERMISSION_GRANTED = env->GetStaticIntField(ClassPackageManager, lid_PERMISSION_GRANTED);
    
    jclass ClassContext = env->FindClass("android/content/Context");
    jmethodID MethodcheckSelfPermission = env->GetMethodID(ClassContext, "checkSelfPermission", "(Ljava/lang/String;)I");
    jint int_result = env->CallIntMethod(jActivity, MethodcheckSelfPermission, ls_PERM);

    return (int_result == PERMISSION_GRANTED);
}

inline void RequestPermissions(JNIEnv* env, jobject jActivity)
{
    jobjectArray perm_array = env->NewObjectArray(2, env->FindClass("java/lang/String"), env->NewStringUTF(""));

    env->SetObjectArrayElement(perm_array, 0, GetAndroidPermission(env, "READ_EXTERNAL_STORAGE"));

    env->SetObjectArrayElement(perm_array, 1, GetAndroidPermission(env, "WRITE_EXTERNAL_STORAGE"));

    jclass ClassActivity = env->FindClass("android/app/Activity");

    jmethodID MethodrequestPermissions = env->GetMethodID(ClassActivity, "requestPermissions", "([Ljava/lang/String;I)V");

    env->CallVoidMethod(jActivity, MethodrequestPermissions, perm_array, 0);
}