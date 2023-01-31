#ifndef _CONFIG
#define _CONFIG

#define VALUE_BUFFER_C 512

/* Is not required. Can be used only for a smaller size of mod (~480kb savings) */
#include "icfg.h"

class ConfigEntry;

struct rgba_t
{
    union {
        struct { unsigned char r,g,b,a; };
        unsigned int value;
    };
};

class Config
{
public:
    Config(const char* szName);
    void Init();
    void Save();
    // Allocated, needs to be manually deleted
    ConfigEntry* Bind(const char* szKey, const char* szDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* Bind(const char* szKey, int nDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* Bind(const char* szKey, float flDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* Bind(const char* szKey, bool bDefaultValue, const char* szSection = "Preferences");
    // Sits in a stack, can be used only in a single function
    // SHOULD NOT BE DELETED.
    ConfigEntry* BindOnce(const char* szKey, const char* szDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* BindOnce(const char* szKey, int nDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* BindOnce(const char* szKey, float flDefaultValue, const char* szSection = "Preferences");
    ConfigEntry* BindOnce(const char* szKey, bool bDefaultValue, const char* szSection = "Preferences");
    // Self-explained
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
    ConfigEntry() : m_bLoadedData(false), m_szValue(""), m_szDefaultValue("") {}
    void SetString(const char* newValue);
    inline const char* GetString() { return m_szValue; }
    void GetString(char* str, size_t len);
    void SetFloat(float newValue);
    inline float GetFloat() { return m_fFloatValue; }
    void SetBool(bool newValue);
    inline bool GetBool() { return m_nIntegerValue; }
    void SetInt(int newValue);
    inline int GetInt() { return m_nIntegerValue; }
    inline void Reset() { SetString(m_szDefaultValue); }
    rgba_t ParseColor();
    void SetColor(rgba_t clr, bool asFloat = false);
    
private:
    Config* m_pBoundCfg;
    bool m_bLoadedData;
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
