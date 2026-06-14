#include <modslist.h>

#define FILE_DATA_SIZE (1024 * 16)

extern char szFileData[FILE_DATA_SIZE];

bool DownloadFile(const char* url, const char* path);
bool DownloadFileToData(const char* url);
void ProcessData(ModDesc* d);