#include "modpaks.h"
#include <stdio.h>
#include <stdlib.h>
#include <mod/logger.h>

void InitCURL()
{
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
}


CURL* curl = NULL;
char szFileData[FILE_DATA_SIZE] = {0};

static size_t WriteToFileCB(void* buffer, size_t size, size_t nmemb, void* userdata)
{
    FILE* file = (FILE*)userdata;
    size_t written = fwrite(buffer, size, nmemb, file);
    return written;
}
static size_t WriteToDataCB(void* buffer, size_t size, size_t nmemb, void* userdata)
{
    return snprintf(szFileData, FILE_DATA_SIZE, "%s", buffer);
}

CURLcode DownloadFile(const char* url, const char* path)
{
    if(!curl) return CURLE_FAILED_INIT;
    
    FILE* file = fopen(path, "wb");
    if(!file) return CURLE_WRITE_ERROR;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFileCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    
    CURLcode res = curl_easy_perform(curl);
    fclose(file);
    return res;
}

CURLcode DownloadFileToData(const char* url)
{
    if(!curl) return CURLE_FAILED_INIT;
    
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // cURL fails at SSL/TLS here, for some reason
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToDataCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    
    CURLcode res = curl_easy_perform(curl);
    return res;
}

extern bool g_bShowUpdatedToast;
static inline void ProcessLine(ModDesc* d, char* data)
{
    char left[64], middle[64], right[128];
    int scanned = sscanf(data, "%[^:]:%[^:]:%[^\n]", left, middle, right);
    if(scanned < 3) return;
    else if(!strncmp(left, "myself", 6) || !strcmp(left, d->info->GUID()))
    {
        if(!modlist->HasModOfBiggerVersion(d->info->GUID(), middle))
        {
            //logger->Info("DownloadFile(%s, %s)", right, d->szLibPath);
            if(DownloadFile(right, d->szLibPath) == CURLE_OK)
            {
                if(g_bShowUpdatedToast) aml->ShowToast(1000, "Mod %s has been updated!\nRestart the game to load new mods.", d->info->Name());
            }
            else
            {
                if(g_bShowUpdatedToast) aml->ShowToast(1000, "Mod %s has failed to update!", d->info->Name());
            }
        }
    }
    else
    {
        // files
        // 1: filepath (relative to files folder)
        // 2: checksum (MD5?)
        // 3: URL
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
