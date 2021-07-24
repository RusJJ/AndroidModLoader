class Logger
{
public:
    Logger();
    void SetTag(const char* szTag);
    void Info(const char* szMessage, ...);
    void Error(const char* szMessage, ...);
    static Logger* GetLogger();
private:
    const char* m_szTag;
};
extern Logger* logger;