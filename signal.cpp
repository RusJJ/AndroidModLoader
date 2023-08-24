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

bool bHasHandledError = false;
void Handler(int sig, siginfo_t *si, void *ptr)
{
    if(bHasHandledError)
    {
        exit(0);
        return;
    }
    bHasHandledError = true;

    ucontext_t* ucontext = (ucontext_t*)ptr;
    mcontext_t* mcontext = &ucontext->uc_mcontext;
    
    #ifdef AML32
        uintptr_t PC = mcontext->arm_pc;
    #else
        uintptr_t PC = mcontext->pc;
    #endif
    uintptr_t faultAddr = mcontext->fault_address; // == si->si_addr?

    char path[320], pathText[320];
    sprintf(path, "%s/aml_crashlog.txt", aml->GetAndroidDataRootPath());
    sprintf(pathText, "Application has been crashed!\n\nCrashlog has been saved in %s", path);
    g_pLogFile.open(path, std::ios::out | std::ios::trunc);

    // Java doesnt work here and so crashing again and again?
    //aml->ShowToast(true, pathText);
    logger->Error("Exception Signal %d - %s (%s)", sig, SignalEnum(sig), CodeEnum(sig, si->si_code));
    logger->Error("Crashlog has been saved in %s", path);

    if(!g_pLogFile.is_open()) goto skip_logging;

    g_pLogFile << "Exception Signal " << sig << " - " << SignalEnum(sig) << "(" << CodeEnum(sig, si->si_code) << ")" << std::endl;

    Dl_info dlInfo;
    if(dladdr((void*)PC, &dlInfo) != 0)
    {
        // Success
        if(dlInfo.dli_fname)
        {
            g_pLogFile << "Program counter: " << dlInfo.dli_fname << " + 0x" << std::hex << (PC - (uintptr_t)dlInfo.dli_fbase) << std::endl;
        }
        else
        {
            if(!dlInfo.dli_fbase) goto label_unsuccess;
            g_pLogFile << "Program counter: Unknown Lib + 0x" << std::hex << (PC - (uintptr_t)dlInfo.dli_fbase) << std::endl;
        }
    }
    else
    {
        // Unsuccess
      label_unsuccess:
        g_pLogFile << "Program counter: Unknown Lib, 0x" << std::hex << PC << std::endl;
    }

    g_pLogFile << std::endl << "Registers data:" << std::endl;
    #ifdef AML32
        g_pLogFile << "R0:   " << mcontext->arm_r0 <<   " 0x" << std::hex << mcontext->arm_r0 << std::endl;
        g_pLogFile << "R1:   " << mcontext->arm_r1 <<   " 0x" << std::hex << mcontext->arm_r1 << std::endl;
        g_pLogFile << "R2:   " << mcontext->arm_r2 <<   " 0x" << std::hex << mcontext->arm_r2 << std::endl;
        g_pLogFile << "R3:   " << mcontext->arm_r3 <<   " 0x" << std::hex << mcontext->arm_r3 << std::endl;
        g_pLogFile << "R4:   " << mcontext->arm_r4 <<   " 0x" << std::hex << mcontext->arm_r4 << std::endl;
        g_pLogFile << "R5:   " << mcontext->arm_r5 <<   " 0x" << std::hex << mcontext->arm_r5 << std::endl;
        g_pLogFile << "R6:   " << mcontext->arm_r6 <<   " 0x" << std::hex << mcontext->arm_r6 << std::endl;
        g_pLogFile << "R7:   " << mcontext->arm_r7 <<   " 0x" << std::hex << mcontext->arm_r7 << std::endl;
        g_pLogFile << "R8:   " << mcontext->arm_r8 <<   " 0x" << std::hex << mcontext->arm_r8 << std::endl;
        g_pLogFile << "R9:   " << mcontext->arm_r9 <<   " 0x" << std::hex << mcontext->arm_r9 << std::endl;
        g_pLogFile << "R10:  " << mcontext->arm_r10 <<  " 0x" << std::hex << mcontext->arm_r10 << std::endl;
        g_pLogFile << "R11:  " << mcontext->arm_fp <<   " 0x" << std::hex << mcontext->arm_fp << std::endl;
        g_pLogFile << "R12:  " << mcontext->arm_ip <<   " 0x" << std::hex << mcontext->arm_ip << std::endl;
        g_pLogFile << "SP:   " << mcontext->arm_sp <<   " 0x" << std::hex << mcontext->arm_sp << std::endl;
        g_pLogFile << "LR:   " << mcontext->arm_lr <<   " 0x" << std::hex << mcontext->arm_lr << std::endl;
        g_pLogFile << "PC:   " << mcontext->arm_pc <<   " 0x" << std::hex << mcontext->arm_pc << std::endl;
        g_pLogFile << "CPSR: " << mcontext->arm_cpsr << " 0x" << std::hex << mcontext->arm_cpsr << std::endl;
    #else
        g_pLogFile << "X0:   " << mcontext->regs[0] <<  " 0x" << std::hex << mcontext->regs[0] << std::endl;
        g_pLogFile << "X1:   " << mcontext->regs[1] <<  " 0x" << std::hex << mcontext->regs[1] << std::endl;
        g_pLogFile << "X2:   " << mcontext->regs[2] <<  " 0x" << std::hex << mcontext->regs[2] << std::endl;
        g_pLogFile << "X3:   " << mcontext->regs[3] <<  " 0x" << std::hex << mcontext->regs[3] << std::endl;
        g_pLogFile << "X4:   " << mcontext->regs[4] <<  " 0x" << std::hex << mcontext->regs[4] << std::endl;
        g_pLogFile << "X5:   " << mcontext->regs[5] <<  " 0x" << std::hex << mcontext->regs[5] << std::endl;
        g_pLogFile << "X6:   " << mcontext->regs[6] <<  " 0x" << std::hex << mcontext->regs[6] << std::endl;
        g_pLogFile << "X7:   " << mcontext->regs[7] <<  " 0x" << std::hex << mcontext->regs[7] << std::endl;
        g_pLogFile << "X8:   " << mcontext->regs[8] <<  " 0x" << std::hex << mcontext->regs[8] << std::endl;
        g_pLogFile << "X9:   " << mcontext->regs[9] <<  " 0x" << std::hex << mcontext->regs[9] << std::endl;
        g_pLogFile << "X10:  " << mcontext->regs[10] << " 0x" << std::hex << mcontext->regs[10] << std::endl;
        g_pLogFile << "X11:  " << mcontext->regs[11] << " 0x" << std::hex << mcontext->regs[11] << std::endl;
        g_pLogFile << "X12:  " << mcontext->regs[12] << " 0x" << std::hex << mcontext->regs[12] << std::endl;
        g_pLogFile << "X13:  " << mcontext->regs[13] << " 0x" << std::hex << mcontext->regs[13] << std::endl;
        g_pLogFile << "X14:  " << mcontext->regs[14] << " 0x" << std::hex << mcontext->regs[14] << std::endl;
        g_pLogFile << "X15:  " << mcontext->regs[15] << " 0x" << std::hex << mcontext->regs[15] << std::endl;
        g_pLogFile << "X16:  " << mcontext->regs[16] << " 0x" << std::hex << mcontext->regs[16] << std::endl;
        g_pLogFile << "X17:  " << mcontext->regs[17] << " 0x" << std::hex << mcontext->regs[17] << std::endl;
        g_pLogFile << "X18:  " << mcontext->regs[18] << " 0x" << std::hex << mcontext->regs[18] << std::endl;
        g_pLogFile << "X19:  " << mcontext->regs[19] << " 0x" << std::hex << mcontext->regs[19] << std::endl;
        g_pLogFile << "X20:  " << mcontext->regs[20] << " 0x" << std::hex << mcontext->regs[20] << std::endl;
        g_pLogFile << "X21:  " << mcontext->regs[21] << " 0x" << std::hex << mcontext->regs[21] << std::endl;
        g_pLogFile << "X22:  " << mcontext->regs[22] << " 0x" << std::hex << mcontext->regs[22] << std::endl;
        g_pLogFile << "X23:  " << mcontext->regs[23] << " 0x" << std::hex << mcontext->regs[23] << std::endl;
        g_pLogFile << "X24:  " << mcontext->regs[24] << " 0x" << std::hex << mcontext->regs[24] << std::endl;
        g_pLogFile << "X25:  " << mcontext->regs[25] << " 0x" << std::hex << mcontext->regs[25] << std::endl;
        g_pLogFile << "X26:  " << mcontext->regs[26] << " 0x" << std::hex << mcontext->regs[26] << std::endl;
        g_pLogFile << "X27:  " << mcontext->regs[27] << " 0x" << std::hex << mcontext->regs[27] << std::endl;
        g_pLogFile << "X28:  " << mcontext->regs[28] << " 0x" << std::hex << mcontext->regs[28] << std::endl;
        g_pLogFile << "X29:  " << mcontext->regs[29] << " 0x" << std::hex << mcontext->regs[29] << std::endl;
        g_pLogFile << "X30:  " << mcontext->regs[30] << " 0x" << std::hex << mcontext->regs[30] << std::endl;
        g_pLogFile << "SP:   " << mcontext->sp <<       " 0x" << std::hex << mcontext->sp << std::endl;
        g_pLogFile << "PC:   " << mcontext->pc <<       " 0x" << std::hex << mcontext->pc << std::endl;
        g_pLogFile << "CPSR: " << mcontext->pstate <<   " 0x" << std::hex << mcontext->pstate << std::endl;
    #endif

  skip_logging:
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

    __builtin_trap();
}