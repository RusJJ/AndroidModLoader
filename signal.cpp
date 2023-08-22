#include <mod/logger.h>
#include <signal.h>
#include <fstream>
#include <unistd.h>
#include <dlfcn.h>
#include <aml.h>

#ifdef __arm__
    #define AML32
#elif defined __aarch64__
    #define AML64
#else
    #error This lib is supposed to work on ARM only!
#endif

std::ofstream g_pLogFile;

struct sigaction newSigaction[7];
struct sigaction oldSigaction[7];

int SignalInnerId(int code)
{
    switch(code)
    {
        case SIGABRT: return 0;
        case SIGBUS: return 1;
        case SIGFPE: return 2;
        case SIGSEGV: return 3;
        case SIGILL: return 4;
        case SIGSTKFLT: return 5;
        case SIGTRAP: return 6;
    }
    return -1;
}

#define ENUMERATE_THIS(_me) case _me: return #_me
const char* SignalEnum(int sig)
{
    switch(sig)
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

const char* CodeEnum(int sig, int code)
{
    switch(code)
    {
        ENUMERATE_THIS(SI_USER);
        ENUMERATE_THIS(SI_QUEUE);
        ENUMERATE_THIS(SI_TIMER);
        ENUMERATE_THIS(SI_ASYNCIO);
        ENUMERATE_THIS(SI_MESGQ);
        default:         break;
    }
    switch(sig)
    {
        case SIGILL:
        {
            switch(code)
            {
                ENUMERATE_THIS(ILL_ILLOPC);
                ENUMERATE_THIS(ILL_ILLOPN);
                ENUMERATE_THIS(ILL_ILLADR);
                ENUMERATE_THIS(ILL_ILLTRP);
                ENUMERATE_THIS(ILL_PRVOPC);
                ENUMERATE_THIS(ILL_PRVREG);
                ENUMERATE_THIS(ILL_COPROC);
                ENUMERATE_THIS(ILL_BADSTK);
            }
        }
        case SIGFPE:
        {
            switch(code)
            {
                ENUMERATE_THIS(FPE_INTDIV);
                ENUMERATE_THIS(FPE_INTOVF);
                ENUMERATE_THIS(FPE_FLTDIV);
                ENUMERATE_THIS(FPE_FLTOVF);
                ENUMERATE_THIS(FPE_FLTUND);
                ENUMERATE_THIS(FPE_FLTRES);
                ENUMERATE_THIS(FPE_FLTINV);
                ENUMERATE_THIS(FPE_FLTSUB);
            }
        }
        case SIGSEGV:
        {
            switch(code)
            {
                ENUMERATE_THIS(SEGV_MAPERR);
                ENUMERATE_THIS(SEGV_ACCERR);
            }
        }
        case SIGBUS:
        {
            switch(code)
            {
                ENUMERATE_THIS(BUS_ADRALN);
                ENUMERATE_THIS(BUS_ADRERR);
                ENUMERATE_THIS(BUS_OBJERR);

                #ifdef BUS_MCEEFF_AO
                    ENUMERATE_THIS(BUS_MCEEFF_AO);
                #endif
                #ifdef BUS_MCEERR_AR
                    ENUMERATE_THIS(BUS_MCEERR_AR);
                #endif
            }
        }
        case SIGTRAP:
        {
            switch(code)
            {
                ENUMERATE_THIS(TRAP_BRKPT);
                ENUMERATE_THIS(TRAP_TRACE);
                #ifdef TRAP_BRANCH
                    ENUMERATE_THIS(TRAP_BRANCH);
                #endif
                #ifdef TRAP_HWBKPT
                    ENUMERATE_THIS(TRAP_HWBKPT);
                #endif
            }
        }
        case SIGCHLD:
        {
            switch(code)
            {
                ENUMERATE_THIS(CLD_EXITED);
                ENUMERATE_THIS(CLD_KILLED);
                ENUMERATE_THIS(CLD_DUMPED);
                ENUMERATE_THIS(CLD_TRAPPED);
                ENUMERATE_THIS(CLD_STOPPED);
                ENUMERATE_THIS(CLD_CONTINUED);
            }
        }
#ifdef SIGPOLL
        case SIGPOLL:
        {
            switch(code)
            {
                ENUMERATE_THIS(POLL_IN);
                ENUMERATE_THIS(POLL_OUT);
                ENUMERATE_THIS(POLL_MSG);
                ENUMERATE_THIS(POLL_ERR);
                ENUMERATE_THIS(POLL_PRI);
                ENUMERATE_THIS(POLL_HUP);
            }
        }
#endif
    }
    return "UNKNOWN";
}

void Handler(int sig, siginfo_t *si, void *ptr)
{
    mcontext_t& mcontext = ((ucontext_t *)ptr)->uc_mcontext;
    
    #ifdef AML32
        uintptr_t PC = mcontext.arm_pc;
    #else
        uintptr_t PC = mcontext.pc;
    #endif
    uintptr_t faultAddr = mcontext.fault_address; // == si->si_addr?

    // Java doesnt work here and so crashing again and again?
    //aml->ShowToast(true, "Application has been crashed!");
    logger->Error("Exception Signal %d - %s (%s)", sig, SignalEnum(sig), CodeEnum(sig, si->si_code));

    Dl_info dlInfo;
    if(dladdr((void*)PC, &dlInfo) != 0)
    {
        // Success
        if(dlInfo.dli_fname)
        {
            logger->Error("Library: %s + 0x%08X", dlInfo.dli_fname, PC - (uintptr_t)dlInfo.dli_fbase);
        }
        else
        {

        }
    }
    else
    {
        // Unsuccess
        
    }
    oldSigaction[SignalInnerId(sig)].sa_sigaction(sig, si, ptr);
    exit(0);
}

#define HANDLESIG(_code) sigbreak = newSigaction + SignalInnerId(_code); sigbreak->sa_sigaction = &Handler; \
                         sigbreak->sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND; sigemptyset(&sigbreak->sa_mask); sigaction(_code, sigbreak, oldSigaction + SignalInnerId(_code))
void StartSignalHandler()
{
    static char stack[SIGSTKSZ];
    stack_t ss;
    ss.ss_sp = stack;
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    struct sigaction* sigbreak = NULL;

    HANDLESIG(SIGABRT);
    HANDLESIG(SIGBUS);
    HANDLESIG(SIGFPE);
    HANDLESIG(SIGSEGV);
    HANDLESIG(SIGILL);
    HANDLESIG(SIGSTKFLT);
    HANDLESIG(SIGTRAP);
}