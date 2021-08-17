#ifndef _CONFIG
#define _CONFIG

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
    static Config* GetConfig();
private:
    bool m_bInitialized;
    const char* m_szName;
    void* m_iniMyConfig;

#ifdef _ICFG
    /* Built-in optimizer think he's best! Ha-ha... Not funny. It's 3AM... */
    ICFG* m_pICFG;
#endif
    friend class ConfigEntry;
};

class ConfigEntry
{
public:
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
    const char* m_szValue;
    float m_fFloatValue;
    int m_nIntegerValue;

    friend class Config;
};
extern Config* cfg;

#endif // _CONFIG