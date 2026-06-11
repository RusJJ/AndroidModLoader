#include <mod/icfg.h>

class CFG : public ICFG
{
public:
    void* InitIniPointer();
    void ParseInputStream(void* iniPointer, const char* szFilename);
    void GenerateToOutputStream(void* iniPointer, const char* szFilename);
    const char* GetValueFrom(void* iniPointer, const char* szSection, const char* szKey); // Unsafe
    void SetValueTo(void* iniPointer, const char* szSection, const char* szKey, const char* szValue);

    // AML 1.4.0

    bool GetValueFrom(void* iniPointer, const char* szSection, const char* szKey, char* pValue, int maxLen);
    bool HasKey(void* iniPointer, const char* szSection, const char* szKey);
    bool HasSectionComment(void* iniPointer, const char* szSection);
    bool HasKeyComment(void* iniPointer, const char* szSection, const char* szKey);
};