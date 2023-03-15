#ifndef DONT_USE_STB
    #include <mod/thirdparty/stb_sprintf.h>
    #define sprintf stbsp_sprintf
    #define snprintf stbsp_snprintf
#endif
#include <icfg_desc.h>
#include <mod/thirdparty/inicpp.h>
#include <mod/logger.h>
#include <fstream>

#include <mod/iaml.h>

extern char g_szCfgPath[0xFF];

void* CFG::InitIniPointer()
{
    void* ret = new ini::IniFile;
    return ret;
}
void CFG::ParseInputStream(void* iniPointer, const char* szFilename)
{
    char path[0xFF];
    snprintf(path, sizeof(path), "%s/%s.ini", g_szCfgPath, szFilename);
    ((ini::IniFile*)iniPointer)->load(path);
}
void CFG::GenerateToOutputStream(void* iniPointer, const char* szFilename)
{
    char path[0xFF];
    snprintf(path, sizeof(path), "%s/%s.ini", g_szCfgPath, szFilename);
    ((ini::IniFile*)iniPointer)->save(path);
}
const char* CFG::GetValueFrom(void* iniPointer, const char* szSection, const char* szKey)
{
    return (*(ini::IniFile*)iniPointer)[szSection][szKey].as<const char*>();
}
void CFG::SetValueTo(void* iniPointer, const char* szSection, const char* szKey, const char* szValue)
{
    (*(ini::IniFile*)iniPointer)[szSection][szKey] = szValue;
}
