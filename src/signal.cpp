#include <mod/logger.h>
#include <modslist.h>

#include <fstream>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <dlfcn.h>
#include <aml.h>
#include <jnifn.h>
#include <cxxabi.h> // char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status); free(demangled);
#include "jcrasher.h"

#define STACKDUMP_SIZE 0x510
static int g_nLogFileFd = -1;

struct sigaction newSigaction[7];
struct sigaction oldSigaction[7];
extern bool g_bSimplerCrashLog, g_bNoSPInLog, g_bNoModsInLog, g_bDumpAllThreads,
            g_bMoreRegsInfo, g_bDumpThreadRegisters;
extern int g_nAndroidSDKVersion;

static stack_t stackstruct;
static char signalstack[256 * 1024]; // 256kB
static char signalbuf[1024];
#define fd_printf(format, ...) \
    do{ int _macro_len = snprintf(signalbuf, sizeof(signalbuf), format, ##__VA_ARGS__); \
        if(_macro_len > 0) write(g_nLogFileFd, signalbuf, _macro_len); } while(0)
#define fd_print(text) write(g_nLogFileFd, text, sizeof(text))

ModDesc* pLastModProcessed = NULL;

int SignalInnerId(int code)
{
    switch(code)
    {
        case SIGABRT:   return 0;
        case SIGBUS:    return 1;
        case SIGFPE:    return 2;
        case SIGSEGV:   return 3;
        case SIGILL:    return 4;
        case SIGSTKFLT: return 5;
        case SIGTRAP:   return 6;
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
    return "SIGUNKNOWN";
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
                default: return "ILL_UNKNOWN";
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
                default: return "FPE_UNKNOWN";
            }
        }
        case SIGSEGV:
        {
            switch(code)
            {
                ENUMERATE_THIS(SEGV_MAPERR);
                ENUMERATE_THIS(SEGV_ACCERR);
                default: return "SEGV_UNKNOWN";
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
                default: return "SIG_UNKNOWN";
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
                default: return "TRAP_UNKNOWN";
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
                default: return "CLD_UNKNOWN";
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
                default: return "POLL_UNKNOWN";
            }
        }
#endif
    }
    return "UNKNOWN";
}

inline const char* GetFilenamePart(const char* path)
{
    int idx = 0;
    for(int i = 0; path[i] != 0; ++i)
    {
        if((path[i] == '/' || path[i] == '\\') &&
           (path[i + 1] != '/' && path[i + 1] != '\\'))
        {
            idx = i + 1;
        }
    }
    return &path[idx];
}

static volatile sig_atomic_t g_handling_signal = 0;
static void ContinueToOldHandler(int sig, siginfo_t* si, void* ctx)
{
    int id = SignalInnerId(sig);
    if(id < 0) _exit(128 + sig);

    struct sigaction old = oldSigaction[id];
    sigaction(sig, &old, NULL);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    if(old.sa_flags & SA_SIGINFO)
    {
        if(old.sa_sigaction) old.sa_sigaction(sig, si, ctx);
    }
    else
    {
        if(old.sa_handler == SIG_DFL)
        {
            syscall(SYS_tgkill, getpid(), gettid(), sig);
        }
        else if(old.sa_handler == SIG_IGN)
        {
            signal(sig, SIG_DFL);
            syscall(SYS_tgkill, getpid(), gettid(), sig);
        }
        else if(old.sa_handler)
        {
            old.sa_handler(sig);
        }
    }

    // If previous handler returned, force fatal exit (this should not happen.)
    signal(sig, SIG_DFL);
    syscall(SYS_tgkill, getpid(), gettid(), sig);
    _exit(128 + sig);
}

void Handler(int sig, siginfo_t *si, void *ptr)
{
    if(g_handling_signal) ContinueToOldHandler(sig, si, ptr);
    g_handling_signal = 1;

    char *stack;
    ucontext_t* ucontext = (ucontext_t*)ptr;
    mcontext_t* mcontext = &ucontext->uc_mcontext;
    
    #ifdef AML32
        uintptr_t PC = mcontext->arm_pc;
    #else
        uintptr_t PC = mcontext->pc;
    #endif
    uintptr_t faultAddr = (uintptr_t)si->si_addr;

    char path[512], pathText[512];
    snprintf(path, sizeof(path), "%s/aml_crashlog.txt", aml->GetAndroidDataRootPath());
    snprintf(pathText, sizeof(pathText), "Application has been crashed!\n\nCrashlog should be saved in %s", path);
    logger->Error("Exception Signal %d - %s (%s)", sig, SignalEnum(sig), CodeEnum(sig, si->si_code));
    logger->Error("%s", pathText);

    g_nLogFileFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(g_nLogFileFd < 0)
    {
        logger->Error("Failed to open aml_crashlog.txt !");
        goto skip_logging;
    }

    fd_print("!!! THIS IS A CRASH LOG !!!\nIf you are experiencing a crash, give us this file.\n>>> DO NOT SEND US A SCREENSHOT OF THIS FILE <<<\n\n");

    #define DEVVAR_LOG(__v) fd_printf(#__v " = %d", (int)(__v == true))
    fd_print("Config variables: | "); DEVVAR_LOG(g_bSimplerCrashLog); DEVVAR_LOG(g_bNoSPInLog);
    DEVVAR_LOG(g_bNoModsInLog);       DEVVAR_LOG(g_bDumpAllThreads);
    DEVVAR_LOG(g_bMoreRegsInfo);      DEVVAR_LOG(g_bDumpThreadRegisters);
    fd_print("\n");

    fd_printf("Exception Signal %d - %s (%s)\n", sig, SignalEnum(sig), CodeEnum(sig, si->si_code));
    fd_printf("Fault address: " PTRFMT " (" PTRFMT ") at " PTRFMT "\n", faultAddr, (uintptr_t)si->si_addr, PC);
    fd_print("A POSSIBLE (!) reasons of the crash:\n- ");
    switch(sig)
    {
    case SIGABRT:
        fd_print("Memory assertion, stack smashing, heap corruption, exception\n");
        break;
    case SIGBUS:
        fd_print("Corrupted memory pointer, wrong file pointer, hardware memory error\n");
        break;
    case SIGFPE:
        fd_print("Division or module by zero, integer overflow\n");
        break;
    case SIGSEGV:
        fd_print("Nullw/wrong pointer, stack overflow\n");
        break;
    case SIGILL:
        fd_print("Wrong patch/return address, code corruption\n");
        break;
    case SIGSTKFLT:
        fd_print("Stack fault on coprocessor\n"); // We.. dont have it?
        break;
    case SIGTRAP:
        fd_print("Broken mod/patch (undefined behavior), debugger activated, mod/patch reached wrong address\n");
        break;
    }

    static Dl_info dlInfo;
    if(PC && dladdr((void*)PC, &dlInfo) != 0)
    {
        // Success
        if(dlInfo.dli_fname)
        {
            fd_printf("Library base: " PTRFMT "\n", (uintptr_t)dlInfo.dli_fbase);
            fd_printf("%s + " PTRFMT, GetFilenamePart(dlInfo.dli_fname), (PC - (uintptr_t)dlInfo.dli_fbase));
        }
        else
        {
            if(!dlInfo.dli_fbase) goto label_unsuccess;
            fd_printf("Library base: " PTRFMT "\n", (uintptr_t)dlInfo.dli_fbase);
            fd_printf("Program counter: Unknown Lib + " PTRFMT, (PC - (uintptr_t)dlInfo.dli_fbase));
        }
    }
    else
    {
        // Unsuccess
      label_unsuccess:
        fd_printf("Failed to get a library. Program counter: " PTRFMT, PC);
    }
    if(dlInfo.dli_sname) fd_printf(" (%s)", dlInfo.dli_sname);

    char sysprop_str[92];
    fd_print("\n\n----------------------------------------------------\nShort device info:\n");
    fd_printf("Android SDK Version: %d\n", g_nAndroidSDKVersion);
    if(__system_property_get("ro.product.brand", sysprop_str) || __system_property_get("ro.product.system.brand", sysprop_str))
    {
        fd_printf("Brand: %s\n", sysprop_str);
    }
    if(__system_property_get("ro.product.device", sysprop_str) || __system_property_get("ro.product.system.device", sysprop_str))
    {
        fd_printf("Device: %s\n", sysprop_str);
    }
    if(__system_property_get("ro.system.product.cpu.abilist", sysprop_str))
    {
        fd_printf("Supported ABIs: %s", sysprop_str);
        if(strstr(sysprop_str, "86") != NULL) fd_print(" (it looks like you`re on emulator?)");
        fd_print("\n");
    }
    if(__system_property_get("ro.build.date", sysprop_str))
    {
        fd_printf("OS Build Date: %s\n", sysprop_str);
    }
    if(__system_property_get("ro.build.id", sysprop_str) || __system_property_get("ro.system.build.id", sysprop_str))
    {
        fd_printf("OS Build ID: %s\n", sysprop_str);
    }

    fd_print("\n----------------------------------------------------\nRegisters:\n");

    // dlInfo.dli_sname in register might point to the variable with corrupted name
    #define SHOWREG(__t, __v)   fd_printf(#__t ":\t" PTRNUMFMT "\t" PTRFMT, (uintptr_t)(__v), (uintptr_t)(__v)); \
                                if((void*)(__v) && dladdr((void*)(__v), &dlRegInfo) != 0 && dlRegInfo.dli_fname) { \
                                    fd_printf(" (%s + " PTRFMT ")", GetFilenamePart(dlRegInfo.dli_fname), ((uintptr_t)(__v) - (uintptr_t)dlRegInfo.dli_fbase) ); \
                                    fd_print("\n"); \
                                }

    #ifdef AML32
        if(g_bMoreRegsInfo)
        {
            static Dl_info dlRegInfo;
            SHOWREG(R0,   mcontext->arm_r0);
            SHOWREG(R1,   mcontext->arm_r1);
            SHOWREG(R2,   mcontext->arm_r2);
            SHOWREG(R3,   mcontext->arm_r3);
            SHOWREG(R4,   mcontext->arm_r4);
            SHOWREG(R5,   mcontext->arm_r5);
            SHOWREG(R6,   mcontext->arm_r6);
            SHOWREG(R7,   mcontext->arm_r7);
            SHOWREG(R8,   mcontext->arm_r8);
            SHOWREG(R9,   mcontext->arm_r9);
            SHOWREG(R10,  mcontext->arm_r10);
            SHOWREG(R11,  mcontext->arm_fp);
            SHOWREG(R12,  mcontext->arm_ip);
            SHOWREG(SP,   mcontext->arm_sp);
            SHOWREG(LR,   mcontext->arm_lr);
            SHOWREG(PC,   mcontext->arm_pc);
            SHOWREG(CPSR, mcontext->arm_cpsr);
        }
        else
        {
            fd_printf("R0:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r0  , (uintptr_t)mcontext->arm_r0  );
            fd_printf("R1:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r1  , (uintptr_t)mcontext->arm_r1  );
            fd_printf("R2:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r2  , (uintptr_t)mcontext->arm_r2  );
            fd_printf("R3:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r3  , (uintptr_t)mcontext->arm_r3  );
            fd_printf("R4:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r4  , (uintptr_t)mcontext->arm_r4  );
            fd_printf("R5:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r5  , (uintptr_t)mcontext->arm_r5  );
            fd_printf("R6:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r6  , (uintptr_t)mcontext->arm_r6  );
            fd_printf("R7:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r7  , (uintptr_t)mcontext->arm_r7  );
            fd_printf("R8:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r8  , (uintptr_t)mcontext->arm_r8  );
            fd_printf("R9:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r9  , (uintptr_t)mcontext->arm_r9  );
            fd_printf("R10:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_r10 , (uintptr_t)mcontext->arm_r10 );
            fd_printf("R11:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_fp  , (uintptr_t)mcontext->arm_fp  );
            fd_printf("R12:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_ip  , (uintptr_t)mcontext->arm_ip  );
            fd_printf("SP:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_sp  , (uintptr_t)mcontext->arm_sp  );
            fd_printf("LR:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_lr  , (uintptr_t)mcontext->arm_lr  );
            fd_printf("PC:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_pc  , (uintptr_t)mcontext->arm_pc  );
            fd_printf("CPSR: " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->arm_cpsr, (uintptr_t)mcontext->arm_cpsr);
        }
    #else
        if(g_bMoreRegsInfo)
        {
            static Dl_info dlRegInfo;
            SHOWREG(X0, mcontext->regs[0]);
            SHOWREG(X1, mcontext->regs[1]);
            SHOWREG(X2, mcontext->regs[2]);
            SHOWREG(X3, mcontext->regs[3]);
            SHOWREG(X4, mcontext->regs[4]);
            SHOWREG(X5, mcontext->regs[5]);
            SHOWREG(X6, mcontext->regs[6]);
            SHOWREG(X7, mcontext->regs[7]);
            SHOWREG(X8, mcontext->regs[8]);
            SHOWREG(X9, mcontext->regs[9]);
            SHOWREG(X10, mcontext->regs[10]);
            SHOWREG(X11, mcontext->regs[11]);
            SHOWREG(X12, mcontext->regs[12]);
            SHOWREG(X13, mcontext->regs[13]);
            SHOWREG(X14, mcontext->regs[14]);
            SHOWREG(X15, mcontext->regs[15]);
            SHOWREG(X16, mcontext->regs[16]);
            SHOWREG(X17, mcontext->regs[17]);
            SHOWREG(X18, mcontext->regs[18]);
            SHOWREG(X19, mcontext->regs[19]);
            SHOWREG(X20, mcontext->regs[20]);
            SHOWREG(X21, mcontext->regs[21]);
            SHOWREG(X22, mcontext->regs[22]);
            SHOWREG(X23, mcontext->regs[23]);
            SHOWREG(X24, mcontext->regs[24]);
            SHOWREG(X25, mcontext->regs[25]);
            SHOWREG(X26, mcontext->regs[26]);
            SHOWREG(X27, mcontext->regs[27]);
            SHOWREG(X28, mcontext->regs[28]);
            SHOWREG(X29, mcontext->regs[29]);
            SHOWREG(X30, mcontext->regs[30]);
            SHOWREG(SP, mcontext->sp);
            SHOWREG(PC, mcontext->pc);
            SHOWREG(CPSR, mcontext->pstate);
        }
        else
        {
            fd_printf("X0:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[0] , (uintptr_t)mcontext->regs[0] );
            fd_printf("X1:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[1] , (uintptr_t)mcontext->regs[1] );
            fd_printf("X2:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[2] , (uintptr_t)mcontext->regs[2] );
            fd_printf("X3:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[3] , (uintptr_t)mcontext->regs[3] );
            fd_printf("X4:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[4] , (uintptr_t)mcontext->regs[4] );
            fd_printf("X5:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[5] , (uintptr_t)mcontext->regs[5] );
            fd_printf("X6:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[6] , (uintptr_t)mcontext->regs[6] );
            fd_printf("X7:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[7] , (uintptr_t)mcontext->regs[7] );
            fd_printf("X8:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[8] , (uintptr_t)mcontext->regs[8] );
            fd_printf("X9:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[9] , (uintptr_t)mcontext->regs[9] );
            fd_printf("X10:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[10], (uintptr_t)mcontext->regs[10]);
            fd_printf("X11:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[11], (uintptr_t)mcontext->regs[11]);
            fd_printf("X12:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[12], (uintptr_t)mcontext->regs[12]);
            fd_printf("X13:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[13], (uintptr_t)mcontext->regs[13]);
            fd_printf("X14:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[14], (uintptr_t)mcontext->regs[14]);
            fd_printf("X15:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[15], (uintptr_t)mcontext->regs[15]);
            fd_printf("X16:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[16], (uintptr_t)mcontext->regs[16]);
            fd_printf("X17:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[17], (uintptr_t)mcontext->regs[17]);
            fd_printf("X18:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[18], (uintptr_t)mcontext->regs[18]);
            fd_printf("X19:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[19], (uintptr_t)mcontext->regs[19]);
            fd_printf("X20:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[20], (uintptr_t)mcontext->regs[20]);
            fd_printf("X21:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[21], (uintptr_t)mcontext->regs[21]);
            fd_printf("X22:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[22], (uintptr_t)mcontext->regs[22]);
            fd_printf("X23:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[23], (uintptr_t)mcontext->regs[23]);
            fd_printf("X24:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[24], (uintptr_t)mcontext->regs[24]);
            fd_printf("X25:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[25], (uintptr_t)mcontext->regs[25]);
            fd_printf("X26:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[26], (uintptr_t)mcontext->regs[26]);
            fd_printf("X27:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[27], (uintptr_t)mcontext->regs[27]);
            fd_printf("X28:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[28], (uintptr_t)mcontext->regs[28]);
            fd_printf("X29:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[29], (uintptr_t)mcontext->regs[29]);
            fd_printf("X30:  " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->regs[30], (uintptr_t)mcontext->regs[30]);
            fd_printf("SP:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->sp      , (uintptr_t)mcontext->sp      );
            fd_printf("PC:   " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->pc      , (uintptr_t)mcontext->pc      );
            fd_printf("CPSR: " PTRNUMFMT " " PTRFMT "\n", (uintptr_t)mcontext->pstate  , (uintptr_t)mcontext->pstate  );
        }
    #endif

    if(!g_bNoModsInLog)
    {
        modlist->PrintModsList(g_nLogFileFd);
    }

    if(pLastModProcessed)
    {
        fd_print("\n----------------------------------------------------\nLatest mod processed:\n");
        fd_printf("%s (%s, version %s)\n", pLastModProcessed->m_pInfo->Name(), pLastModProcessed->m_pInfo->Author(), pLastModProcessed->m_pInfo->VersionString());
        fd_printf(" - GUID: %s | Base " PTRFMT " | Path: %s\n", pLastModProcessed->m_pInfo->GUID(), (uintptr_t)pLastModProcessed->m_pHandle, pLastModProcessed->m_szLibPath);
    }

    #ifdef AML32
        stack = (char*)mcontext->arm_sp;
    #else
        stack = (char*)mcontext->sp;
    #endif

    if(!g_bNoSPInLog && stack)
    {
        fd_printf("\n----------------------------------------------------\nPrinting %d bytes of stack:\n", STACKDUMP_SIZE);
        for(int i = 1; i <= STACKDUMP_SIZE; ++i)
        {
            fd_printf(" %02X", (int)(stack[i - 1]));
            if(i % 16 == 0)
            {
                fd_printf(" (SP+0x%03X) [", (int)( 16 * ((i / 16) - 1) ));
                int endv = i;
                for(int j = i-16; j < endv && j < STACKDUMP_SIZE; ++j)
                {
                    char spc = stack[j];
                    if(std::isalnum(spc) || std::ispunct(spc)) fd_printf("%c", spc);
                    fd_print(".");
                }
                fd_print("]\n");
            }
        }
    }
    else if(!stack)
    {
        fd_print("\n----------------------------------------------------\n- A program stack is missing..?!\n");
    }

    if(g_bSimplerCrashLog)
    {
        fd_print("\n----------------------------------------------------\nCall stack:\n- You disabled a detailed callstack dump!\n- This is not helpful. Do it if you know what you do.");
    }
    else
    {
        fd_print("\n----------------------------------------------------\n");
        if(g_bDumpAllThreads)
        {
            fd_print("Call stack (of all threads):\n");
            JCrasher::DumpAllThreadsFd(g_nLogFileFd, ucontext, JCrasher::DUMP_REGISTERS, 64);
        }
        else
        {
            fd_print("Call stack:\n");
            JCrasher::DumpCurrentThreadFd(g_nLogFileFd, ucontext, JCrasher::DUMP_REGISTERS, 64);
        }
    }

    fd_print("\n----------------------------------------------------\n\t\tEND OF REPORT\n----------------------------------------------------\n\n");
    fd_print("If you`re having problems using OFFICIAL mods, please report about this problem in our OFFICIAL server:\n\t\thttps://discord.gg/2MY7W39kBg\nPlease follow the rules and head to the #help section!");
    close(g_nLogFileFd);
    
  skip_logging:
    logger->Info("Notifying mods about the crash...");
    modlist->ProcessCrash(dlInfo.dli_fname ? GetFilenamePart(dlInfo.dli_fname) : "", sig, si->si_code, (uintptr_t)dlInfo.dli_fbase, mcontext);
    logger->Info("Telling mods to unload after the crash...");
    modlist->ProcessUnloading();

    ContinueToOldHandler(sig, si, ptr);
}

#define HANDLESIG(_code) sigbreak = newSigaction + SignalInnerId(_code); sigbreak->sa_sigaction = &Handler; \
                         sigbreak->sa_flags = SA_SIGINFO | SA_ONSTACK; sigemptyset(&sigbreak->sa_mask); \
                         sigaction(_code, sigbreak, oldSigaction + SignalInnerId(_code))
void StartSignalHandler()
{
    stackstruct.ss_sp = &signalstack[0];
    stackstruct.ss_size = sizeof(signalstack);
    stackstruct.ss_flags = 0;
    sigaltstack(&stackstruct, NULL);

    struct sigaction* sigbreak = NULL;

    HANDLESIG(SIGABRT);
    HANDLESIG(SIGBUS);
    HANDLESIG(SIGFPE);
    HANDLESIG(SIGSEGV);
    HANDLESIG(SIGILL);
    HANDLESIG(SIGSTKFLT);
    HANDLESIG(SIGTRAP);
}