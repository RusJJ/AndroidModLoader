#ifndef _CONFIG
#define _CONFIG

#define VALUE_BUFFER_C 512

/* Is not required. Can be used only for a smaller size of mod (~480kb savings) */
#include "icfg.h"

class ConfigEntry;

class Config
{
public:
    Config(const char* szName);
    void Init();
    void Save();
    ConfigEntry* Bind(const char* szKey, const char* szDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* Bind(const char* szKey, int nDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* Bind(const char* szKey, float flDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* Bind(const char* szKey, bool bDefaultValue, const char* szSection = "Preferences");
    inline bool IsValueChanged() { return m_bValueChanged; }
    
    static Config* GetConfig();
    static ConfigEntry* pLastEntry;
private:
    bool m_bInitialized;
    const char* m_szName;
    void* m_iniMyConfig;

#ifdef _ICFG
    /* Built-in optimizer think he's best! Ha-ha... Not funny. It's 3AM... */
    ICFG* m_pICFG;
#endif
    
    bool m_bValueChanged;
    
    friend class ConfigEntry;
};

class ConfigEntry
{
public:
    ConfigEntry() : m_szValue("") {}
    void SetString(const char* newValue);
    inline const char* GetString() { return m_szValue; }
    void SetFloat(float newValue);
    inline float GetFloat() { return m_fFloatValue; }
    void SetBool(bool newValue);
    inline bool GetBool() { return m_nIntegerValue; }
    void SetInt(int newValue);
    inline int GetInt() { return m_nIntegerValue; }
private:
    Config* m_pBoundCfg;
    const char* m_szMySection;
    const char* m_szMyKey;
    float m_fFloatValue;
    int m_nIntegerValue;
    char m_szValue[VALUE_BUFFER_C];
    char m_szDefaultValue[VALUE_BUFFER_C];

    friend class Config;
};
extern Config* cfg;

#endif // _CONFIG
