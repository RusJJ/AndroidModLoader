#include <jni.h>

extern JavaVM *g_pJavaVM;

inline jobject GetGlobalContext(JNIEnv *env)
{
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread = env->GetStaticMethodID(activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return context;
}

inline jstring GetPackageName(JNIEnv *env, jobject jActivity)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getPackageName", "()Ljava/lang/String;");
    jstring ret = (jstring)env->CallObjectMethod(jActivity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

inline jobject GetFilesDir(JNIEnv *env, jobject jActivity)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getFilesDir", "()Ljava/io/File;");
    jstring ret = (jstring)env->CallObjectMethod(jActivity, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

inline jstring GetAbsolutePath(JNIEnv *env, jobject jFile)
{
    jmethodID method = env->GetMethodID(env->GetObjectClass(jFile), "getAbsolutePath", "()Ljava/lang/String;");
    jstring ret = (jstring)env->CallObjectMethod(jFile, method);
    if (env->ExceptionCheck()) env->ExceptionClear();
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
    jmethodID method = env->GetMethodID(env->GetObjectClass(jActivity), "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    jstring ret = (jstring)env->CallObjectMethod(jActivity, method, NULL);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

// fastman92
#ifdef FASTMAN92_CODE
inline bool GetExternalFilesDir_FLA(JNIEnv* env, jobject context, char* strPath, size_t bufferSize)
{
    jobject objectFile;
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
    }
        
    jclass classFile = env->GetObjectClass(objectFile);

    jmethodID methodIDgetAbsolutePath = env->GetMethodID(classFile, "getAbsolutePath", "()Ljava/lang/String;");
    jstring stringPath = (jstring)env->CallObjectMethod(objectFile, methodIDgetAbsolutePath);

    if (stringPath)
    {
        const char* strPathValueStr = env->GetStringUTFChars(stringPath, NULL);

        strxcpy(strPath, strPathValueStr, bufferSize);
        env->ReleaseStringUTFChars(stringPath, strPathValueStr);
    }
    return bReadFromF92launcher;
}
#endif

inline jobject GetStorageDir(JNIEnv* env) // /storage/emulated/0 instead of /sdcard (example)
{
    jclass classEnvironment = env->FindClass("android/os/Environment");
    jstring ret = (jstring)env->CallStaticObjectMethod(classEnvironment, env->GetStaticMethodID(classEnvironment, "getExternalStorageDirectory", "()Ljava/io/File;"));
    if (env->ExceptionCheck()) env->ExceptionClear();
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

    if (env->ExceptionCheck()) env->ExceptionClear();
}