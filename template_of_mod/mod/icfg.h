/* Is not required. Can be used only for a smaller size of mod (~370kb savings) */
/* because of fstream include (it has a lot of templates & not only) */
/* There's no reason of using this feature if you're already using fstream */

#ifndef _ICFG
#define _ICFG

class ICFG
{
public:
    virtual void* InitIniPointer() = 0;
    virtual void ParseInputStream(void* iniPointer, const char* szFilename) = 0;
    virtual void GenerateToOutputStream(void* iniPointer, const char* szFilename) = 0;
    virtual const char* GetValueFrom(void* iniPointer, const char* szSection, const char* szKey) = 0;
    virtual void SetValueTo(void* iniPointer, const char* szSection, const char* szKey, const char* szValue) = 0;
};

extern ICFG* icfg;
inline ICFG* GetCFGInterface()
{
    return icfg;
}

#endif // _ICFG