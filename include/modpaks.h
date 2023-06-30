#include <curl/curl.h>
#include <modslist.h>

#define FILE_DATA_SIZE 8192

extern CURL* curl;
extern char szFileData[FILE_DATA_SIZE];

void InitCURL();
CURLcode DownloadFile(const char* url, const char* path);
CURLcode DownloadFileToData(const char* url);
void ProcessData(ModDesc* d);
