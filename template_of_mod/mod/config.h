#ifndef _CONFIG
#define _CONFIG

#define KEY_SECTION_BUFFER_C 64
#define VALUE_BUFFER_C 384

/* Is not required. Can be used only for a smaller size of mod (~480kb savings) */
#include "icfg.h"
#include <stdint.h>

class ConfigEntry;

struct rgba_t
{
    union {
        struct { unsigned char r, g, b, a; };
        struct { unsigned char x, y, z, w; };
        unsigned char v[4];
        unsigned int value;
    };
    
    rgba_t() : r(0), g(0), b(0), a(0) {}
    rgba_t(unsigned char v) : r(v), g(v), b(v), a(255) {}
    rgba_t(unsigned char _r, unsigned char _g, unsigned char _b) : r(_r), g(_g), b(_b), a(255) {}
    rgba_t(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a) : r(_r), g(_g), b(_b), a(_a) {}
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
    ConfigEntry* Bind(const char* szKey, rgba_t clrDefaultValue, const char* szSection = "Preferences");
    
    // FAST GET. NO NEED TO CLEAN THE MEMORY.
    const char*  GetString(const char* szKey, const char* szDefaultValue, const char* szSection = "Preferences");
    int          GetInt(const char* szKey, int nDefaultValue, const char* szSection = "Preferences");
    float        GetFloat(const char* szKey, float flDefaultValue, const char* szSection = "Preferences");
    bool         GetBool(const char* szKey, bool bDefaultValue, const char* szSection = "Preferences");
    rgba_t       GetColor(const char* szKey, rgba_t clrDefaultValue, const char* szSection = "Preferences");

    // Self-explained
    inline bool  IsValueChanged() { return m_bValueChanged; }
    inline void  ClearLast();
    
    static Config* GetConfig();
    static ConfigEntry* pLastEntry;
    
private:
    bool m_bInitialized;
    bool m_bValueChanged;
    const char* m_szName;
    void* m_iniMyConfig;

#ifdef _ICFG
    /* Built-in optimizer thinks he's the best! Ha-ha... Not funny. It's 3AM... */
    ICFG* m_pICFG;
#endif
    
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

    inline bool LoadedUndefault() { return m_bNotDefaultValue; }
    inline int Clamp(int min, int max)
    {
        if(m_nIntegerValue < min)
        {
            m_pBoundCfg->m_bValueChanged = true;
            int ret = m_nIntegerValue - min;
            m_nIntegerValue = min;
            m_fFloatValue = (float)min;
            m_szValue[0] = 0;
            return ret;
        }
        if(m_nIntegerValue > max)
        {
            m_pBoundCfg->m_bValueChanged = true;
            int ret = m_nIntegerValue - max;
            m_nIntegerValue = max;
            m_fFloatValue = (float)max;
            m_szValue[0] = 0;
            return ret;
        }
        return 0;
    }
    inline float Clamp(float min, float max)
    {
        if(m_fFloatValue < min)
        {
            m_pBoundCfg->m_bValueChanged = true;
            float ret = m_fFloatValue - min;
            m_nIntegerValue = (int)min;
            m_fFloatValue = min;
            m_szValue[0] = 0;
            return ret;
        }
        if(m_fFloatValue > max)
        {
            m_pBoundCfg->m_bValueChanged = true;
            float ret = m_fFloatValue - max;
            m_nIntegerValue = (int)max;
            m_fFloatValue = max;
            m_szValue[0] = 0;
            return ret;
        }
        return 0.0f;
    }
    
private:
    Config* m_pBoundCfg;
    bool m_bLoadedData;
    bool m_bNotDefaultValue;
    char m_szMySection[KEY_SECTION_BUFFER_C];
    char m_szMyKey[KEY_SECTION_BUFFER_C];
    float m_fFloatValue;
    union
    {
        int m_nIntegerValue;
        rgba_t m_ColorValue;
    };
    char m_szValue[VALUE_BUFFER_C];
    char m_szDefaultValue[VALUE_BUFFER_C];

    friend class Config;
};
inline void Config::ClearLast() { if(pLastEntry) { delete pLastEntry; pLastEntry = NULL; } }
extern Config* cfg;

#endif // _CONFIG
