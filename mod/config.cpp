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
    #include "thirdparty/inicpp.h"
    ini::IniFile hINI;
#endif
#ifdef __AML
    extern char g_szCfgPath[0xFF];
#endif

inline bool str_equal(const char* str1, const char* str2)
{ 
    for( ; *str1 == *str2 && *str1 != 0; ++str1, ++str2 ) {}
    return *str2 == *str1; 
}

extern ModInfo* modinfo;
ConfigEntry* Config::pLastEntry = NULL;

Config::Config(const char* szName)
{
#if !defined(__AML) && defined(_ICFG)
    m_pICFG = (ICFG*)GetInterface("AMLConfig");
    m_iniMyConfig = m_pICFG->InitIniPointer();
#else
    m_iniMyConfig = &hINI;
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
        #else
            snprintf(path, sizeof(path), "%s/%s.ini", aml->GetConfigPath(), m_szName);
        #endif
        hINI.load(path);
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
        #else
            snprintf(path, sizeof(path), "%s/%s.ini", aml->GetConfigPath(), m_szName);
        #endif
        hINI.save(path);
    #endif
}

ConfigEntry* Config::Bind(const char* szKey, const char* szDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return NULL;
    ConfigEntry* pRet = new ConfigEntry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    strxcpy(pRet->m_szDefaultValue, szDefaultValue, sizeof(pRet->m_szDefaultValue));
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        pRet->m_bNotDefaultValue = false;
        pRet->SetString(szDefaultValue);
    }
    else 
    {
        bool bShouldChange = !pRet->m_pBoundCfg->m_bValueChanged;
        pRet->SetString(tryToGetValue);
        pRet->m_bNotDefaultValue = strcmp(tryToGetValue, szDefaultValue) != 0;
        if(bShouldChange) pRet->m_pBoundCfg->m_bValueChanged = false;
    }
    Save();
    pLastEntry = pRet;
    return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, int nDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return NULL;
    ConfigEntry* pRet = new ConfigEntry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    snprintf(pRet->m_szDefaultValue, sizeof(pRet->m_szDefaultValue), "%d", nDefaultValue);
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        pRet->m_bNotDefaultValue = false;
        pRet->SetInt(nDefaultValue);
    }
    else
    {
        bool bShouldChange = !pRet->m_pBoundCfg->m_bValueChanged;
        pRet->SetString(tryToGetValue);
        pRet->m_bNotDefaultValue = (nDefaultValue != pRet->m_nIntegerValue);
        if(bShouldChange) pRet->m_pBoundCfg->m_bValueChanged = false;
    }
    Save();
    pLastEntry = pRet;
    return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, float flDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return NULL;
    ConfigEntry* pRet = new ConfigEntry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    snprintf(pRet->m_szDefaultValue, sizeof(pRet->m_szDefaultValue), "%f", flDefaultValue);
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        pRet->m_bNotDefaultValue = false;
        pRet->SetFloat(flDefaultValue);
    }
    else
    {
        bool bShouldChange = !pRet->m_pBoundCfg->m_bValueChanged;
        pRet->SetString(tryToGetValue);
        pRet->m_bNotDefaultValue = (flDefaultValue != pRet->m_fFloatValue);
        if(bShouldChange) pRet->m_pBoundCfg->m_bValueChanged = false;
    }
    Save();
    pLastEntry = pRet;
    return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, bool bDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return NULL;
    ConfigEntry* pRet = new ConfigEntry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    pRet->m_szDefaultValue[0] = bDefaultValue ? '1' : '0'; pRet->m_szDefaultValue[1] = 0;
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        pRet->m_bNotDefaultValue = false;
        pRet->SetBool(bDefaultValue);
    }
    else
    {
        bool bShouldChange = !pRet->m_pBoundCfg->m_bValueChanged;
        pRet->SetString(tryToGetValue);
        pRet->m_bNotDefaultValue = (bDefaultValue != (!!pRet->m_nIntegerValue));
        if(bShouldChange) pRet->m_pBoundCfg->m_bValueChanged = false;
    }
    Save();
    pLastEntry = pRet;
    return pRet;
}

ConfigEntry* Config::Bind(const char* szKey, rgba_t clr, const char* szSection)
{
    if(!m_bInitialized) return NULL;
    ConfigEntry* pRet = new ConfigEntry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    snprintf(pRet->m_szDefaultValue, sizeof(pRet->m_szDefaultValue), "%d %d %d %d", (int)clr.r, (int)clr.g, (int)clr.b, (int)clr.a);
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        pRet->m_bNotDefaultValue = false;
        pRet->SetString(pRet->m_szDefaultValue);
    }
    else
    {
        bool bShouldChange = !pRet->m_pBoundCfg->m_bValueChanged;
        pRet->SetString(tryToGetValue);
        pRet->m_bNotDefaultValue = (clr.value != pRet->m_ColorValue.value);
        if(bShouldChange) pRet->m_pBoundCfg->m_bValueChanged = false;
    }
    Save();
    pLastEntry = pRet;
    return pRet;
}

const char* Config::GetString(const char* szKey, const char* szDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return NULL;
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        m_bValueChanged = true;
        #if !defined(__AML) && defined(_ICFG)
            m_pICFG->SetValueTo(m_iniMyConfig, szSection, szKey, szDefaultValue);
        #else
            hINI[szSection][szKey] = szDefaultValue;
        #endif
        Save();
        return szDefaultValue;
    }
    return tryToGetValue;
}

int Config::GetInt(const char* szKey, int nDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return 0;
    ConfigEntry entry; ConfigEntry* pRet = &entry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    snprintf(pRet->m_szDefaultValue, sizeof(pRet->m_szDefaultValue), "%d", nDefaultValue);
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        m_bValueChanged = true;
        #if !defined(__AML) && defined(_ICFG)
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "%d", nDefaultValue);
            m_pICFG->SetValueTo(m_iniMyConfig, szSection, szKey, tmp);
        #else
            hINI[szSection][szKey] = nDefaultValue;
        #endif
        Save();
        return nDefaultValue;
    }
    return atoi(tryToGetValue);
}

float Config::GetFloat(const char* szKey, float flDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return 0.0f;
    ConfigEntry entry; ConfigEntry* pRet = &entry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    snprintf(pRet->m_szDefaultValue, sizeof(pRet->m_szDefaultValue), "%f", flDefaultValue);
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        m_bValueChanged = true;
        #if !defined(__AML) && defined(_ICFG)
            char tmp[24];
            snprintf(tmp, sizeof(tmp), "%f", flDefaultValue);
            m_pICFG->SetValueTo(m_iniMyConfig, szSection, szKey, tmp);
        #else
            hINI[szSection][szKey] = flDefaultValue;
        #endif
        Save();
        return flDefaultValue;
    }
    return atof(tryToGetValue);
}

bool Config::GetBool(const char* szKey, bool bDefaultValue, const char* szSection)
{
    if(!m_bInitialized) return false;
    ConfigEntry entry; ConfigEntry* pRet = &entry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    pRet->m_szDefaultValue[0] = bDefaultValue ? '1' : '0'; pRet->m_szDefaultValue[1] = 0;
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
    {
        m_bValueChanged = true;
        #if !defined(__AML) && defined(_ICFG)
            m_pICFG->SetValueTo(m_iniMyConfig, szSection, szKey, bDefaultValue ? "1" : "0");
        #else
            hINI[szSection][szKey] = bDefaultValue ? "1" : "0";
        #endif
        Save();
        return bDefaultValue;
    }
    return atoi(tryToGetValue)!=0;
}

rgba_t Config::GetColor(const char* szKey, rgba_t clr, const char* szSection)
{
    if(!m_bInitialized) return rgba_t {0,0,0,0};
    ConfigEntry entry; ConfigEntry* pRet = &entry;
    pRet->m_pBoundCfg = this;
    strxcpy(pRet->m_szMySection, szSection, sizeof(pRet->m_szMySection));
    strxcpy(pRet->m_szMyKey, szKey, sizeof(pRet->m_szMyKey));
    snprintf(pRet->m_szDefaultValue, sizeof(pRet->m_szDefaultValue), "%d %d %d %d", (int)clr.r, (int)clr.g, (int)clr.b, (int)clr.a);
    const char* tryToGetValue;
    #if !defined(__AML) && defined(_ICFG)
        tryToGetValue = m_pICFG->GetValueFrom(m_iniMyConfig, szSection, szKey);
    #else
        tryToGetValue = hINI[szSection][szKey].as<const char*>();
    #endif
    if(tryToGetValue[0] == '\0')
        pRet->SetString(pRet->m_szDefaultValue);
    else
    {
        bool bShouldChange = !pRet->m_pBoundCfg->m_bValueChanged;
        pRet->SetString(tryToGetValue);
        if(bShouldChange) pRet->m_pBoundCfg->m_bValueChanged = false;
    }
    Save();
    return pRet->ParseColor();
}

void ConfigEntry::SetString(const char* newValue)
{
    if(m_bLoadedData && str_equal(newValue, m_szValue)) return;
    
    strxcpy(m_szValue, newValue, sizeof(m_szValue)-1); m_szValue[sizeof(m_szValue)-1] = 0;
    m_nIntegerValue = atoi(m_szValue);
    m_fFloatValue = (float)atof(m_szValue);
    
    m_pBoundCfg->m_bValueChanged = true;
    m_bLoadedData = true;

    #if !defined(__AML) && defined(_ICFG)
        m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
    #else
        hINI[m_szMySection][m_szMyKey] = m_szValue;
    #endif
}

void ConfigEntry::GetString(char* str, size_t len)
{
    strxcpy(str, GetString(), len);
}

void ConfigEntry::SetFloat(float newValue)
{
    if(m_bLoadedData && m_fFloatValue == newValue) return;
    
    m_fFloatValue = newValue;
    m_nIntegerValue = (int)newValue;
    snprintf(m_szValue, sizeof(m_szValue), "%f", newValue);
    
    m_pBoundCfg->m_bValueChanged = true;
    m_bLoadedData = true;

    #if !defined(__AML) && defined(_ICFG)
        m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
    #else
        hINI[m_szMySection][m_szMyKey] = m_szValue;
    #endif
}

void ConfigEntry::SetInt(int newValue)
{
    if(m_bLoadedData && m_nIntegerValue == newValue) return;
    
    m_fFloatValue = (float)newValue;
    m_nIntegerValue = newValue;
    snprintf(m_szValue, sizeof(m_szValue), "%d", newValue);
    
    m_pBoundCfg->m_bValueChanged = true;
    m_bLoadedData = true;

    #if !defined(__AML) && defined(_ICFG)
        m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
    #else
        hINI[m_szMySection][m_szMyKey] = m_szValue;
    #endif
}

void ConfigEntry::SetBool(bool newValue)
{
    if(m_bLoadedData && m_nIntegerValue == newValue?1:0) return;
    
    m_fFloatValue = newValue?1.0f:0.0f;
    m_nIntegerValue = newValue?1:0;
    m_szValue[0] = newValue ? '1' : '0'; m_szValue[1] = 0;
    
    m_pBoundCfg->m_bValueChanged = true;
    m_bLoadedData = true;

    #if !defined(__AML) && defined(_ICFG)
        m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
    #else
        hINI[m_szMySection][m_szMyKey] = m_szValue;
    #endif
}

inline bool IsRGBValue(int value) { return value >= 0 && value <= 255; }
inline bool IsRGBFloatValue(float value) { return value >= 0 && value <= 1; }
rgba_t ConfigEntry::ParseColor()
{
    int r, g, b, a, sscanfed = sscanf(m_szValue, "%d %d %d %d", &r, &g, &b, &a);
    if(sscanfed == 4 && IsRGBValue(r) && IsRGBValue(g) && IsRGBValue(b) && IsRGBValue(a))
    {
        m_ColorValue = rgba_t{(unsigned char)r,(unsigned char)g,(unsigned char)b,(unsigned char)a};
    }
    else if(sscanfed == 3 && IsRGBValue(r) && IsRGBValue(g) && IsRGBValue(b))
    {
        m_ColorValue = rgba_t{(unsigned char)r,(unsigned char)g,(unsigned char)b,255};
    }
    else
    {
        float fr, fg, fb, fa;
        sscanfed = sscanf(m_szValue, "%f %f %f %f", &fr, &fg, &fb, &fa);
        if(sscanfed == 4 && IsRGBFloatValue(r) && IsRGBFloatValue(g) && IsRGBFloatValue(b) && IsRGBFloatValue(a))
        {
            m_ColorValue = rgba_t{(unsigned char)(255*fr),(unsigned char)(255*fg),(unsigned char)(255*fb),(unsigned char)(255*fa)};
        }
        else if(sscanfed == 3 && IsRGBFloatValue(r) && IsRGBFloatValue(g) && IsRGBFloatValue(b))
        {
            m_ColorValue = rgba_t{(unsigned char)(255*fr),(unsigned char)(255*fg),(unsigned char)(255*fb),255};
        }
    }
    //m_ColorValue = rgba_t{255,255,255,255}
    return m_ColorValue;
}

void ConfigEntry::SetColor(rgba_t clr, bool asFloat)
{
    m_nIntegerValue = (int)clr.r;
    m_fFloatValue = (float)clr.r;
    if(asFloat) snprintf(m_szValue, sizeof(m_szValue), "%.3f %.3f %.3f %.3f", (float)(clr.r/255.0f), (float)(clr.g/255.0f), (float)(clr.b/255.0f), (float)(clr.a/255.0f));
    else snprintf(m_szValue, sizeof(m_szValue), "%d %d %d %d", (int)clr.r, (int)clr.g, (int)clr.b, (int)clr.a);
    
    // Kinda expensive to parse the color every time
    // Why do you may want it to be changed automatically anyway?
    m_pBoundCfg->m_bValueChanged = true;
    m_bLoadedData = true;

    #if !defined(__AML) && defined(_ICFG)
        m_pBoundCfg->m_pICFG->SetValueTo(m_pBoundCfg->m_iniMyConfig, m_szMySection, m_szMyKey, m_szValue);
    #else
        hINI[m_szMySection][m_szMyKey] = m_szValue;
    #endif
}
