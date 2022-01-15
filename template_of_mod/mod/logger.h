#ifndef _LOGGER
#define _LOGGER

#include <stdio.h>

class Logger
{
public:
    Logger();
    void SetTag(const char* szTag);
    void Info(const char* szMessage, ...);
    void InfoV(const char* szMessage, va_list args);
    void Error(const char* szMessage, ...);
    void ErrorV(const char* szMessage, va_list args);
    static Logger* GetLogger();
private:
    const char* m_szTag;
};
extern Logger* logger;

#endif // _LOGGER