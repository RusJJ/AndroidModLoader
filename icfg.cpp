#include <icfg_desc.h>
#include <mod/thirdparty/inipp.h>
#include <fstream>

#include <mod/iaml.h>

extern std::string g_szCfgPath;

void* CFG::InitIniPointer()
{
    return new inipp::Ini<char>();
}
void CFG::ParseInputStream(void* iniPointer, const char* szFilename)
{	std::ifstream cfgStream((g_szCfgPath + szFilename + ".ini").c_str());
    ((inipp::Ini<char>*)iniPointer)->parse(cfgStream);
}
void CFG::GenerateToOutputStream(void* iniPointer, const char* szFilename)
{	std::ofstream cfgStream((g_szCfgPath + szFilename + ".ini").c_str());
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