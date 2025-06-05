#include <aml.h>
#include <mod/logger.h>
#include <ARMPatch/armpatch_src/ARMPatch.h>
#include <vtable_hooker.h>
#include <modslist.h>
#include <mls.h>

#include <curl/curl.h>
#define WC_NO_HARDEN // suppress the annoying warning.
#include <wolfssl/wolfcrypt/md5.h>
#include <stdio.h>
#include <time.h>
#include <jnifn.h>

#include <Gloss.h>

char g_szAMLFeatures[2048] = "AML ARMPATCH HOOK CONFIG INTERFACE GLOSS ";
extern char g_szAppName[256], g_szFakeAppName[256];
extern char g_szCfgPath[256];
extern char g_szAndroidDataDir[256];
extern char g_szAndroidDataRootDir[256];
extern char g_szInternalStoragePath[256];
extern char g_szInternalModsDir[256];
extern const char* g_szDataDir;
extern jobject appContext;
extern JNIEnv* env;
extern bool g_bEnableFileDownloads;
extern CURL* curl;
extern int g_nDownloadTimeout;

extern JavaVM *g_pJavaVM;
JNIEnv* GetCurrentJNI();

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

uintptr_t AML::PatternScan(const char* pattern, const char* soLib)
{
    return ARMPatch::GetAddressFromPattern(pattern, soLib);
}

uintptr_t AML::PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen)
{
    return ARMPatch::GetAddressFromPattern(pattern, libStart, scanLen);
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
    strcat(g_szAMLFeatures, f);
    strcat(g_szAMLFeatures, " ");
}

void AML::HookVtableFunc(void* ptr, unsigned int funcNum, void* func, void** original, bool instantiate)
{
    HookVtableFunc(ptr, funcNum, func, original, instantiate);
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
    FILE* file = fopen((const char*)userdata, "wb");
    if(!file) return 0;
    
    size_t written = fwrite(buffer, size, nmemb, file);
    fclose(file);
    return written;
}
bool AML::DownloadFile(const char* url, const char* saveto)
{
    if(!g_bEnableFileDownloads) return false;
    if(!curl) return false;
    curl_easy_reset(curl);
    
    FILE* file = fopen(saveto, "a");
    if(!file) return false; // Dont clean it up first!
    fclose(file);
    
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // cURL fails at SSL/TLS here, for some reason
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFileCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, saveto);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_nDownloadTimeout);
    
    CURLcode res = curl_easy_perform(curl);
    return res != CURLE_OK;
}

static size_t WriteToDataCB(void* buffer, size_t size, size_t nmemb, MemChunk_t* chunk)
{
    return snprintf(chunk->out, chunk->out_len, "%s", (const char*)buffer);
}
bool AML::DownloadFileToData(const char* url, char* out, size_t outLen)
{
    if(!g_bEnableFileDownloads) return false;
    if(!curl) return false;
    curl_easy_reset(curl);
    
    MemChunk_t chunk = { out, outLen - 1 };
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // cURL fails at SSL/TLS here, for some reason
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToDataCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_nDownloadTimeout);
    
    CURLcode res = curl_easy_perform(curl);
    out[outLen - 1] = 0;
    return res == CURLE_OK;
}

void AML::FileMD5(const char* path, char* out, size_t out_len)
{
    if(!out || out_len < MINIMUM_MD5_BUF_SIZE) return;
    
    FILE *file;
    wc_Md5 md5Context;
    unsigned char md5Digest[WC_MD5_DIGEST_SIZE];
    out[0] = 0;

    // Open file for reading
    file = fopen(path, "rb");
    if(!file) return;

    // Initialize MD5 context
    wc_InitMd5(&md5Context);

    // Read file contents and update MD5 context
    unsigned char buffer[1024];
    size_t bytesRead;
    while((bytesRead = fread(buffer, 1, sizeof(buffer), file))) {
        wc_Md5Update(&md5Context, buffer, bytesRead);
    }

    // Finalize MD5 context and get hash value
    wc_Md5Final(&md5Context, md5Digest);

    // Save MD5 hash value as hexadecimal string
    char hex[3];
    for(uint8_t i = 0; i < WC_MD5_DIGEST_SIZE; ++i)
    {
        sprintf(hex, "%02x", md5Digest[i]);
        strcat(out, hex);
    }
    out[2 * WC_MD5_DIGEST_SIZE] = 0;

    // Close file
    fclose(file);
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
    HookVtableFunc(ptr, funcNum, count, func, original, instantiate);
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


static AML amlLocal;
IAML* aml = (IAML*)&amlLocal;
AML* g_pAML = &amlLocal;