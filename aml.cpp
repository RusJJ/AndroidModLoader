#include <aml.h>
#include <ARMPatch/armpatch_src/ARMPatch.h>
#include <vtable_hooker.h>
#include <modslist.h>
#include <curl/curl.h>
#define WC_NO_HARDEN // suppress the annoying warning.
#include <wolfssl/wolfcrypt/md5.h>
#include <stdio.h>
#include <time.h>
#include <jnifn.h>

char g_szAMLFeatures[1024] = "AML ARMPATCH HOOK CONFIG INTERFACE SUBSTRATE ";
extern char g_szAppName[0xFF], g_szFakeAppName[0xFF];
extern char g_szCfgPath[0xFF];
extern char g_szAndroidDataDir[0xFF];
extern const char* g_szDataDir;
extern jobject appContext;
extern JNIEnv* env;
extern bool g_bEnableFileDownloads;
extern CURL* curl;
extern int g_nDownloadTimeout;

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
    ARMPatch::WriteBLX(addr, dest);
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
    
    char txt[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(txt, sizeof(txt), fmt, args);
    ShowToastMessage(env, appContext, txt, longerDuration ? 1 : 0);
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
    
    MemChunk_t chunk = { out, outLen };
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // cURL fails at SSL/TLS here, for some reason
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToDataCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_nDownloadTimeout);
    
    CURLcode res = curl_easy_perform(curl);
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
    return env;
}

jobject AML::GetAppContextObject()
{
    return appContext;
}

bool AML::HasModOfBiggerVersion(const char* szGUID, const char* szVersion)
{
    return modlist->HasModOfBiggerVersion(szGUID, szVersion);
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
    return ARMPatch::hookBranchLinkXInternal(handle, fnAddress, orgFnAddress);
}

static AML amlLocal;
IAML* aml = (IAML*)&amlLocal;
AML* g_pAML = &amlLocal;
