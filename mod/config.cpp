#ifndef DONT_USE_STB
    #include <mod/thirdparty/stb_sprintf.h>
    #define sprintf stbsp_sprintf
    #define snprintf stbsp_snprintf
#endif
#include "config.h"
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
	extern char g_szCfgPath[0xFF];
#endif



extern ModInfo* modinfo;
ConfigEntry* Config::pLastEntry = NULL;

Config::Config(const char* szName)
{
#if !defined(__AML) && defined(_ICFG)
	m_pICFG = (ICFG*)GetInterface("AMLConfig");
	m_iniMyConfig = m_pICFG->InitIniPointer();
#else
	m_iniMyConfig = new inipp::Ini<char>();
#endif
	m_bInitialized = false;
    m_bValueChanged = false;
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
		char path[0xFF];
		#ifdef __AML
    		snprintf(path, sizeof(path), "%s/%s.ini", g_szCfgPath, m_szName);
			std::ifstream cfgStream(path);
		#else
    		snprintf(path, sizeof(path), "%s/%s.ini", aml->GetConfigPath(), m_szName);
			std::ifstream cfgStream(path);
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
	if(!m_bInitialized || !m_bValueChanged) return;
    
    m_bValueChanged = false;
	#if !defined(__AML) && defined(_ICFG)
		m_pICFG->GenerateToOutputStream(m_iniMyConfig, m_szName);
	#else
		char path[0xFF];
		#ifdef __AML
    		snprintf(path, sizeof(path), "%s/%s.ini", g_szCfgPath, m_szName);
			std::ofstream cfgStream(path);
		#else
    		snprintf(path, sizeof(path), "%s/%s.ini", aml->GetConfigPath(), m_szName);
			std::ofstream cfgStream(path);
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
	if(!m_bInitialized) return NULL;
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
    pLastEntry = pRet;
	return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, int nDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return NULL;
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
    pLastEntry = pRet;
	return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, float flDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return NULL;
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
    pLastEntry = pRet;
	return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, bool bDefaultValue, const char* szSection)
{
	if(!m_bInitialized) return NULL;
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
    pLastEntry = pRet;
	return pRet;
}

void ConfigEntry::SetString(const char* newValue)
{
    //if(m_szValue != NULL && !strcmp(newValue, m_szValue)) return;
    
	m_szValue = newValue;
	m_nIntegerValue = atoi(m_szValue);
	m_fFloatValue = (float)atof(m_szValue);
    
    m_pBoundCfg->m_bValueChanged = true;

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}

void ConfigEntry::SetFloat(float newValue)
{
    if(m_fFloatValue == newValue) return;
    
	m_fFloatValue = newValue;
    m_nIntegerValue = (int)newValue;
    
    char szVal[32];
    snprintf(szVal, sizeof(szVal), "%f", newValue);
    m_szValue = szVal;
    
    m_pBoundCfg->m_bValueChanged = true;

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}

void ConfigEntry::SetInt(int newValue)
{
    if(m_nIntegerValue == newValue) return;
    
	m_fFloatValue = (float)newValue;
    m_nIntegerValue = newValue;
    
	char szVal[32];
	snprintf(szVal, sizeof(szVal), "%d", newValue);
    m_szValue = szVal;
    
    m_pBoundCfg->m_bValueChanged = true;

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}

void ConfigEntry::SetBool(bool newValue)
{
    if(m_nIntegerValue == (int)newValue) return;
    
	m_fFloatValue = newValue?1.0f:0.0f;
    m_nIntegerValue = newValue?1:0;
    m_szValue = newValue?"1":"0";
    
    m_pBoundCfg->m_bValueChanged = true;

	#if !defined(__AML) && defined(_ICFG)
		m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
	#else
		((inipp::Ini<char>*)(m_pBoundCfg->m_iniMyConfig))->sections[m_szMySection][m_szMyKey] = m_szValue;
	#endif
}
