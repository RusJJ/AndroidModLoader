#include "flogger.h"
#include <mod/amlmod.h>
#include <mod/iaml.h>
#include <jni.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

FLogger::FLogger(const char* szFileName, bool bClearFile) : m_pFile(NULL), m_bClearOnOpen(bClearFile)
{
    m_szFileName[0] = 0;
    if(szFileName && szFileName[0] != 0)
    {
        snprintf(m_szFileName, sizeof(m_szFileName), "%s", szFileName);
    }
}

FLogger::~FLogger()
{
    if(m_pFile)
    {
        fclose(m_pFile);
        m_pFile = NULL;
    }
}

bool FLogger::OpenViaMediaStore()
{
    JNIEnv* env = aml->GetJNIEnvironment();
    jobject ctx = aml->GetAppContextObject();
    if(!env || !ctx) return false;

    char relPath[256];
    char displayName[128];
    const char* lastSlash = strrchr(m_szFileName, '/');
    if(lastSlash)
    {
        size_t subLen = (size_t)(lastSlash - m_szFileName);
        char subDir[128];
        if(subLen >= sizeof(subDir)) subLen = sizeof(subDir) - 1;
        memcpy(subDir, m_szFileName, subLen);
        subDir[subLen] = 0;
        snprintf(relPath, sizeof(relPath), "Documents/AMLModLogs/%s/%s/", modinfo->GUID(), subDir);
        snprintf(displayName, sizeof(displayName), "%s", lastSlash + 1);
    }
    else
    {
        snprintf(relPath, sizeof(relPath), "Documents/AMLModLogs/%s/", modinfo->GUID());
        snprintf(displayName, sizeof(displayName), "%s", m_szFileName);
    }

    jclass ctxCls = env->GetObjectClass(ctx);
    jmethodID getResolver = env->GetMethodID(ctxCls, "getContentResolver", "()Landroid/content/ContentResolver;");
    jobject resolver = env->CallObjectMethod(ctx, getResolver);
    if(env->ExceptionCheck() || !resolver)
    {
        env->ExceptionClear();
        env->DeleteLocalRef(ctxCls);
        return false;
    }

    jclass filesCls = env->FindClass("android/provider/MediaStore$Files");
    jmethodID getContentUri = env->GetStaticMethodID(filesCls, "getContentUri", "(Ljava/lang/String;)Landroid/net/Uri;");
    jstring jExternal = env->NewStringUTF("external");
    jobject collectionUri = env->CallStaticObjectMethod(filesCls, getContentUri, jExternal);

    jclass cvCls = env->FindClass("android/content/ContentValues");
    jmethodID cvInit = env->GetMethodID(cvCls, "<init>", "()V");
    jmethodID cvPut = env->GetMethodID(cvCls, "put", "(Ljava/lang/String;Ljava/lang/String;)V");
    jobject values = env->NewObject(cvCls, cvInit);

    jstring kDisplay = env->NewStringUTF("_display_name");
    jstring vDisplay = env->NewStringUTF(displayName);
    env->CallVoidMethod(values, cvPut, kDisplay, vDisplay);

    jstring kMime = env->NewStringUTF("mime_type");
    jstring vMime = env->NewStringUTF("text/plain");
    env->CallVoidMethod(values, cvPut, kMime, vMime);

    jstring kRelPath = env->NewStringUTF("relative_path");
    jstring vRelPath = env->NewStringUTF(relPath);
    env->CallVoidMethod(values, cvPut, kRelPath, vRelPath);

    jclass resolverCls = env->GetObjectClass(resolver);

    if(m_bClearOnOpen)
    {
        jmethodID deleteMethod = env->GetMethodID(resolverCls, "delete", "(Landroid/net/Uri;Ljava/lang/String;[Ljava/lang/String;)I");
        jstring whereClause = env->NewStringUTF("_display_name=? AND relative_path=?");
        jclass strCls = env->FindClass("java/lang/String");
        jobjectArray whereArgs = env->NewObjectArray(2, strCls, NULL);
        env->SetObjectArrayElement(whereArgs, 0, vDisplay);
        env->SetObjectArrayElement(whereArgs, 1, vRelPath);
        env->CallIntMethod(resolver, deleteMethod, collectionUri, whereClause, whereArgs);
        if(env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(whereArgs);
        env->DeleteLocalRef(strCls);
        env->DeleteLocalRef(whereClause);
    }

    jmethodID insertMethod = env->GetMethodID(resolverCls, "insert", "(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;");
    jobject itemUri = env->CallObjectMethod(resolver, insertMethod, collectionUri, values);
    if(env->ExceptionCheck()) env->ExceptionClear();

    if(itemUri)
    {
        jmethodID openPfd = env->GetMethodID(resolverCls, "openFileDescriptor", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;");
        jstring jMode = env->NewStringUTF(m_bClearOnOpen ? "wt" : "wa");
        jobject pfd = env->CallObjectMethod(resolver, openPfd, itemUri, jMode);
        if(!env->ExceptionCheck() && pfd)
        {
            jclass pfdCls = env->GetObjectClass(pfd);
            jmethodID detachFd = env->GetMethodID(pfdCls, "detachFd", "()I");
            jint fd = env->CallIntMethod(pfd, detachFd);
            if(!env->ExceptionCheck() && fd >= 0)
            {
                m_pFile = fdopen(fd, m_bClearOnOpen ? "w" : "a");
                if(!m_pFile) close(fd);
            }
            env->DeleteLocalRef(pfdCls);
            env->DeleteLocalRef(pfd);
        }
        env->ExceptionClear();
        env->DeleteLocalRef(jMode);
        env->DeleteLocalRef(itemUri);
    }

    env->DeleteLocalRef(vRelPath);
    env->DeleteLocalRef(kRelPath);
    env->DeleteLocalRef(vMime);
    env->DeleteLocalRef(kMime);
    env->DeleteLocalRef(vDisplay);
    env->DeleteLocalRef(kDisplay);
    env->DeleteLocalRef(values);
    env->DeleteLocalRef(cvCls);
    env->DeleteLocalRef(collectionUri);
    env->DeleteLocalRef(jExternal);
    env->DeleteLocalRef(filesCls);
    env->DeleteLocalRef(resolverCls);
    env->DeleteLocalRef(resolver);
    env->DeleteLocalRef(ctxCls);

    if(m_pFile)
    {
        m_bClearOnOpen = false;
        return true;
    }
    return false;
}

bool FLogger::Open()
{
    if(m_pFile) return true;
    if(!aml || !modinfo || m_szFileName[0] == 0) return false;

    const char* root = aml->GetInternalPath();
    if(root && root[0] != 0)
    {
        char fullPath[512];
        size_t len = strlen(root);
        if(root[len - 1] == '/')
        {
            snprintf(fullPath, sizeof(fullPath), "%sDocuments/AMLModLogs/%s/%s", root, modinfo->GUID(), m_szFileName);
        }
        else
        {
            snprintf(fullPath, sizeof(fullPath), "%s/Documents/AMLModLogs/%s/%s", root, modinfo->GUID(), m_szFileName);
        }

        char dirPath[512];
        snprintf(dirPath, sizeof(dirPath), "%s", fullPath);
        char* lastSlash = strrchr(dirPath, '/');
        if(lastSlash)
        {
            *lastSlash = 0;
            aml->CreateDirRecursive(dirPath);
        }

        m_pFile = fopen(fullPath, m_bClearOnOpen ? "w" : "a");
        if(m_pFile)
        {
            m_bClearOnOpen = false;
            return true;
        }
    }
    return OpenViaMediaStore();
}

void FLogger::WriteLine(const char* szPrefix, const char* szMessage, va_list args)
{
    if(!Open()) return;

    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), szMessage, args);

    time_t rawTime = time(NULL);
    struct tm t;
    localtime_r(&rawTime, &t);

    if(szPrefix)
    {
        fprintf(m_pFile, "[%02d:%02d:%02d] [%s] %s\n", t.tm_hour, t.tm_min, t.tm_sec, szPrefix, buffer);
    }
    else
    {
        fprintf(m_pFile, "[%02d:%02d:%02d] %s\n", t.tm_hour, t.tm_min, t.tm_sec, buffer);
    }
    fflush(m_pFile);
}

void FLogger::Info(const char* szMessage, ...)
{
    va_list args;
    va_start(args, szMessage);
    WriteLine("INFO", szMessage, args);
    va_end(args);
}

void FLogger::Error(const char* szMessage, ...)
{
    va_list args;
    va_start(args, szMessage);
    WriteLine("ERROR", szMessage, args);
    va_end(args);
}

void FLogger::Write(const char* szMessage, ...)
{
    va_list args;
    va_start(args, szMessage);
    WriteLine(NULL, szMessage, args);
    va_end(args);
}