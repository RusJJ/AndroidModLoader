#ifndef DONT_USE_STB
    #include <mod/thirdparty/stb_sprintf.h>
    #define sprintf stbsp_sprintf
    #define snprintf stbsp_snprintf
#endif

#include <mod/amlmod.h>
#include <mod/logger.h>
#include <icfg_desc.h>

#include <vector>
#include <mutex>
#include <JINI.h>

extern char g_szCfgPath[256];

static std::vector<JINI*> g_allocatedInis;
static std::mutex g_iniMutex; // thread safety
__attribute__((destructor)) void ClearIniPointers()
{
    for(JINI* ptr : g_allocatedInis)
    {
        delete ptr;
    }
    g_allocatedInis.clear();
}

void* CFG::InitIniPointer()
{
    JINI* ret = new JINI;

    std::lock_guard<std::mutex> lock(g_iniMutex);
    g_allocatedInis.push_back(ret);

    return ret;
}
void CFG::ParseInputStream(void* iniPointer, const char* szFilename)
{
    if(!szFilename) return;

    char path[384];
    snprintf(path, sizeof(path), "%s/%s.ini", g_szCfgPath, szFilename);
    ((JINI*)iniPointer)->load(path);
}
void CFG::GenerateToOutputStream(void* iniPointer, const char* szFilename)
{
    char path[384];
    int n = snprintf(path, sizeof(path), "%s/%s.ini", g_szCfgPath, szFilename);
    if(n < 0 || n >= (int)sizeof(path)) return;

    ((JINI*)iniPointer)->save_as(path);
}
const char* CFG::GetValueFrom(void* iniPointer, const char* szSection, const char* szKey)
{
    thread_local std::string value;
    value.clear();

    if(!szSection || !szKey || !((JINI*)iniPointer)->get_into(szSection, szKey, value))
    {
        return "";
    }
    return value.c_str();
}
void CFG::SetValueTo(void* iniPointer, const char* szSection, const char* szKey, const char* szValue)
{
    if(!szSection || !szKey || !szValue) return;
    ((JINI*)iniPointer)->set(szSection, szKey, szValue);
}

// AML 1.4.0

bool CFG::GetValueFrom(void* iniPointer, const char* szSection, const char* szKey, char* pValue, int maxLen)
{
    std::string value;
    if(!szSection || !szKey || !((JINI*)iniPointer)->get_into(szSection, szKey, value))
    {
        return false;
    }

    strxcpy(pValue, value.c_str(), maxLen);
    return true;
}
bool CFG::HasSection(void* iniPointer, const char* szSection)
{
    return ( szSection && ((JINI*)iniPointer)->has_section(szSection) );
}
bool CFG::HasKey(void* iniPointer, const char* szSection, const char* szKey)
{
    return ( szSection && szKey && ((JINI*)iniPointer)->has(szSection, szKey) );
}
bool CFG::HasSectionComment(void* iniPointer, const char* szSection)
{
    std::string tmp;
    return ( szSection && ((JINI*)iniPointer)->get_section_comment(szSection, tmp) );
}
bool CFG::HasKeyComment(void* iniPointer, const char* szSection, const char* szKey)
{
    std::string tmp;
    return ( szSection && szKey && ((JINI*)iniPointer)->get_key_comment(szSection, szKey, tmp) );
}