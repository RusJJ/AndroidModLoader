#include "modpaks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mod/logger.h>

extern int g_nDownloadTimeout;

CURL* curl = NULL;
char szFileData[FILE_DATA_SIZE] = {0};
extern char g_szUserAgent[256];
size_t nReadedBytes = 0;

void InitCURL()
{
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
}

static size_t WriteToFileCB(void* buffer, size_t size, size_t nmemb, void* userdata)
{
    FILE* file = (FILE*)userdata;
    return file ? fwrite(buffer, size, nmemb, file) : 0;
}
static size_t WriteToDataCB(void* buffer, size_t size, size_t nmemb, void* userdata)
{
    size_t bytes = size * nmemb;
    if(nReadedBytes >= FILE_DATA_SIZE - 1) return bytes;

    size_t remaining = FILE_DATA_SIZE - 1 - nReadedBytes;
    size_t copyBytes = (bytes < remaining) ? bytes : remaining;

    memcpy(szFileData + nReadedBytes, buffer, copyBytes);
    nReadedBytes += copyBytes;
    szFileData[nReadedBytes] = 0;
    return bytes;
}

CURLcode DownloadFile(const char* url, const char* path)
{
    if(!curl) return CURLE_FAILED_INIT;
    curl_easy_reset(curl);
    
    FILE* file = fopen(path, "wb");
    if(!file) return CURLE_WRITE_ERROR;
    
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // cURL fails at SSL/TLS here, for some reason
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFileCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_nDownloadTimeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, g_szUserAgent);
    
    CURLcode res = curl_easy_perform(curl);
    fclose(file);
    return res;
}

CURLcode DownloadFileToData(const char* url)
{
    if(!curl) return CURLE_FAILED_INIT;
    curl_easy_reset(curl);
    szFileData[0] = 0;
    nReadedBytes = 0;
    
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // cURL fails at SSL/TLS here, for some reason
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToDataCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_nDownloadTimeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, g_szUserAgent);
    
    CURLcode res = curl_easy_perform(curl);
    return res;
}

inline bool str_equal(const char* str1, const char* str2) { 
    for ( ; *str1 == *str2 && *str1 != 0; ++str1, ++str2 ) {}
        return *str2 == *str1; 
}
extern bool g_bShowUpdatedToast, g_bShowUpdateFailedToast;
static inline void ProcessLine(ModDesc* d, char* data)
{
    char left[64], middle[64], right[128];
    int scanned = sscanf(data, "%[^:]:%[^:]:%[^\n]", left, middle, right);
    if(scanned < 3) return;
    else if(!strncmp(left, "myself", 6) || !strcmp(left, d->m_pInfo->GUID()))
    {
        if(!modlist->HasModOfVersion(d->m_pInfo->GUID(), middle))
        {
            CURLcode res = DownloadFile(right, d->m_szLibPath);
            if(res == CURLE_OK)
            {
                if(g_bShowUpdatedToast) aml->ShowToast(true, "Mod %s has been updated!\nRestart the game to load new mod.", d->m_pInfo->Name());
            }
            else
            {
                if(g_bShowUpdateFailedToast) aml->ShowToast(true, "Mod %s has failed to update!\nIs this located in internal folder..?", d->m_pInfo->Name());
            }
        }
    }
    else
    {
        // files
        // 1: filepath (relative to files folder)
        // 2: checksum (MD5?)
        // 3: URL
        
        char md5[MINIMUM_MD5_BUF_SIZE] {0};
        char filepath[256], filepathTmp[256], filepathOld[256];
        snprintf(filepath, sizeof(filepath), "%s/%s", aml->GetAndroidDataPath(), left);
        snprintf(filepathOld, sizeof(filepathOld), "%s/%s.old", aml->GetAndroidDataPath(), left);
        aml->FileMD5(filepath, md5, sizeof(md5));
        
        //if(!md5[0]) return;
        
        if(md5[0] == 0 || !str_equal(md5, middle))
        {
            DownloadFile(right, filepath);
        }
    }
}
void ProcessData(ModDesc* d)
{
    if(szFileData[0] == 0) return; // bruh
    
    char* newlinePtr = &szFileData[0], *data;
    do
    {
        data = newlinePtr;
        newlinePtr = strstr(data, "\n");
        if(newlinePtr != NULL)
        {
            newlinePtr[0] = 0;
            ++newlinePtr;
        }
        if(data[0] != 0 && data[0] != '/' && data[1] != '/') ProcessLine(d, data);
        if(newlinePtr == NULL) break;
    }
    while(true);
    
    // End
    szFileData[0] = 0;
}
