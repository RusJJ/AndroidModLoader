#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>

#include "amlmod.h"
#include "iaml.h"
#include "thirdparty/inipp.h"

extern ModInfo* modinfo;

extern std::string g_szCfgPath;

Config::Config(const char* szName)
{
	m_bInitialized = false;
    m_szName = szName;
	m_iniMyConfig = new inipp::Ini<char>();

	#ifndef __AML
	Init();
	#endif
}

void Config::Init()
{
	if(m_bInitialized) return;
	m_bInitialized = true;
	#ifdef __AML
		std::ifstream cfgStream((g_szCfgPath + m_szName + ".ini").c_str());
	#else
		std::ifstream cfgStream((std::string(aml->GetConfigPath()) + m_szName + ".ini").c_str());
	#endif
	if(cfgStream.is_open())
	{
		((inipp::Ini<char>*)m_iniMyConfig)->parse(cfgStream);
	}
	cfgStream.close();
}

void Config::Save()
{
	if(!m_bInitialized) return;
	#ifdef __AML
		std::ofstream cfgStream((g_szCfgPath + m_szName + ".ini").c_str());
	#else
		std::ofstream cfgStream((std::string(aml->GetConfigPath()) + m_szName + ".ini").c_str());
	#endif
	if(cfgStream.is_open())
	{
		((inipp::Ini<char>*)m_iniMyConfig)->generate(cfgStream);
	}
	cfgStream << "\n";
	cfgStream.close();
}

ConfigEntry* Config::Bind(const char* szKey, const char* szDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return nullptr;

	ConfigEntry* pRet = new ConfigEntry;
	pRet->m_pBoundCfg = this;
	pRet->m_szMySection = szSection;
	pRet->m_szMyKey = szKey;
	const char* tryToGetValue;
	
	auto s = ((inipp::Ini<char>*)m_iniMyConfig)->sections[szSection];
	tryToGetValue = s[szKey].c_str();
	if(tryToGetValue[0] == '\0')
		tryToGetValue = szDefaultValue;
	pRet->SetString(tryToGetValue);

	Save();

	return pRet;
}

void ConfigEntry::SetString(const char* newValue)
{
	m_szValue = newValue;
	m_nIntegerValue = atoi(m_szValue);
	m_fFloatValue = (float)atof(m_szValue);

	((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
}

void ConfigEntry::SetFloat(float newValue)
{
	m_fFloatValue = newValue;
    m_nIntegerValue = (int)newValue;
    
	char szVal[32];
	sprintf(szVal, "%f", newValue);
    m_szValue = szVal;

	((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
}

void ConfigEntry::SetInt(int newValue)
{
	m_fFloatValue = (float)newValue;
    m_nIntegerValue = newValue;
    
	char szVal[32];
	sprintf(szVal, "%d", newValue);
    m_szValue = szVal;
	
	((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
}

void ConfigEntry::SetBool(bool newValue)
{
	m_fFloatValue = newValue?1.0f:0.0f;
    m_nIntegerValue = newValue?1:0;
    m_szValue = newValue?"1":"0";
	
	((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
}