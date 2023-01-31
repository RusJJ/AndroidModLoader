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
    jclass ClassPackageManager = env->FindClass("android/content/pm/PackageManager");
    //bool result = false;
    jstring ls_PERM = GetAndroidPermission(env, szPermissionName);
    jfieldID lid_PERMISSION_GRANTED = env->GetStaticFieldID(ClassPackageManager, "PERMISSION_GRANTED", "I");
    jint PERMISSION_GRANTED = jint(-1);

    PERMISSION_GRANTED = env->GetStaticIntField(ClassPackageManager, lid_PERMISSION_GRANTED);
    jint int_result = env->CallIntMethod(jActivity, env->GetMethodID(env->FindClass("android/content/Context"), "checkSelfPermission", "(Ljava/lang/String;)I"), ls_PERM);
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
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    return (jstring)env->CallObjectMethod(jActivity, method, NULL);
}

inline jobject GetStorageDir(JNIEnv* env) // /storage/emulated/0 instead of /sdcard (example)
{
    jclass classEnvironment = env->FindClass("android/os/Environment");
    return (jstring)env->CallStaticObjectMethod(classEnvironment, env->GetStaticMethodID(classEnvironment, "getExternalStorageDirectory", "()Ljava/io/File;"));
}
