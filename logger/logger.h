//#include <string>

class Logger
{
public:
    Logger();
    static Logger* GetLogger();
    void SetTag(const char* szTag);
    void Info(const char* szMessage, ...);
    void Error(const char* szMessage, ...);
private:
    const char* m_szTag;
};
extern Logger* logger;