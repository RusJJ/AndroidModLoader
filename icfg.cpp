#ifndef DONT_USE_STB
    #include <mod/thirdparty/stb_sprintf.h>
    #define sprintf stbsp_sprintf
    #define snprintf stbsp_snprintf
#endif
#include <icfg_desc.h>
#include <JINI.h>
#include <mod/logger.h>
#include <fstream>

#include <mod/iaml.h>

extern char g_szCfgPath[256];

void* CFG::InitIniPointer()
{
    void* ret = new JINI;
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