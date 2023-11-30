#include <stdarg.h>
#include <fstream>
#include <time.h>
#include <aml.h>
#include <mod/logger.h>
#include <android/log.h>

void* hAndroidLog;
bool bAndroidLog_OnlyImportant, bAndroidLog_NoAfter;
std::ofstream oAndroidLogFile;

inline const char* EnumPriority(int prio)
{
    switch(prio)
    {
        case ANDROID_LOG_UNKNOWN: return "UNKNOWN";
        case ANDROID_LOG_DEFAULT: return "DEFAULT";
        case ANDROID_LOG_VERBOSE: return "VERBOSE";
        case ANDROID_LOG_DEBUG:   return "DEBUG";
        case ANDROID_LOG_INFO:    return "INFO";
        case ANDROID_LOG_WARN:    return "WARN";
        case ANDROID_LOG_ERROR:   return "ERROR";
        case ANDROID_LOG_FATAL:   return "FATAL";
        case ANDROID_LOG_SILENT:  return "SILENT";
    }
    return "UNKNOWN";
}
inline bool IsImportantLogLevel(int prio)
{
    switch(prio)
    {
        case ANDROID_LOG_DEBUG:
        case ANDROID_LOG_WARN:
        case ANDROID_LOG_ERROR:
        case ANDROID_LOG_FATAL:
            return true;
    }
    return false;
}

char text[4096]; // Log texts are not bigger than 4kb
extern DECL_HOOKv(__aml_log_print, int prio, const char *tag, const char *fmt, ...);
DECL_HOOKv(__aml_log_vprint, int prio, const char *tag, const char *fmt, va_list ap)
{
    if(!fmt) return;
    if(!tag) tag = "AML: Untagged sender";

    vsprintf(text, fmt, ap);
    __aml_log_print(prio, tag, text);

    if(bAndroidLog_OnlyImportant && !IsImportantLogLevel(prio)) return;

    time_t rawtime;
    time ( &rawtime );

    oAndroidLogFile << asctime(localtime ( &rawtime )) << " [" << EnumPriority(prio) << "][" << tag << "] " << text << std::endl << std::endl;
}

DECL_HOOKv(__aml_log_print, int prio, const char *tag, const char *fmt, ...)
{
    if(!fmt) return;

    va_list ap;
    va_start(ap, fmt);
    if(!bAndroidLog_NoAfter) HookOf___aml_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
}

void HookALog()
{
    hAndroidLog = aml->GetLibHandle("liblog.so");
    if(!hAndroidLog) return;

    uintptr_t __android_log_print_addr = aml->GetSym(hAndroidLog, "__android_log_print");
    uintptr_t __android_log_vprint_addr = aml->GetSym(hAndroidLog, "__android_log_vprint");

    if(!__android_log_print_addr && !__android_log_vprint_addr)
    {
        logger->Error("AML Core just failed to patch logs function!");
        return;
    }

    char path[320];
    sprintf(path, "%s/android_log_print.txt", aml->GetAndroidDataRootPath());
    oAndroidLogFile.open(path, std::ios::out | std::ios::trunc);

    if(oAndroidLogFile.is_open())
    {
        if(__android_log_print_addr) HOOK(__aml_log_print, __android_log_print_addr);
        if(__android_log_vprint_addr) HOOK(__aml_log_vprint, __android_log_vprint_addr);
    }
    else
    {
      an_epic_fail_ever:
        logger->Error("AML Core just failed to open log file!");
    }
}