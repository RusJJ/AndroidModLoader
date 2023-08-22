#include <stdarg.h>
#include <fstream>
#include <time.h>
#include <aml.h>
#include <android/log.h>

void* hAndroidLog;
std::ofstream oAndroidLogFile;

#define ENUMERATE_THIS(_me) case _me: return #_me
const char* EnumPriority(int prio)
{
    switch(prio)
    {
        ENUMERATE_THIS(ANDROID_LOG_UNKNOWN);
        ENUMERATE_THIS(ANDROID_LOG_DEFAULT);
        ENUMERATE_THIS(ANDROID_LOG_VERBOSE);
        ENUMERATE_THIS(ANDROID_LOG_DEBUG);
        ENUMERATE_THIS(ANDROID_LOG_INFO);
        ENUMERATE_THIS(ANDROID_LOG_WARN);
        ENUMERATE_THIS(ANDROID_LOG_ERROR);
        ENUMERATE_THIS(ANDROID_LOG_FATAL);
        ENUMERATE_THIS(ANDROID_LOG_SILENT);
    }
    return "ANDROID_LOG_UNKNOWN";
}

DECL_HOOKv(__aml_log_print, int prio, const char *tag, const char *text)
{
    __aml_log_print(prio, tag, text);
    if(!text) return;
    if(!tag) tag = "Unknown Tag";

    time_t rawtime;
    time ( &rawtime );

    oAndroidLogFile << asctime(localtime ( &rawtime )) << " [" << EnumPriority(prio) << "][" << tag << "] " << text << std::endl << std::endl;
}

DECL_HOOKv(__aml_log_vprint, int prio, const char *tag, const char *fmt, va_list ap)
{
    if(!fmt) return;

    char text[512];
    vsprintf(text, fmt, ap);

    HookOf___aml_log_print(prio, tag, text);
}

void HookALog()
{
    hAndroidLog = aml->GetLibHandle("liblog.so");
    if(!hAndroidLog) return;

    uintptr_t __android_log_print_addr = aml->GetSym(hAndroidLog, "__android_log_print");
    uintptr_t __android_log_vprint_addr = aml->GetSym(hAndroidLog, "__android_log_vprint");

    char path[320];
    sprintf(path, "%s/android_log_print.txt", aml->GetAndroidDataRootPath());
    oAndroidLogFile.open(path, std::ios::out | std::ios::trunc);

    if(__android_log_print_addr) HOOK(__aml_log_print, __android_log_print_addr);
    if(__android_log_vprint_addr) HOOK(__aml_log_vprint, __android_log_vprint_addr);
}