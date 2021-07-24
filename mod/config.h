class ConfigEntry;

class Config
{
public:
    Config(const char* szName);
    void Init();
    void Save();
    ConfigEntry* Bind(const char* szKey, const char* szDefaultValue);
    static Config* GetConfig();
private:
    bool m_bInitialized;
    const char* m_szName;
    void* m_iniMyConfig;

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
    const char* m_szMyKey;
    const char* m_szValue;
    float m_fFloatValue;
    int m_nIntegerValue;

    friend class Config;
};
extern Config* cfg;