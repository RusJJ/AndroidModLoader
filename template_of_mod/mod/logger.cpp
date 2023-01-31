#ifndef DONT_USE_STB
    #ifndef DONT_IMPLEMENT_STB
        #define STB_SPRINTF_IMPLEMENTATION
    #endif
    #include <mod/thirdparty/stb_sprintf.h>

    #define vsnprintf stbsp_vsnprintf
#endif
#include "logger.h"
#include <android/log.h>

Logger::Logger()
{
    m_szTag = "AML Mod";
    m_bEnabled = true;
}

Logger* Logger::GetLogger()
{
    return logger;
}

void Logger::ToggleOutput(bool enabled)
{
    m_bEnabled = enabled;
}

void Logger::SetTag(const char* szTag)
{
    m_szTag = szTag;
}

void Logger::Print(eLogPrio prio, const char* szMessage, ...)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    va_list args;
    va_start(args, szMessage);
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write((android_LogPriority)prio, m_szTag, buffer);
    va_end(args);
}

void Logger::PrintV(eLogPrio prio, const char* szMessage, va_list args)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write((android_LogPriority)prio, m_szTag, buffer);
}

void Logger::PrintTag(eLogPrio prio, const char* szTag, const char* szMessage, ...)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    va_list args;
    va_start(args, szMessage);
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write((android_LogPriority)prio, m_szTag, buffer);
    va_end(args);
}

void Logger::PrintTagV(eLogPrio prio, const char* szTag, const char* szMessage, va_list args)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write((android_LogPriority)prio, m_szTag, buffer);
}

void Logger::Info(const char* szMessage, ...)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    va_list args;
    va_start(args, szMessage);
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write(ANDROID_LOG_INFO, m_szTag, buffer);
    va_end(args);
}

void Logger::InfoV(const char* szMessage, va_list args)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write(ANDROID_LOG_INFO, m_szTag, buffer);
}

void Logger::Error(const char* szMessage, ...)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    va_list args;
    va_start(args, szMessage);
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write(ANDROID_LOG_ERROR, m_szTag, buffer);
    va_end(args);
}

void Logger::ErrorV(const char* szMessage, va_list args)
{
    if(!m_bEnabled) return;
    
    char buffer[TMPBUF_SIZE];
    vsnprintf(buffer, sizeof(buffer), szMessage, args);
    __android_log_write(ANDROID_LOG_ERROR, m_szTag, buffer);
}

static Logger loggerLocal;
Logger* logger = &loggerLocal;
