#include <aml.h>
#include <mod/logger.h>
#include <ARMPatch/armpatch_src/ARMPatch.h>
#include <vtable_hooker.h>
#include <modslist.h>
#include <mls.h>
#include <unordered_map>
#include <sstream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <stdio.h>
#include <time.h>
#include <jnifn.h>

#include <Gloss.h>

#include <httputils.h>
#include <cryptutils.h>
JMD5 g_MD5;

char g_szAMLFeatures[2048] = "AML ARMPATCH HOOK CONFIG INTERFACE GLOSS ";
extern char g_szAppName[256], g_szFakeAppName[256], g_szNativeLibPath[512];
extern char g_szCfgPath[256], g_szAndroidDataDir[256], g_szAndroidDataRootDir[256];
extern char g_szInternalStoragePath[256], g_szInternalModsDir[256], g_szNewsString[512];
extern char g_szUserAgent[256];
extern const char* g_szDataDir;
extern jobject appContext;
extern bool g_bEnableFileDownloads, g_bMLSOnlyManualSaves;
extern int g_nDownloadTimeout, g_nFailedToLoad, g_nLatestDownloadErrorCode;
extern pid_t g_MainThreadID;

extern JavaVM *g_pJavaVM;
extern jobject appContext;
JNIEnv* GetCurrentJNI();
jobject GetCurrentContext();
jobject GetCurrentActivity();
AAssetManager* GetCurrentAssetManager();
bool PushToJavaUIThread(void (*fn)(void*), void* data);

inline bool HasFakeAppName()
{
    return (g_szFakeAppName[0] != 0 && strlen(g_szFakeAppName) > 5);
}

const char* AML::GetCurrentGame()
{
    return HasFakeAppName() ? g_szFakeAppName : g_szAppName;
}

const char* AML::GetConfigPath()
{
    return g_szCfgPath;
}

const char* AML::GetDataPath()
{
    return g_szDataDir;
}

const char* AML::GetAndroidDataPath()
{
    return g_szAndroidDataDir;
}

bool AML::HasMod(const char* szGUID)
{
    return modlist->HasMod(szGUID);
}

bool AML::HasModOfVersion(const char* szGUID, const char* szVersion)
{
    return modlist->HasModOfVersion(szGUID, szVersion);
}

uintptr_t AML::GetLib(const char* szLib)
{
    return ARMPatch::GetLib(szLib);
}

uintptr_t AML::GetSym(void* handle, const char* sym)
{
    return ARMPatch::GetSym(handle, sym);
}

uintptr_t AML::GetSym(uintptr_t libAddr, const char* sym)
{
    return ARMPatch::GetSym(libAddr, sym);
}

bool AML::Hook(void* handle, void* fnAddress, void** orgFnAddress)
{
    return ARMPatch::hookInternal(handle, fnAddress, orgFnAddress);
}

bool AML::HookPLT(void* handle, void* fnAddress, void** orgFnAddress)
{
    return ARMPatch::hookPLTInternal(handle, fnAddress, orgFnAddress);
}

int AML::Unprot(uintptr_t handle, size_t len)
{
    return ARMPatch::Unprotect(handle, len);
}

void AML::Write(uintptr_t dest, uintptr_t src, size_t size)
{
    ARMPatch::Write(dest, src, size);
}

void AML::Read(uintptr_t src, uintptr_t dest, size_t size)
{
    ARMPatch::Read(src, dest, size);
}

int AML::PlaceNOP(uintptr_t addr, size_t count)
{
    return ARMPatch::WriteNOP(addr, count);
}

int AML::PlaceJMP(uintptr_t addr, uintptr_t dest)
{
    return ARMPatch::WriteB(addr, dest);
}

int AML::PlaceRET(uintptr_t addr)
{
    return ARMPatch::WriteRET(addr);
}

uintptr_t AML::GetLibLength(const char* szLib)
{
    return ARMPatch::GetLibLength(szLib);
}

int AML::Redirect(uintptr_t addr, uintptr_t to)
{
    return ARMPatch::Redirect(addr, to);
}

void AML::PlaceBL(uintptr_t addr, uintptr_t dest)
{
    ARMPatch::WriteBL(addr, dest);
}

void AML::PlaceBLX(uintptr_t addr, uintptr_t dest)
{
#ifdef AML32
    ARMPatch::WriteBLX(addr, dest);
#else // AML64
    ARMPatch::WriteBL(addr, dest);
#endif
}

static std::vector<int> ParsePattern(const char* pattern)
{
    std::vector<int> bytes;
    std::stringstream ss(pattern);
    std::string hex;
    while(ss >> hex)
    {
        if(hex == "?" || hex == "??")
        {
            bytes.push_back(-1);
        }
        else
        {
            bytes.push_back((int)(std::stoul(hex, nullptr, 16)));
        }
    }
    return bytes;
}
static bool ComparePattern(const uint8_t* data, const std::vector<int>& parsedPattern)
{
    for(size_t i = 0; i < parsedPattern.size(); ++i)
    {
        if(parsedPattern[i] != -1 && data[i] != parsedPattern[i]) return false;
    }
    return true;
}
uintptr_t AML::PatternScan(const char* pattern, const char* soLib)
{
    uintptr_t libStart = GetLib(soLib), scanLen = GetLibLength(soLib);
    return PatternScan(pattern, libStart, scanLen);
}

uintptr_t AML::PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen)
{
    std::vector<int> parsedPattern = ParsePattern(pattern);
    if(parsedPattern.empty()) return 0;

    size_t patternLen = parsedPattern.size();
    if(scanLen < patternLen) return 0;

    uint8_t* scanStart = (uint8_t*)libStart;
    size_t searchLength = scanLen - patternLen;
    for(size_t i = 0; i <= searchLength; ++i)
    {
        const uint8_t* currentAddress = scanStart + i;
        if(::ComparePattern(currentAddress, parsedPattern))
        {
            return (uintptr_t)currentAddress;
        }
    }
    return 0;
}

void AML::PatchForThumb(bool forThumb)
{
    ARMPatch::bThumbMode = forThumb;
}

const char* AML::GetFeatures()
{
    return g_szAMLFeatures;
}

void AML::AddFeature(const char* f)
{
    strncat(g_szAMLFeatures, f, sizeof(g_szAMLFeatures) - strlen(g_szAMLFeatures) - 1);
    strncat(g_szAMLFeatures, " ", sizeof(g_szAMLFeatures) - strlen(g_szAMLFeatures) - 1);
}

void AML::HookVtableFunc(void* ptr, unsigned int funcNum, void* func, void** original, bool instantiate)
{
    ::HookVtableFunc(ptr, funcNum, func, original, instantiate);
}

bool AML::IsGameFaked()
{
    return HasFakeAppName();
}

const char* AML::GetRealCurrentGame()
{
    return g_szAppName;
}

void* AML::GetLibHandle(const char* soLib)
{
    return ARMPatch::GetLibHandle(soLib);
}

void* AML::GetLibHandle(uintptr_t addr)
{
    return ARMPatch::GetLibHandle(addr);
}

bool AML::IsCorrectXDLHandle(void* ptr)
{
    return ARMPatch::IsCorrectXDLHandle(ptr);
}

uintptr_t AML::GetLibXDL(void* ptr)
{
    return ARMPatch::GetLibXDL(ptr);
}

uintptr_t AML::GetAddrBaseXDL(uintptr_t addr)
{
    return ARMPatch::GetAddrBaseXDL(addr);
}

size_t AML::GetSymSizeXDL(void* ptr)
{
    return ARMPatch::GetSymSizeXDL(ptr);
}

const char* AML::GetSymNameXDL(void* ptr)
{
    return ARMPatch::GetSymNameXDL(ptr);
}

void AML::ShowToast(bool longerDuration, const char* fmt, ...)
{
    if(!fmt) return;
    
    static char txt[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(txt, sizeof(txt), fmt, args);
    ShowToastMessage(GetCurrentJNI(), appContext, txt, longerDuration ? 1 : 0);
    va_end(args);
}

static size_t WriteToFileCB(void* buffer, size_t size, size_t nmemb, void* userdata)
{
    FILE* file = (FILE*)userdata;
    return file ? fwrite(buffer, size, nmemb, file) : 0;
}
bool AML::DownloadFile(const char* url, const char* saveto)
{
    if(!g_bEnableFileDownloads) return false;
    return JHTTPUtils::DownloadFile(url, saveto, g_nDownloadTimeout, g_szUserAgent, true);
}

static size_t WriteToDataCB(void* buffer, size_t size, size_t nmemb, MemChunk_t* chunk)
{
    size_t bytes = size * nmemb;
    if(chunk->out_len == 0) return bytes;

    size_t used = strlen(chunk->out);
    if(used >= chunk->out_len) return bytes;

    size_t remaining = chunk->out_len - used;
    size_t copyBytes = (bytes < remaining) ? bytes : remaining;
    memcpy(chunk->out + used, buffer, copyBytes);
    chunk->out[used + copyBytes] = 0;
    return bytes;
}
bool AML::DownloadFileToData(const char* url, char* out, size_t outLen)
{
    if(!g_bEnableFileDownloads) return false;
    return JHTTPUtils::DownloadFileToData(url, out, outLen, g_nDownloadTimeout, g_szUserAgent);
}

void AML::FileMD5(const char* path, char* out, size_t out_len)
{
    if(!out || out_len < MINIMUM_MD5_BUF_SIZE) return;

    out[0] = 0;
    g_MD5.Reset();
    if(g_MD5.UpdateFile(path)) g_MD5.Get(out, out_len);
}

int AML::GetModsLoadedCount()
{
    return modlist->GetModsNum();
}

JNIEnv* AML::GetJNIEnvironment()
{
    return GetCurrentJNI();
}

jobject AML::GetAppContextObject()
{
    return appContext;
}

bool AML::HasModOfBiggerVersion(const char* szGUID, const char* szVersion)
{
    return modlist->HasModOfBiggerVersion(szGUID, szVersion);
}

void AML::HookVtableFunc(void* ptr, unsigned int funcNum, unsigned int count, void* func, void** original, bool instantiate)
{
    ::HookVtableFunc(ptr, funcNum, count, func, original, instantiate);
}

int AML::PlaceNOP4(uintptr_t addr, size_t count)
{
    return ARMPatch::WriteNOP4(addr, count);
}

const char* AML::GetAndroidDataRootPath()
{
    return g_szAndroidDataRootDir;
}

bool AML::HookB(void* handle, void* fnAddress, void** orgFnAddress)
{
    return ARMPatch::hookBranchInternal(handle, fnAddress, orgFnAddress);
}

bool AML::HookBL(void* handle, void* fnAddress, void** orgFnAddress)
{
    return ARMPatch::hookBranchLinkInternal(handle, fnAddress, orgFnAddress);
}

bool AML::HookBLX(void* handle, void* fnAddress, void** orgFnAddress)
{
#ifdef AML32
    return ARMPatch::hookBranchLinkXInternal(handle, fnAddress, orgFnAddress);
#else // AML64
    return ARMPatch::hookBranchLinkInternal(handle, fnAddress, orgFnAddress);
#endif
}

void AML::MLSSaveFile()
{
    MLS::SaveFile();
}

bool AML::MLSHasValue(const char* key)
{
    return MLS::HasValue(key);
}

void AML::MLSDeleteValue(const char* key)
{
    MLS::DeleteValue(key);
}

void AML::MLSSetInt(const char* key, int32_t val)
{
    MLS::SetInt(key, val);
}

void AML::MLSSetFloat(const char* key, float val)
{
    MLS::SetFloat(key, val);
}

void AML::MLSSetInt64(const char* key, int64_t val)
{
    MLS::SetInt64(key, val);
}

void AML::MLSSetStr(const char* key, const char *val)
{
    MLS::SetStr(key, val);
}

bool AML::MLSGetInt(const char* key, int32_t *val)
{
    return MLS::GetInt(key, val);
}

bool AML::MLSGetFloat(const char* key, float *val)
{
    return MLS::GetFloat(key, val);
}

bool AML::MLSGetInt64(const char* key, int64_t *val)
{
    return MLS::GetInt64(key, val);
}

bool AML::MLSGetStr(const char* key, char *val, size_t len)
{
    return MLS::GetStr(key, val, len);
}

bool AML::IsThumbAddr(uintptr_t addr)
{
    return ARMPatch::IsThumbAddr(addr);
}

uintptr_t AML::GetBranchDest(uintptr_t addr)
{
    return ARMPatch::GetBranchDest(addr);
}

int AML::GetAndroidVersion()
{
    return g_nAndroidSDKVersion;
}

bool AML_CopyFile(const char* file, const char* dest);
bool AML::CopyFile(const char* file, const char* dest)
{
    return AML_CopyFile(file, dest);
}

void AML::RedirectReg(uintptr_t addr, uintptr_t to, bool doShortHook, GlossRegisters::e_reg targetReg)
{
#ifdef AML64
    if(doShortHook) Gloss::Inst::MakeArm64AbsoluteJump32(addr, to, (gloss_reg::e_reg)targetReg);
    else Gloss::Inst::MakeArm64AbsoluteJump(addr, to, (gloss_reg::e_reg)targetReg);
#endif
}

bool AML::HasAddrExecFlag(uintptr_t addr)
{
    return IsAddrExecute(addr);
}

void AML::ToggleHook(PHookHandle hook, bool enable)
{
    if(!hook) return;
    if(enable) GlossHookEnable(hook);
    else GlossHookDisable(hook);
}

void AML::DeHook(PHookHandle hook)
{
    if(!hook) return;
    GlossHookDelete(hook);
}

PHookHandle AML::HookInline(void* fnAddress, HookWithRegistersFn newFn, bool doShortHook)
{
#ifdef AML32
    uintptr_t addr = (uintptr_t)fnAddress;
    if(ARMPatch::IsThumbAddr(addr)) addr |= 0x1;
    return GlossHookInternal((void*)addr, (GlossHookInternalCallback)newFn, doShortHook, (addr & 0x1) ? i_set::$THUMB : i_set::$ARM);
#else // AML64
    return GlossHookInternal(fnAddress, (GlossHookInternalCallback)newFn, doShortHook, i_set::$ARM64);
#endif
}

bool bAML_HasFastmanModified = false;
bool AML::HasFastmanAPKModified()
{
    return bAML_HasFastmanModified;
}

const char* AML::GetInternalPath()
{
    return g_szInternalStoragePath;
}

const char* AML::GetInternalModsPath()
{
    return g_szInternalModsDir;
}

JavaVM* AML::GetJavaVM()
{
    return g_pJavaVM;
}

jobject AML::GetCurrentContext()
{
    return ::GetCurrentContext();
}

jobject g_VibratorObject;
jmethodID g_VibrateLongMethod, g_VibratePatternMethod, g_VibrateCancelMethod;
bool g_bVibratorInited = false;
inline bool InitVibroJNI(JNIEnv* env)
{
    jobject curCtx = ::GetCurrentContext();
    if(!curCtx) return false;

    if(!g_bVibratorInited)
    {
        jclass vibratorCls = env->FindClass("android/os/Vibrator");
        if(!vibratorCls) return false;

        jclass contextCls = env->FindClass("android/content/Context");
        if(!contextCls) return false;

        jmethodID sysServiceMethod = env->GetMethodID(contextCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        jfieldID vibratorSrvField = env->GetStaticFieldID(contextCls, "VIBRATOR_SERVICE", "Ljava/lang/String;");
        jstring vibratorFieldStr = (jstring)env->GetStaticObjectField(contextCls, vibratorSrvField);
        jobject localVibrateObject = env->CallObjectMethod(curCtx, sysServiceMethod, vibratorFieldStr);
        g_VibratorObject = env->NewGlobalRef(localVibrateObject);
        g_VibrateLongMethod = env->GetMethodID(vibratorCls, "vibrate", "(J)V");
        g_VibratePatternMethod = env->GetMethodID(vibratorCls, "vibrate", "([JI)V");
        g_VibrateCancelMethod = env->GetMethodID(vibratorCls, "cancel", "()V");

        env->DeleteLocalRef(localVibrateObject);
        env->DeleteLocalRef(vibratorFieldStr);
        env->DeleteLocalRef(vibratorCls);
        env->DeleteLocalRef(contextCls);

        g_bVibratorInited = true;
    }
    return true;
}
void AML::DoVibro(int msTime)
{
    if(msTime < 1 || msTime > 3000) return; // do not vibrate THAT MUCH

    JNIEnv* env = GetCurrentJNI();
    if(!env) return;

    if(InitVibroJNI(env))
    {
        env->CallVoidMethod(g_VibratorObject, g_VibrateLongMethod, (jlong)msTime);
    }
}
void AML::DoVibro(jlong* pattern, int patternItems)
{
    JNIEnv* env = GetCurrentJNI();
    if(!env) return;

    if(InitVibroJNI(env))
    {
        jlongArray patternArray = env->NewLongArray(patternItems);
        env->SetLongArrayRegion(patternArray, 0, patternItems, pattern);
        env->CallVoidMethod(g_VibratorObject, g_VibratePatternMethod, patternArray, -1);

        env->DeleteLocalRef(patternArray);
    }
}
void AML::CancelVibro()
{
    JNIEnv* env = GetCurrentJNI();
    if(env) env->CallVoidMethod(g_VibratorObject, g_VibrateCancelMethod);
}

bool g_bBatteryInited = false;
jstring g_pLevelStr;
float g_fCachedScale;
jmethodID g_GetLevelMethod;
jobject g_BatteryIntent;
float AML::GetBatteryLevel()
{
    JNIEnv* env = GetCurrentJNI();
    if(!env) return -1.0f;

    if(!g_bBatteryInited)
    {
        jclass intentFilterCls = env->FindClass("android/content/IntentFilter");
        jmethodID ctor = env->GetMethodID(intentFilterCls, "<init>", "(Ljava/lang/String;)V");
        jstring action = env->NewStringUTF("android.intent.action.BATTERY_CHANGED");
        jobject filter = env->NewObject(intentFilterCls, ctor, action);
        env->DeleteLocalRef(action);

        jclass contextCls = env->GetObjectClass(::GetCurrentContext());
        jmethodID registerReceiver = env->GetMethodID(contextCls, "registerReceiver", "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;");
        jobject batteryIntent = env->CallObjectMethod(::GetCurrentContext(), registerReceiver, NULL, filter);
        g_BatteryIntent = env->NewGlobalRef(batteryIntent);

        jclass intentCls = env->GetObjectClass(g_BatteryIntent);
        g_GetLevelMethod = env->GetMethodID(intentCls, "getIntExtra", "(Ljava/lang/String;I)I");
        jstring levelStr = env->NewStringUTF("level");
        g_pLevelStr = (jstring)env->NewGlobalRef(levelStr);
        jstring scaleStr = env->NewStringUTF("scale");
        g_fCachedScale = (float)env->CallIntMethod(g_BatteryIntent, g_GetLevelMethod, scaleStr, -1);
        env->DeleteLocalRef(scaleStr);
        env->DeleteLocalRef(levelStr);
        env->DeleteLocalRef(batteryIntent);
        env->DeleteLocalRef(filter);
        env->DeleteLocalRef(intentCls);
        env->DeleteLocalRef(contextCls);
        env->DeleteLocalRef(intentFilterCls);

        g_bBatteryInited = true;
    }

    jint level = env->CallIntMethod(g_BatteryIntent, g_GetLevelMethod, g_pLevelStr, -1);
    return ((level * 100.0f) / g_fCachedScale);
}

const char* AML::GetNativeLibsPath()
{
    if(g_szNativeLibPath[0] == 0)
    {
        JNIEnv* env = GetCurrentJNI();
        if(!env) return "";

        jstring jTmp = GetNativeLibDir(env);
        const char* szTmp = env->GetStringUTFChars(jTmp, NULL);
        snprintf(g_szNativeLibPath, sizeof(g_szNativeLibPath), "%s", szTmp);
        env->ReleaseStringUTFChars(jTmp, szTmp);
        env->DeleteLocalRef(jTmp);
    }
    return g_szNativeLibPath;
}

bool AML::PushToJavaUIThread(void (*fn)(void*), void* data)
{
    return ::PushToJavaUIThread(fn, data);
}

AAssetManager* AML::GetAssetManager()
{
    return ::GetCurrentAssetManager();
}

void* AML::OpenAsset(const char* path, int mode)
{
    return AAssetManager_open(::GetCurrentAssetManager(), path, mode);
}

void AML::CloseAsset(void* asset)
{
    if(asset) AAsset_close((AAsset*)asset);
}

size_t AML::GetAssetSize(void* asset)
{
    if(!asset) return 0;
    return AAsset_getLength((AAsset*)asset);
}

const void* AML::GetAssetBuffer(void* asset)
{
    if(!asset) return NULL;
    return AAsset_getBuffer((AAsset*)asset);
}

void AML::ReadAsset(void* asset, void* buf, size_t count)
{
    if(!asset || !buf || count < 1) return;
    AAsset_read((AAsset*)asset, buf, count);
}

jobject AML::InjectSmaliDEX(const uint8_t* dexBytes, size_t dexSize, const char* classToInit)
{
    return ::InjectSmaliDEX(GetCurrentJNI(), dexBytes, dexSize, classToInit);
}

jobject AML::GetInjectedSmaliDEX(const char* className)
{
    auto it = g_InjectedInstances.find(className);
    if(it != g_InjectedInstances.end()) return it->second;
    return NULL;
}

void AML::GetDisplaySize(int* width, int* height)
{
    JNIEnv* env = GetCurrentJNI();
    if (!env) return;

    jobject resources = CallJavaMethod<jobject>(::GetCurrentContext(), "getResources", "()Landroid/content/res/Resources;");
    jobject displayMetrics = CallJavaMethod<jobject>(resources, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
    
    jclass dmClass = env->GetObjectClass(displayMetrics);
    if(width)  *width  = env->GetIntField(displayMetrics, env->GetFieldID(dmClass, "widthPixels", "I"));
    if(height) *height = env->GetIntField(displayMetrics, env->GetFieldID(dmClass, "heightPixels", "I"));
    
    env->DeleteLocalRef(dmClass);
    env->DeleteLocalRef(displayMetrics);
    env->DeleteLocalRef(resources);
}

std::unordered_map<uintptr_t, size_t> g_PointerSizesMap;
uintptr_t AML::AllocateMemory(size_t size, bool executable)
{
    int prot = PROT_READ | PROT_WRITE;
    if(executable) prot |= PROT_EXEC;

    void* allocatedAddr = mmap(NULL, size, prot, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if(allocatedAddr == MAP_FAILED) return 0;

    g_PointerSizesMap[(uintptr_t)allocatedAddr] = size;
    return (uintptr_t)allocatedAddr;
}

bool AML::FreeMemory(uintptr_t pointer)
{
    auto it = g_PointerSizesMap.find(pointer);
    if(it != g_PointerSizesMap.end())
    {
        munmap((void*)pointer, it->second);
        return true;
    }
    return false;
}

uintptr_t AML::ReadPointerChain(uintptr_t baseAddr, std::initializer_list<int> offsets)
{
    // Simplify walls of the code like that:
    //   int data = *(int*)( *(uintptr_t*)( *(uintptr_t*)player + 0x40 ) + 0x60 )
    //   Into this: aml->ReadPointerChain((uintptr_t)player, 2, {0x40, 0x60})
    uintptr_t currentAddr = baseAddr;
    for(int offset : offsets)
    {
        if(currentAddr == 0) return 0;
        currentAddr = *(uintptr_t*)currentAddr;
        currentAddr += offset;
    }
    return currentAddr;
}

std::vector<uintptr_t> AML::FindAllPatterns(const char* pattern, uintptr_t libStart, uintptr_t scanLen)
{
    std::vector<uintptr_t> foundAddresses;

    std::vector<int> parsedPattern = ParsePattern(pattern);
    if(parsedPattern.empty()) return foundAddresses;

    size_t patternLen = parsedPattern.size();
    if(scanLen < patternLen) return foundAddresses;

    uint8_t* scanStart = reinterpret_cast<uint8_t*>(libStart);
    size_t searchLength = scanLen - patternLen;
    for(size_t i = 0; i <= searchLength; ++i)
    {
        const uint8_t* currentAddress = scanStart + i;
        if(::ComparePattern(currentAddress, parsedPattern))
        {
            foundAddresses.push_back(reinterpret_cast<uintptr_t>(currentAddress));
        }
    }
    return foundAddresses;
}

bool AML::ComparePattern(uintptr_t addr, const char* pattern)
{
    if(!addr || !pattern || !pattern[0]) return false;

    std::vector<int> parsedPattern = ParsePattern(pattern);
    if(parsedPattern.empty()) return false;

    return ::ComparePattern((uint8_t*)addr, parsedPattern);
}

void AML::ShowDialog(const char* title, const char* message, const char* buttonText, int styleResource)
{
    if (!title || !message) return;

    JNIEnv* env = GetJNIEnvironment();
    if(!env) return;
    
    jobject activityContext = ::GetCurrentActivity();
    if(!activityContext) return;

    jclass builderClass = env->FindClass("android/app/AlertDialog$Builder");
    if(!builderClass) return;

    jmethodID builderCtor;
    if(styleResource == 0)
    {
        builderCtor = env->GetMethodID(builderClass, "<init>", "(Landroid/content/Context;)V");
    }
    else
    {
        builderCtor = env->GetMethodID(builderClass, "<init>", "(Landroid/content/Context;I)V");
    }
    jmethodID setTitleMethod = env->GetMethodID(builderClass, "setTitle", "(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;");
    jmethodID setMessageMethod = env->GetMethodID(builderClass, "setMessage", "(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;");
    jmethodID setPosBtnMethod = env->GetMethodID(builderClass, "setPositiveButton", "(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;");
    jmethodID showMethod = env->GetMethodID(builderClass, "show", "()Landroid/app/AlertDialog;");

    if(!builderCtor || !setTitleMethod || !setMessageMethod || !setPosBtnMethod || !showMethod)
    {
        env->DeleteLocalRef(builderClass);
        return;
    }

    jstring titleStr = env->NewStringUTF(title);
    jstring messageStr = env->NewStringUTF(message);
    const char* btnLabel = buttonText ? buttonText : "OK";
    jstring btnStr = env->NewStringUTF(btnLabel);

    jobject builderObj;
    if(styleResource == 0)
    {
        builderObj = env->NewObject(builderClass, builderCtor, activityContext);
    }
    else
    {
        builderObj = env->NewObject(builderClass, builderCtor, activityContext, styleResource);
    }

    if(builderObj)
    {
        env->CallObjectMethod(builderObj, setTitleMethod, titleStr);
        env->CallObjectMethod(builderObj, setMessageMethod, messageStr);
        env->CallObjectMethod(builderObj, setPosBtnMethod, btnStr, NULL);
        
        jobject dialogObj = env->CallObjectMethod(builderObj, showMethod);
        if(dialogObj) env->DeleteLocalRef(dialogObj);
        env->DeleteLocalRef(builderObj);
    }
    env->DeleteLocalRef(titleStr);
    env->DeleteLocalRef(messageStr);
    env->DeleteLocalRef(btnStr);
    env->DeleteLocalRef(builderClass);
}

bool AML::FileExists(const char* path)
{
    if(!path || !path[0]) return false;
    return (access(path, F_OK) == 0);
}

size_t AML::FileSize(const char* path)
{
    if(!path || !path[0]) return 0;

    struct stat st;
    if(stat(path, &st) == 0) return st.st_size;
    return 0;
}

bool AML::IsDirectory(const char* path)
{
    if(!path || !path[0]) return false;

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

bool AML::RemoveFile(const char* path)
{
    return (unlink(path) == 0);
}

bool AML::RemoveDir(const char* path, bool recursive)
{
    if (!recursive) return (rmdir(path) == 0);

    DIR *dir = opendir(path);
    if (!dir) return false;

    struct dirent *entry;
    char fullPath[512];

    while((entry = readdir(dir)) != NULL)
    {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
        if(entry->d_type == DT_DIR)
        {
            RemoveDir(fullPath, true);
        }
        else
        {
            unlink(fullPath);
        }
    }
    closedir(dir);
    return (rmdir(path) == 0);
}

bool AML::CreateDirRecursive(const char* path)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);

    size_t len = strlen(tmp);
    if(tmp[len - 1] == '/') tmp[len - 1] = 0;

    for(char *p = tmp + 1; *p; ++p)
    {
        if(*p == '/')
        {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    return (mkdir(tmp, 0777) == 0 || errno == EEXIST);
}

jobject AML::GetCurrentActivity()
{
    return ::GetCurrentActivity();
}

void AML::GetNewsString(char* buf, size_t len)
{
    strxcpy(buf, g_szNewsString, len);
}

int AML::GetAndroidSystemResID(const char* innerClass, const char* fieldName)
{
    JNIEnv* env = GetJNIEnvironment();
    if(!env || !innerClass || !fieldName) return 0;

    char classPath[128];
    snprintf(classPath, sizeof(classPath), "android/R$%s", innerClass);

    jclass rClass = env->FindClass(classPath);
    if(!rClass)
    {
        env->ExceptionClear();
        return 0;
    }

    int resId = 0;
    jfieldID fieldId = env->GetStaticFieldID(rClass, fieldName, "I");
    if(fieldId)
    {
        resId = env->GetStaticIntField(rClass, fieldId);
    }
    else
    {
        env->ExceptionClear();
    }

    env->DeleteLocalRef(rClass);
    return resId;
}

int AML::ListDir(const char* path, ListDirCallback cb, void* data)
{
    DIR* dir = opendir(path);
    if(!dir) return -1;

    int count = 0;
    struct dirent* entry;
    while( (entry = readdir(dir)) != NULL )
    {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        bool isDir = (entry->d_type == DT_DIR);
        if(entry->d_type == DT_UNKNOWN)
        {
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
            struct stat st;
            isDir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
        }
        if(cb) cb(entry->d_name, isDir, data);
        ++count;
    }
    closedir(dir);
    return count;
}

int AML::ReadFileToBuffer(const char* path, char* out, size_t maxLen)
{
    if(!path || !out || maxLen == 0) return -1;

    int fd = open(path, O_RDONLY);
    if(fd < 0) return -1;

    ssize_t total = 0, r;
    while( (r = read(fd, out + total, maxLen - total)) > 0 ) total += r;
    close(fd);
    return ((r < 0) ? -1 : (int)total);
}

bool AML::WriteBufferToFile(const char* path, const void* buf, size_t len)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0) return false;

    size_t written = 0;
    while(written < len)
    {
        ssize_t w = write(fd, (const char*)buf + written, len - written);
        if (w <= 0)
        {
            close(fd);
            return false;
        }
        written += w;
    }
    close(fd);
    return true;
}

bool AML::MoveFile(const char* src, const char* dst)
{
    // rename() works if src and dst are on the same filesystem
    if(rename(src, dst) == 0) return true;

    struct stat st;
    if(stat(src, &st) != 0) return false;

    char* buf = (char*)malloc(st.st_size);
    if(!buf) return false;

    int inFd = open(src, O_RDONLY);
    if(inFd < 0)
    {
        free(buf);
        return false;
    }
    read(inFd, buf, st.st_size);
    close(inFd);

    bool ok = WriteBufferToFile(dst, buf, st.st_size);
    free(buf);
    if(ok) unlink(src);
    return ok;
}

time_t AML::GetFileModTime(const char* path)
{
    struct stat st;
    if(stat(path, &st) != 0) return (time_t)-1;
    return st.st_mtime;
}

void AML::OpenURL(const char* url)
{
    JNIEnv* env = GetJNIEnvironment();
    if(!env) return;

    jobject ctx = ::GetCurrentActivity();
    if(!ctx) return;

    jclass intentClass = env->FindClass("android/content/Intent");
    jfieldID actionView = env->GetStaticFieldID(intentClass, "ACTION_VIEW", "Ljava/lang/String;");
    jstring  action = (jstring)env->GetStaticObjectField(intentClass, actionView);

    jmethodID ctor = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");

    jclass uriClass = env->FindClass("android/net/Uri");
    jmethodID parse = env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jstring jUrl = env->NewStringUTF(url);
    jobject uri = env->CallStaticObjectMethod(uriClass, parse, jUrl);

    jobject intent = env->NewObject(intentClass, ctor, action, uri);

    jclass ctxClass = env->GetObjectClass(ctx);
    jmethodID startAct = env->GetMethodID(ctxClass, "startActivity", "(Landroid/content/Intent;)V");
    env->CallVoidMethod(ctx, startAct, intent);

    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(uri);
    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(action);
    env->DeleteLocalRef(ctxClass);
    env->DeleteLocalRef(uriClass);
    env->DeleteLocalRef(intentClass);
}

jobject AML::CallStaticJavaMethod(const char* cls, const char* method, const char* sig, ...)
{
    JNIEnv* env = GetJNIEnvironment();
    if(!env) return NULL;

    jclass jcls = env->FindClass(cls);
    if(!jcls || env->ExceptionCheck())
    {
        env->ExceptionClear();
        return NULL;
    }

    jmethodID mid = env->GetStaticMethodID(jcls, method, sig);
    if(!mid || env->ExceptionCheck())
    {
        env->ExceptionClear();
        env->DeleteLocalRef(jcls);
        return NULL;
    }

    va_list args;
    va_start(args, sig);
    jobject result = env->CallStaticObjectMethodV(jcls, mid, args);
    va_end(args);

    if(env->ExceptionCheck())
    {
        env->ExceptionClear();
        env->DeleteLocalRef(jcls);
        return NULL;
    }
    env->DeleteLocalRef(jcls);
    return result; 
}

jobject AML::GetStaticJavaField(const char* cls, const char* field, const char* sig)
{
    JNIEnv* env = GetJNIEnvironment();
    if(!env) return NULL;

    jclass jcls = env->FindClass(cls);
    if(!jcls || env->ExceptionCheck())
    {
        env->ExceptionClear();
        return NULL;
    }

    jfieldID fid = env->GetStaticFieldID(jcls, field, sig);
    if(!fid || env->ExceptionCheck())
    {
        env->ExceptionClear();
        env->DeleteLocalRef(jcls);
        return NULL;
    }

    jobject result = env->GetStaticObjectField(jcls, fid);
    if(env->ExceptionCheck())
    {
        env->ExceptionClear();
        env->DeleteLocalRef(jcls);
        return NULL;
    }

    env->DeleteLocalRef(jcls);
    return result;
}

bool AML::SetStaticJavaField(const char* cls, const char* field, const char* sig, jobject value)
{
    JNIEnv* env = GetJNIEnvironment();
    if(!env) return false;

    jclass jcls = env->FindClass(cls);
    if(!jcls || env->ExceptionCheck())
    {
        env->ExceptionClear();
        return false;
    }

    jfieldID fid = env->GetStaticFieldID(jcls, field, sig);
    if(!fid || env->ExceptionCheck())
    {
        env->ExceptionClear();
        env->DeleteLocalRef(jcls);
        return false;
    }

    env->SetStaticObjectField(jcls, fid, value);
    if(env->ExceptionCheck())
    {
        env->ExceptionClear();
        env->DeleteLocalRef(jcls);
        return false;
    }
    env->DeleteLocalRef(jcls);
    return true;
}

int AML::GetFailedModsCount()
{
    return g_nFailedToLoad;
}

bool AML::IsFileDownloadsEnabled()
{
    return g_bEnableFileDownloads;
}

bool AML::IsMLSInManualSave()
{
    return g_bMLSOnlyManualSaves;
}

int AML::GetDownloadTimeout()
{
    return g_nDownloadTimeout;
}

void AML::ListMods(ListModsCallback cb, void* data, bool startWithLatest)
{
    modlist->ListMods(cb, data, startWithLatest);
}

bool AML::IsMainThread()
{
    return (gettid() == g_MainThreadID);
}

void AML::DataMD5(void* data, size_t len, char* out, size_t out_len)
{
    if(!out || out_len < MINIMUM_MD5_BUF_SIZE) return;

    out[0] = 0;
    g_MD5.Reset();
    g_MD5.Update(data, len);
    g_MD5.Get(out, out_len);
}

int AML::GetLatestDownloadErrorCode()
{
    return g_nLatestDownloadErrorCode;
}



static AML amlLocal;
IAML* aml = (IAML*)&amlLocal;
AML* g_pAML = &amlLocal;