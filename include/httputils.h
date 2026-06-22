#ifndef __HTTPUTILS_H
#define __HTTPUTILS_H

#include <mod/logger.h>

#include <jni.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <jnifn.h>

extern int g_nLatestDownloadErrorCode;

struct JHTTPUtils
{
    inline static bool g_bInit = false;
    inline static jclass g_clsURL = NULL;
    inline static jclass g_clsURLConnection = NULL;
    inline static jclass g_clsHttpURLConnection = NULL;
    inline static jclass g_clsInputStream = NULL;
    inline static jclass g_clsOutputStream = NULL;
    inline static jclass g_clsFileOutputStream = NULL;
    inline static jclass g_clsFile = NULL;
    inline static jclass g_clsStrictMode = NULL;
    inline static jmethodID g_midGetThreadPolicy = NULL;
    inline static jmethodID g_midSetThreadPolicy = NULL;
    inline static jobject g_laxPolicy = NULL;

    inline static jmethodID g_midURLCtor = NULL;
    inline static jmethodID g_midOpenConnection = NULL;
    inline static jmethodID g_midSetConnectTimeout = NULL;
    inline static jmethodID g_midSetReadTimeout = NULL;
    inline static jmethodID g_midSetUseCaches = NULL;
    inline static jmethodID g_midSetRequestProperty = NULL;
    inline static jmethodID g_midConnect = NULL;
    inline static jmethodID g_midGetInputStream = NULL;
    inline static jmethodID g_midDisconnect = NULL;
    inline static jmethodID g_midSetInstanceFollowRedirects = NULL;
    inline static jmethodID g_midGetResponseCode = NULL;
    inline static jmethodID g_midRead = NULL;
    inline static jmethodID g_midCloseInput = NULL;
    inline static jmethodID g_midGetErrorStream = NULL;
    inline static jmethodID g_midGetHeaderField = NULL;
    inline static jmethodID g_midFileOutputCtor = NULL;
    inline static jmethodID g_midWrite = NULL;
    inline static jmethodID g_midFlush = NULL;
    inline static jmethodID g_midCloseOutput = NULL;
    inline static jmethodID g_midFileCtor = NULL;
    inline static jmethodID g_midFileDelete = NULL;
    inline static jmethodID g_midFileRenameTo = NULL;

    inline static bool Initialise()
    {
        if(g_bInit) return true;

        JNIEnv* env = GetCurrentJNI();
        if(!env) return false;

        jclass clsURL = env->FindClass("java/net/URL");
        jclass clsURLConnection = env->FindClass("java/net/URLConnection");
        jclass clsHttpURLConnection = env->FindClass("java/net/HttpURLConnection");
        jclass clsInputStream = env->FindClass("java/io/InputStream");
        jclass clsOutputStream = env->FindClass("java/io/OutputStream");
        jclass clsFileOutputStream = env->FindClass("java/io/FileOutputStream");
        jclass clsFile = env->FindClass("java/io/File");

        if(env->ExceptionCheck()) env->ExceptionClear();
        if(!clsURL || !clsURLConnection || !clsHttpURLConnection || !clsInputStream || !clsOutputStream || !clsFileOutputStream || !clsFile) return false;

        g_clsURL = (jclass)env->NewGlobalRef(clsURL);
        g_clsURLConnection = (jclass)env->NewGlobalRef(clsURLConnection);
        g_clsHttpURLConnection = (jclass)env->NewGlobalRef(clsHttpURLConnection);
        g_clsInputStream = (jclass)env->NewGlobalRef(clsInputStream);
        g_clsOutputStream = (jclass)env->NewGlobalRef(clsOutputStream);
        g_clsFileOutputStream = (jclass)env->NewGlobalRef(clsFileOutputStream);
        g_clsFile = (jclass)env->NewGlobalRef(clsFile);

        env->DeleteLocalRef(clsFile);
        env->DeleteLocalRef(clsFileOutputStream);
        env->DeleteLocalRef(clsOutputStream);
        env->DeleteLocalRef(clsInputStream);
        env->DeleteLocalRef(clsHttpURLConnection);
        env->DeleteLocalRef(clsURLConnection);
        env->DeleteLocalRef(clsURL);

        if(env->ExceptionCheck()) env->ExceptionClear();
        if(!g_clsURL || !g_clsURLConnection || !g_clsHttpURLConnection || !g_clsInputStream || !g_clsOutputStream || !g_clsFileOutputStream || !g_clsFile) return false;

        g_midURLCtor = env->GetMethodID(g_clsURL, "<init>", "(Ljava/lang/String;)V");
        g_midOpenConnection = env->GetMethodID(g_clsURL, "openConnection", "()Ljava/net/URLConnection;");
        g_midSetConnectTimeout = env->GetMethodID(g_clsURLConnection, "setConnectTimeout", "(I)V");
        g_midSetReadTimeout = env->GetMethodID(g_clsURLConnection, "setReadTimeout", "(I)V");
        g_midSetUseCaches = env->GetMethodID(g_clsURLConnection, "setUseCaches", "(Z)V");
        g_midSetRequestProperty = env->GetMethodID(g_clsURLConnection, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
        g_midConnect = env->GetMethodID(g_clsURLConnection, "connect", "()V");
        g_midGetInputStream = env->GetMethodID(g_clsURLConnection, "getInputStream", "()Ljava/io/InputStream;");
        g_midDisconnect = env->GetMethodID(g_clsHttpURLConnection, "disconnect", "()V");
        g_midSetInstanceFollowRedirects = env->GetMethodID(g_clsHttpURLConnection, "setInstanceFollowRedirects", "(Z)V");
        g_midGetResponseCode = env->GetMethodID(g_clsHttpURLConnection, "getResponseCode", "()I");
        g_midRead = env->GetMethodID(g_clsInputStream, "read", "([B)I");
        g_midCloseInput = env->GetMethodID(g_clsInputStream, "close", "()V");
        g_midGetErrorStream = env->GetMethodID(g_clsHttpURLConnection, "getErrorStream", "()Ljava/io/InputStream;");
        g_midGetHeaderField = env->GetMethodID(g_clsURLConnection, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;");
        g_midFileOutputCtor = env->GetMethodID(g_clsFileOutputStream, "<init>", "(Ljava/lang/String;Z)V");
        g_midWrite = env->GetMethodID(g_clsOutputStream, "write", "([BII)V");
        g_midFlush = env->GetMethodID(g_clsOutputStream, "flush", "()V");
        g_midCloseOutput = env->GetMethodID(g_clsOutputStream, "close", "()V");
        g_midFileCtor = env->GetMethodID(g_clsFile, "<init>", "(Ljava/lang/String;)V");
        g_midFileDelete = env->GetMethodID(g_clsFile, "delete", "()Z");
        g_midFileRenameTo = env->GetMethodID(g_clsFile, "renameTo", "(Ljava/io/File;)Z");

        if(env->ExceptionCheck()) env->ExceptionClear();
        if(!g_midURLCtor || !g_midOpenConnection || !g_midSetConnectTimeout || !g_midSetReadTimeout || !g_midSetUseCaches || !g_midSetRequestProperty ||
           !g_midConnect || !g_midGetInputStream || !g_midDisconnect || !g_midSetInstanceFollowRedirects || !g_midGetResponseCode || !g_midRead ||
           !g_midCloseInput || !g_midGetErrorStream || !g_midGetHeaderField || !g_midFileOutputCtor ||
           !g_midWrite || !g_midFlush || !g_midCloseOutput || !g_midFileCtor || !g_midFileDelete || !g_midFileRenameTo) return false;

        {
            jclass clsSM = env->FindClass("android/os/StrictMode");
            jclass clsTP = env->FindClass("android/os/StrictMode$ThreadPolicy");
            if(env->ExceptionCheck()) env->ExceptionClear();
            if(clsSM && clsTP)
            {
                g_clsStrictMode = (jclass)env->NewGlobalRef(clsSM);
                g_midGetThreadPolicy = env->GetStaticMethodID(clsSM, "getThreadPolicy", "()Landroid/os/StrictMode$ThreadPolicy;");
                g_midSetThreadPolicy = env->GetStaticMethodID(clsSM, "setThreadPolicy", "(Landroid/os/StrictMode$ThreadPolicy;)V");
                jfieldID fLax = env->GetStaticFieldID(clsTP, "LAX", "Landroid/os/StrictMode$ThreadPolicy;");
                if(env->ExceptionCheck()) env->ExceptionClear();
                if(fLax)
                {
                    jobject lax = env->GetStaticObjectField(clsTP, fLax);
                    if(lax) { g_laxPolicy = env->NewGlobalRef(lax); env->DeleteLocalRef(lax); }
                }
            }
            if(clsTP) env->DeleteLocalRef(clsTP);
            if(clsSM) env->DeleteLocalRef(clsSM);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }

        g_bInit = true;
        return true;
    }

    inline static jobject RelaxThreadPolicy(JNIEnv* env)
    {
        jobject jOld = env->CallStaticObjectMethod(g_clsStrictMode, g_midGetThreadPolicy);
        if(env->ExceptionCheck()) { env->ExceptionClear(); return NULL; }
        env->CallStaticVoidMethod(g_clsStrictMode, g_midSetThreadPolicy, g_laxPolicy);
        if(env->ExceptionCheck()) env->ExceptionClear();
        return jOld;
    }

    inline static void RestoreThreadPolicy(JNIEnv* env, jobject jOld)
    {
        if(jOld && g_clsStrictMode && g_midSetThreadPolicy)
        {
            env->CallStaticVoidMethod(g_clsStrictMode, g_midSetThreadPolicy, jOld);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        if(jOld) env->DeleteLocalRef(jOld);
    }

    inline static bool LogPendingException(JNIEnv* env, const char* where)
    {
        if(!env->ExceptionCheck()) return false;
        jthrowable ex = env->ExceptionOccurred();
        env->ExceptionClear();
        char buf[600];
        if(ex)
        {
            jclass c = env->GetObjectClass(ex);
            jmethodID m = c ? env->GetMethodID(c, "toString", "()Ljava/lang/String;") : NULL;
            jstring js = m ? (jstring)env->CallObjectMethod(ex, m) : NULL;
            if(env->ExceptionCheck()) env->ExceptionClear();
            if(js)
            {
                const char* s = env->GetStringUTFChars(js, NULL);
                snprintf(buf, sizeof(buf), "httputils.h: EXC @%s -> %s", where, s ? s : "(null)");
                if(s) env->ReleaseStringUTFChars(js, s);
                env->DeleteLocalRef(js);
            }
            else snprintf(buf, sizeof(buf), "httputils.h: EXC @%s (no message)", where);
            if(c) env->DeleteLocalRef(c);
            env->DeleteLocalRef(ex);
        }
        else snprintf(buf, sizeof(buf), "httputils.h: EXC @%s (pending but null)", where);
        logger->Info("%s", buf);
        return true;
    }

    inline static jobject OpenConnected(JNIEnv* env, const char* url, int timeout, const char* useragent, jint* pResponseCode)
    {
        *pResponseCode = 0;

        char szURL[2048];
        if(snprintf(szURL, sizeof(szURL), "%s", url) >= (int)sizeof(szURL)) return NULL;

        jstring jUAKey = NULL;
        jstring jUAValue = NULL;
        if(useragent && useragent[0])
        {
            jUAKey = env->NewStringUTF("User-Agent");
            jUAValue = env->NewStringUTF(useragent);
            if(env->ExceptionCheck()) { env->ExceptionClear(); jUAKey = NULL; jUAValue = NULL; }
        }

        jstring jLocKey = env->NewStringUTF("Location");
        if(env->ExceptionCheck() || !jLocKey)
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            if(jUAValue) env->DeleteLocalRef(jUAValue);
            if(jUAKey) env->DeleteLocalRef(jUAKey);
            return NULL;
        }

        jobject jConnection = NULL;
        const int kMaxRedirects = 10;

        for(int nHop = 0; nHop <= kMaxRedirects; nHop++)
        {
            jstring jURLString = env->NewStringUTF(szURL);
            if(env->ExceptionCheck() || !jURLString) { LogPendingException(env, "NewStringUTF"); break; }

            jobject jURL = env->NewObject(g_clsURL, g_midURLCtor, jURLString);
            env->DeleteLocalRef(jURLString);
            if(env->ExceptionCheck() || !jURL) { LogPendingException(env, "new URL (MalformedURL?)"); break; }

            jConnection = env->CallObjectMethod(jURL, g_midOpenConnection);
            env->DeleteLocalRef(jURL);
            if(env->ExceptionCheck() || !jConnection) { LogPendingException(env, "openConnection"); jConnection = NULL; break; }

            jint nTimeoutMs = (timeout > 0) ? (jint)(timeout * 1000) : 0;
            env->CallVoidMethod(jConnection, g_midSetConnectTimeout, nTimeoutMs);
            env->CallVoidMethod(jConnection, g_midSetReadTimeout, nTimeoutMs);
            env->CallVoidMethod(jConnection, g_midSetUseCaches, JNI_FALSE);

            bool bHttp = env->IsInstanceOf(jConnection, g_clsHttpURLConnection);
            if(bHttp) env->CallVoidMethod(jConnection, g_midSetInstanceFollowRedirects, JNI_TRUE);
            if(jUAKey && jUAValue) env->CallVoidMethod(jConnection, g_midSetRequestProperty, jUAKey, jUAValue);

            env->CallVoidMethod(jConnection, g_midConnect);
            if(env->ExceptionCheck()) { LogPendingException(env, "connect"); env->DeleteLocalRef(jConnection); jConnection = NULL; break; }

            if(!bHttp)
            {
                *pResponseCode = 200;
                break;
            }

            jint nCode = env->CallIntMethod(jConnection, g_midGetResponseCode);
            if(env->ExceptionCheck()) { LogPendingException(env, "getResponseCode"); env->DeleteLocalRef(jConnection); jConnection = NULL; break; }

            if(nCode >= 300 && nCode < 400 && nHop < kMaxRedirects)
            {
                jstring jLoc = (jstring)env->CallObjectMethod(jConnection, g_midGetHeaderField, jLocKey);
                if(env->ExceptionCheck()) { env->ExceptionClear(); jLoc = NULL; }

                bool bFollow = false;
                if(jLoc)
                {
                    const char* pszLoc = env->GetStringUTFChars(jLoc, NULL);
                    if(pszLoc)
                    {
                        if(strstr(pszLoc, "://") && snprintf(szURL, sizeof(szURL), "%s", pszLoc) < (int)sizeof(szURL))
                        {
                            bFollow = true;
                        }
                        env->ReleaseStringUTFChars(jLoc, pszLoc);
                    }
                    env->DeleteLocalRef(jLoc);
                }

                env->CallVoidMethod(jConnection, g_midDisconnect);
                if(env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(jConnection);
                jConnection = NULL;

                if(bFollow) continue;
                break;
            }
            *pResponseCode = nCode;
            break;
        }

        env->DeleteLocalRef(jLocKey);
        if(jUAValue) env->DeleteLocalRef(jUAValue);
        if(jUAKey) env->DeleteLocalRef(jUAKey);
        return jConnection;
    }

    inline static bool DownloadFile(const char* url, const char* pathToSave, int timeout, const char* useragent, bool overwrite)
    {
        if(!Initialise() || !url || !url[0] || !pathToSave || !pathToSave[0]) return false;
        if(!overwrite && access(pathToSave, F_OK) == 0) return false;
        if(timeout < 0) timeout = 0;

        JNIEnv* env = GetCurrentJNI();
        if(!env) return false;

        FILE* file = fopen(pathToSave, "rb");
        if(file)
        {
            fclose(file);
            if(!overwrite) return false;
        }

        bool bSuccess = false;
        g_nLatestDownloadErrorCode = 0;
        jstring jURLString = NULL;
        jstring jPathString = NULL;
        jstring jTempPathString = NULL;
        jstring jUserAgentKey = NULL;
        jstring jUserAgentValue = NULL;
        jobject jURL = NULL;
        jobject jConnection = NULL;
        jobject jInputStream = NULL;
        jobject jErrorStream = NULL;
        jobject jOutputStream = NULL;
        jobject jTempFile = NULL;
        jobject jFinalFile = NULL;
        jbyteArray jBuffer = NULL;
        jobject jOldPolicy = NULL;
        char szTempPath[1024];
        szTempPath[0] = 0;

        int nTempLen = snprintf(szTempPath, sizeof(szTempPath), "%s.part", pathToSave);
        if(nTempLen <= 0 || nTempLen >= (int)sizeof(szTempPath)) goto cleanup;

        unlink(szTempPath);

        jPathString = env->NewStringUTF(pathToSave);
        jTempPathString = env->NewStringUTF(szTempPath);
        if(env->ExceptionCheck())
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }

        jOldPolicy = RelaxThreadPolicy(env);
        jConnection = OpenConnected(env, url, timeout, useragent, &g_nLatestDownloadErrorCode);
        if(!jConnection) goto cleanup;

        if(g_nLatestDownloadErrorCode < 200 || g_nLatestDownloadErrorCode >= 300)
        {
            if(env->IsInstanceOf(jConnection, g_clsHttpURLConnection))
            {
                jErrorStream = env->CallObjectMethod(jConnection, g_midGetErrorStream);
                if(env->ExceptionCheck()) env->ExceptionClear();
            }
            goto cleanup;
        }

        jInputStream = env->CallObjectMethod(jConnection, g_midGetInputStream);
        if(env->ExceptionCheck() || !jInputStream)
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }

        jOutputStream = env->NewObject(g_clsFileOutputStream, g_midFileOutputCtor, jTempPathString, JNI_FALSE);
        if(env->ExceptionCheck() || !jOutputStream)
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }

        jBuffer = env->NewByteArray(16384);
        if(env->ExceptionCheck() || !jBuffer)
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }

        while(true)
        {
            jint nRead = env->CallIntMethod(jInputStream, g_midRead, jBuffer);
            if(env->ExceptionCheck())
            {
                env->ExceptionDescribe(); env->ExceptionClear();
                goto cleanup;
            }

            if(nRead < 0) break;
            if(nRead == 0) continue;

            env->CallVoidMethod(jOutputStream, g_midWrite, jBuffer, 0, nRead);
            if(env->ExceptionCheck())
            {
                env->ExceptionDescribe(); env->ExceptionClear();
                goto cleanup;
            }
        }

        env->CallVoidMethod(jOutputStream, g_midFlush);
        if(env->ExceptionCheck())
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }

        env->CallVoidMethod(jOutputStream, g_midCloseOutput);
        if(env->ExceptionCheck())
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }
        env->DeleteLocalRef(jOutputStream);
        jOutputStream = NULL;

        jTempFile = env->NewObject(g_clsFile, g_midFileCtor, jTempPathString);
        jFinalFile = env->NewObject(g_clsFile, g_midFileCtor, jPathString);
        if(env->ExceptionCheck() || !jTempFile || !jFinalFile)
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }

        if(overwrite)
        {
            env->CallBooleanMethod(jFinalFile, g_midFileDelete);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }

        if(!env->CallBooleanMethod(jTempFile, g_midFileRenameTo, jFinalFile))
        {
            if(env->ExceptionCheck()) env->ExceptionClear();
            goto cleanup;
        }
        if(env->ExceptionCheck())
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup;
        }

        bSuccess = true;

    cleanup:
        RestoreThreadPolicy(env, jOldPolicy);
        if(jErrorStream)
        {
            env->CallVoidMethod(jErrorStream, g_midCloseInput);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        if(jInputStream)
        {
            env->CallVoidMethod(jInputStream, g_midCloseInput);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        if(jOutputStream)
        {
            env->CallVoidMethod(jOutputStream, g_midCloseOutput);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        if(jConnection && env->IsInstanceOf(jConnection, g_clsHttpURLConnection))
        {
            env->CallVoidMethod(jConnection, g_midDisconnect);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }

        if(!bSuccess && jTempFile)
        {
            env->CallBooleanMethod(jTempFile, g_midFileDelete);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }

        if(jBuffer) env->DeleteLocalRef(jBuffer);
        if(jFinalFile) env->DeleteLocalRef(jFinalFile);
        if(jTempFile) env->DeleteLocalRef(jTempFile);
        if(jOutputStream) env->DeleteLocalRef(jOutputStream);
        if(jErrorStream) env->DeleteLocalRef(jErrorStream);
        if(jInputStream) env->DeleteLocalRef(jInputStream);
        if(jConnection) env->DeleteLocalRef(jConnection);
        if(jURL) env->DeleteLocalRef(jURL);
        if(jUserAgentValue) env->DeleteLocalRef(jUserAgentValue);
        if(jUserAgentKey) env->DeleteLocalRef(jUserAgentKey);
        if(jTempPathString) env->DeleteLocalRef(jTempPathString);
        if(jPathString) env->DeleteLocalRef(jPathString);
        if(jURLString) env->DeleteLocalRef(jURLString);
        return bSuccess;
    }

    inline static bool DownloadFileToData(const char* url, char* out, size_t outLen, int timeout, const char* useragent)
    {
        if(!Initialise() || !url || !url[0] || !out || !outLen) return false;
        if(timeout < 0) timeout = 0;
        out[0] = 0;

        JNIEnv* env = GetCurrentJNI();
        if(!env) return false;

        bool bSuccess = false;
        size_t nWritten = 0;
        g_nLatestDownloadErrorCode = 0;
        jstring jURLString = NULL;
        jstring jUserAgentKey = NULL;
        jstring jUserAgentValue = NULL;
        jobject jURL = NULL;
        jobject jConnection = NULL;
        jobject jErrorStream = NULL;
        jobject jInputStream = NULL;
        jbyteArray jBuffer = NULL;
        jobject jOldPolicy = RelaxThreadPolicy(env);

        jConnection = OpenConnected(env, url, timeout, useragent, &g_nLatestDownloadErrorCode);
        if(!jConnection) goto cleanup_data;

        if(g_nLatestDownloadErrorCode < 200 || g_nLatestDownloadErrorCode >= 300)
        {
            if(env->IsInstanceOf(jConnection, g_clsHttpURLConnection))
            {
                jErrorStream = env->CallObjectMethod(jConnection, g_midGetErrorStream);
                if(env->ExceptionCheck()) env->ExceptionClear();
            }
            goto cleanup_data;
        }

        jInputStream = env->CallObjectMethod(jConnection, g_midGetInputStream);
        if(env->ExceptionCheck() || !jInputStream)
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup_data;
        }

        jBuffer = env->NewByteArray(16384);
        if(env->ExceptionCheck() || !jBuffer)
        {
            env->ExceptionDescribe(); env->ExceptionClear();
            goto cleanup_data;
        }

        while(true)
        {
            jint nRead = env->CallIntMethod(jInputStream, g_midRead, jBuffer);
            if(env->ExceptionCheck())
            {
                env->ExceptionDescribe(); env->ExceptionClear();
                goto cleanup_data;
            }

            if(nRead < 0) break;
            if(nRead == 0) continue;

            if(nWritten + (size_t)nRead >= outLen)
            {
                goto cleanup_data;
            }

            jboolean bIsCopy = JNI_FALSE;
            jbyte* pBytes = env->GetByteArrayElements(jBuffer, &bIsCopy);
            if(env->ExceptionCheck() || !pBytes)
            {
                env->ExceptionDescribe(); env->ExceptionClear();
                goto cleanup_data;
            }

            memcpy(&out[nWritten], pBytes, (size_t)nRead);
            nWritten += (size_t)nRead;
            env->ReleaseByteArrayElements(jBuffer, pBytes, JNI_ABORT);
        }
        out[nWritten] = 0;
        bSuccess = true;

    cleanup_data:
        RestoreThreadPolicy(env, jOldPolicy);
        if(!bSuccess) out[0] = 0;
        if(jErrorStream)
        {
            env->CallVoidMethod(jErrorStream, g_midCloseInput);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        if(jInputStream)
        {
            env->CallVoidMethod(jInputStream, g_midCloseInput);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        if(jConnection && env->IsInstanceOf(jConnection, g_clsHttpURLConnection))
        {
            env->CallVoidMethod(jConnection, g_midDisconnect);
            if(env->ExceptionCheck()) env->ExceptionClear();
        }

        if(jBuffer) env->DeleteLocalRef(jBuffer);
        if(jErrorStream) env->DeleteLocalRef(jErrorStream);
        if(jInputStream) env->DeleteLocalRef(jInputStream);
        if(jConnection) env->DeleteLocalRef(jConnection);
        if(jURL) env->DeleteLocalRef(jURL);
        if(jUserAgentValue) env->DeleteLocalRef(jUserAgentValue);
        if(jUserAgentKey) env->DeleteLocalRef(jUserAgentKey);
        if(jURLString) env->DeleteLocalRef(jURLString);
        return bSuccess;
    }
};

#endif // __HTTPUTILS_H
