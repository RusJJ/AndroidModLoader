#include <mod/icfg.h>

class CFG : public ICFG
{
public:
    void* InitIniPointer();
    void ParseInputStream(void* iniPointer, const char* szFilename);
    void GenerateToOutputStream(void* iniPointer, const char* szFilename);
    const char* GetValueFrom(void* iniPointer, const char* szSection, const char* szKey);
    void SetValueTo(void* iniPointer, const char* szSection, const char* szKey, const char* szValue);
};