#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>

Logger::Logger()
{
    m_szTag = "AML Mod";
}

Logger* Logger::GetLogger()
{
    return logger;
}

void Logger::SetTag(const char* szTag)
{
    m_szTag = szTag;
}

void Logger::Info(const char* szMessage, ...)
{
    char buffer[384];
    va_list args;
    va_start(args, szMessage);
    vsprintf(buffer, szMessage, args);
    __android_log_write(ANDROID_LOG_INFO, m_szTag, buffer);
    va_end(args);
}

void Logger::Error(const char* szMessage, ...)
{
    char buffer[384];
    va_list args;
    va_start(args, szMessage);
    vsprintf(buffer, szMessage, args);
    __android_log_write(ANDROID_LOG_ERROR, m_szTag, buffer);
    va_end(args);
}

static Logger loggerLocal;
Logger* logger = &loggerLocal;