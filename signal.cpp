#include <mod/logger.h>
#include <signal.h>
#include <fstream>
#include <aml.h>

std::ofstream g_pLogFile;

const char* SignalEnum(int code)
{
    #define ENUMERATE_THIS(_code) case _code: return #_code
    switch(code)
    {
        ENUMERATE_THIS(SIGABRT);
        ENUMERATE_THIS(SIGBUS);
        ENUMERATE_THIS(SIGFPE);
        ENUMERATE_THIS(SIGSEGV);
        ENUMERATE_THIS(SIGILL);
        ENUMERATE_THIS(SIGSTKFLT);
        ENUMERATE_THIS(SIGTRAP);
    }
    return "UNKNOWN";
}

void Handler(int code)
{
    aml->ShowToast(true, "Application has been crashed!");
    logger->Info("Exception code 0x%08X - %s", code, SignalEnum(code));
    exit(0);
}

#define HANDLESIG(_code) sigaction(_code, &sigbreak, NULL)
void StartSignalHandler()
{
    struct sigaction sigbreak;
    sigbreak.sa_handler = &Handler;
    sigbreak.sa_flags = 0;
    sigemptyset(&sigbreak.sa_mask);

    HANDLESIG(SIGABRT);
    HANDLESIG(SIGBUS);
    HANDLESIG(SIGFPE);
    HANDLESIG(SIGSEGV);
    HANDLESIG(SIGILL);
    HANDLESIG(SIGSTKFLT);
    HANDLESIG(SIGTRAP);
}