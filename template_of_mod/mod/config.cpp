#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <mod/logger.h>

#include "amlmod.h"
#include "iaml.h"
#if !defined(__AML) && defined(_ICFG)
	ICFG* icfg;
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
#if !defined(__AML) && defined(_ICFG)
	m_pICFG = (ICFG*)GetInterface("AMLConfig");
	m_iniMyConfig = m_pICFG->InitIniPointer();
#else
	m_iniMyConfig = new inipp::Ini<char>();
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
	#if !defined(__AML) && defined(_ICFG)
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
	#if !defined(__AML) && defined(_ICFG)
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
	#if !defined(__AML) && defined(_ICFG)
		tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
	#else
		tryToGetValue = ((inipp::Ini<char>*)m_iniMyConfig)->sections[szSection][szKey].c_str();
	#endif
	if(tryToGetValue[0] == '\0')
		pRet->SetString(szDefaultValue);
	else
		pRet->SetString(tryToGetValue);
	Save();
	return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, int nDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return nullptr;
	ConfigEntry* pRet = new ConfigEntry;
	pRet->m_pBoundCfg = this;
	pRet->m_szMySection = szSection;
	pRet->m_szMyKey = szKey;
	const char* tryToGetValue;
	#if !defined(__AML) && defined(_ICFG)
		tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
	#else
		tryToGetValue = ((inipp::Ini<char>*)m_iniMyConfig)->sections[szSection][szKey].c_str();
	#endif
	if(tryToGetValue[0] == '\0')
		pRet->SetInt(nDefaultValue);
	else
		pRet->SetString(tryToGetValue);
	Save();
	return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, float flDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return nullptr;
	ConfigEntry* pRet = new ConfigEntry;
	pRet->m_pBoundCfg = this;
	pRet->m_szMySection = szSection;
	pRet->m_szMyKey = szKey;
	const char* tryToGetValue;
	#if !defined(__AML) && defined(_ICFG)
		tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
	#else
		tryToGetValue = ((inipp::Ini<char>*)m_iniMyConfig)->sections[szSection][szKey].c_str();
	#endif
	if(tryToGetValue[0] == '\0')
		pRet->SetFloat(flDefaultValue);
	else
		pRet->SetString(tryToGetValue);
	Save();
	return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, bool bDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return nullptr;
	ConfigEntry* pRet = new ConfigEntry;
	pRet->m_pBoundCfg = this;
	pRet->m_szMySection = szSection;
	pRet->m_szMyKey = szKey;
	const char* tryToGetValue;
	#if !defined(__AML) && defined(_ICFG)
		tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
	#else
		tryToGetValue = ((inipp::Ini<char>*)m_iniMyConfig)->sections[szSection][szKey].c_str();
	#endif
	if(tryToGetValue[0] == '\0')
		pRet->SetBool(bDefaultValue);
	else
		pRet->SetString(tryToGetValue);
	Save();
	return pRet;
}

void ConfigEntry::SetString(const char* newValue)
{
	m_szValue = newValue;
	m_nIntegerValue = atoi(m_szValue);
	m_fFloatValue = (float)atof(m_szValue);

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}

void ConfigEntry::SetFloat(float newValue)
{
	m_fFloatValue = newValue;
    m_nIntegerValue = (int)newValue;
    
	char szVal[32];
	sprintf(szVal, "%f", newValue);
    m_szValue = szVal;

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}

void ConfigEntry::SetInt(int newValue)
{
	m_fFloatValue = (float)newValue;
    m_nIntegerValue = newValue;
    
	char szVal[32];
	sprintf(szVal, "%d", newValue);
    m_szValue = szVal;

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}

void ConfigEntry::SetBool(bool newValue)
{
	m_fFloatValue = newValue?1.0f:0.0f;
    m_nIntegerValue = newValue?1:0;
    m_szValue = newValue?"1":"0";

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}