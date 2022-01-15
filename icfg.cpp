#include <icfg_desc.h>
#include <mod/thirdparty/inipp.h>
#include <fstream>

#include <mod/iaml.h>

extern const char* g_szCfgPath;

void* CFG::InitIniPointer()
{
    return new inipp::Ini<char>();
}
void CFG::ParseInputStream(void* iniPointer, const char* szFilename)
{
    char path[0xFF];
    sprintf(path, "%s%s.ini", g_szCfgPath, szFilename);
    std::ifstream cfgStream(path);
    ((inipp::Ini<char>*)iniPointer)->parse(cfgStream);
}
void CFG::GenerateToOutputStream(void* iniPointer, const char* szFilename)
{
    char path[0xFF];
    sprintf(path, "%s%s.ini", g_szCfgPath, szFilename);
    std::ofstream cfgStream(path);
    ((inipp::Ini<char>*)iniPointer)->generate(cfgStream);
}
const char* CFG::GetValueFrom(void* iniPointer, const char* szSection, const char* szKey)
{
    return ((inipp::Ini<char>*)iniPointer)->sections[szSection][szKey].c_str();
}
void CFG::SetValueTo(void* iniPointer, const char* szSection, const char* szKey, const char* szValue)
{
    ((inipp::Ini<char>*)iniPointer)->sections[szSection][szKey] = szValue;
}