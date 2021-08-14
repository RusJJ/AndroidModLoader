#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <mod/logger.h>

#include "amlmod.h"
#include "iaml.h"
#ifdef _ICFG
	//ICFG* icfg;
#else
	#include <fstream>
	#include "thirdparty/inipp.h"
#endif
#ifdef __AML
	extern std::string g_szCfgPath;
#endif



extern ModInfo* modinfo;

Config::Config(const char* szName)
{
#ifndef _ICFG
	m_iniMyConfig = new inipp::Ini<char>();
#else
	m_pICFG = (ICFG*)GetInterface("AMLConfig");
	m_iniMyConfig = m_pICFG->InitIniPointer();
#endif
	m_bInitialized = false;
    m_szName = szName;

	#ifndef __AML
		Init();
	#endif
}

void Config::Init()
{
	if(m_bInitialized) return;
	m_bInitialized = true;
	#ifdef _ICFG
		m_pICFG->ParseInputStream(m_iniMyConfig, m_szName);
	#else
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
	#endif
}

void Config::Save()
{
	if(!m_bInitialized) return;
	#ifdef _ICFG
		m_pICFG->GenerateToOutputStream(m_iniMyConfig, m_szName);
	#else
		#ifdef __AML
			std::ofstream cfgStream((g_szCfgPath + m_szName + ".ini").c_str());
		#else
			std::ofstream cfgStream((std::string(aml->GetConfigPath()) + m_szName + ".ini").c_str());
		#endif
		if(cfgStream.is_open())
		{
			((inipp::Ini<char>*)m_iniMyConfig)->generate(cfgStream);
		}
		cfgStream << "";
		cfgStream.close();
	#endif
}

ConfigEntry* Config::Bind(const char* szKey, const char* szDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return nullptr;

	ConfigEntry* pRet = new ConfigEntry;
	pRet->m_pBoundCfg = this;
	pRet->m_szMySection = szSection;
	pRet->m_szMyKey = szKey;
	const char* tryToGetValue;
	
	#ifndef _ICFG
		tryToGetValue = ((inipp::Ini<char>*)m_iniMyConfig)->sections[szSection][szKey].c_str();
	#else
		tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
	#endif
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

	#ifndef _ICFG
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#else
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#endif
}

void ConfigEntry::SetFloat(float newValue)
{
	m_fFloatValue = newValue;
    m_nIntegerValue = (int)newValue;
    
	char szVal[32];
	sprintf(szVal, "%f", newValue);
    m_szValue = szVal;

	#ifndef _ICFG
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#else
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#endif
}

void ConfigEntry::SetInt(int newValue)
{
	m_fFloatValue = (float)newValue;
    m_nIntegerValue = newValue;
    
	char szVal[32];
	sprintf(szVal, "%d", newValue);
    m_szValue = szVal;

	#ifndef _ICFG
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#else
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#endif
}

void ConfigEntry::SetBool(bool newValue)
{
	m_fFloatValue = newValue?1.0f:0.0f;
    m_nIntegerValue = newValue?1:0;
    m_szValue = newValue?"1":"0";

	#ifndef _ICFG
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#else
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#endif
}