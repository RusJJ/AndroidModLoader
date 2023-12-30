#include <mod/logger.h>
#include <modslist.h>
#include <fstream>
#include <unistd.h>
#include <dlfcn.h>
#include <aml.h>
#include <jnifn.h>
#include "xunwind.h"

#define STACKDUMP_SIZE 0x510
std::ofstream g_pLogFile;

struct sigaction newSigaction[7];
struct sigaction oldSigaction[7];
extern bool g_bSimplerCrashLog, g_bNoSPInLog, g_bNoModsInLog, g_bDumpAllThreads, g_bEHUnwind, g_bMoreRegsInfo;

static stack_t stackstruct;
static char signalstack[SIGSTKSZ];
static uintptr_t g_frames[128];

extern jobject appContext;
extern JNIEnv* env;

ModDesc* pLastModProcessed = NULL;

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

bool bHasHandledError = false;
void Handler(int sig, siginfo_t *si, void *ptr)
{
    if(bHasHandledError)
    {
        exit(0);
        return;
    }
    bHasHandledError = true;

    char *stack;
    ucontext_t* ucontext = (ucontext_t*)ptr;
    mcontext_t* mcontext = &ucontext->uc_mcontext;
    
    #ifdef AML32
        uintptr_t PC = mcontext->arm_pc;
    #else
        uintptr_t PC = mcontext->pc;
    #endif
    uintptr_t faultAddr = mcontext->fault_address; // == si->si_addr?

    char path[320], pathText[512];
    sprintf(path, "%s/aml_crashlog.txt", aml->GetAndroidDataRootPath());
    sprintf(pathText, "Application has been crashed!\n\nCrashlog should be saved in %s", path);
    logger->Error("Exception Signal %d - %s (%s)", sig, SignalEnum(sig), CodeEnum(sig, si->si_code));
    logger->Error(pathText);
    g_pLogFile.open(path, std::ios::out | std::ios::trunc);


    char* stackLog;
    if(!g_pLogFile.is_open()) goto skip_logging;


    g_pLogFile << "!!! THIS IS A CRASH LOG !!!\nIf you are experiencing a crash, give us this file.\n>>> DO NOT SEND US A SCREENSHOT OF THIS FILE <<<" << std::endl << std::endl;

    #define DEVVAR_LOG(__v) g_pLogFile << #__v << (__v ? " = 1 | " : " = 0 | ")
    g_pLogFile << "Config variables: | "; DEVVAR_LOG(g_bSimplerCrashLog); DEVVAR_LOG(g_bNoSPInLog);
    DEVVAR_LOG(g_bNoModsInLog); DEVVAR_LOG(g_bDumpAllThreads); DEVVAR_LOG(g_bEHUnwind); DEVVAR_LOG(g_bMoreRegsInfo); g_pLogFile << std::endl;

    g_pLogFile << "Exception Signal " << sig << " - " << SignalEnum(sig) << " (" << CodeEnum(sig, si->si_code) << ")" << std::endl;
    g_pLogFile << "Fault address: 0x" << std::hex << std::uppercase << faultAddr << std::nouppercase << std::endl;
    g_pLogFile << "An overall reason of the crash:\n- ";
    switch(sig)
    {
    case SIGABRT:
        g_pLogFile << "Because an application is killed by something (Android`s Low Memory Killer?)" << std::endl;
        break;
    case SIGBUS:
        g_pLogFile << "Not enough memory, or invalid execution address, or bad mod patch" << std::endl;
        break;
    case SIGFPE:
        g_pLogFile << "An error somewhere in the code, often - dividing by zero" << std::endl;
        break;
    case SIGSEGV:
        g_pLogFile << "An application tried to access the memory address that is unaccessible, protected or just wrong" << std::endl;
        break;
    case SIGILL:
        g_pLogFile << "Corrupted application stack, wrong call address or no privileges to do something" << std::endl;
        break;
    case SIGSTKFLT:
        g_pLogFile << "Stack fault on coprocessor" << std::endl;
        break;
    case SIGTRAP:
        g_pLogFile << "It`s a trap! Somewhere in the application is called \"it`s a trap! stop the application!\"" << std::endl;
        break;
    }

    Dl_info dlInfo;
    if(dladdr((void*)PC, &dlInfo) != 0)
    {
        // Success
        if(dlInfo.dli_fname)
        {
            g_pLogFile << "Library base: 0x" << std::hex << std::uppercase << (uintptr_t)dlInfo.dli_fbase << std::endl;
            g_pLogFile << GetFilenamePart(dlInfo.dli_fname) << " + 0x" << std::hex << std::uppercase << (PC - (uintptr_t)dlInfo.dli_fbase);
        }
        else
        {
            if(!dlInfo.dli_fbase) goto label_unsuccess;
            g_pLogFile << "Library base: 0x" << std::hex << std::uppercase << (uintptr_t)dlInfo.dli_fbase << std::endl;
            g_pLogFile << "Program counter: Unknown Lib + 0x" << std::hex << std::uppercase << (PC - (uintptr_t)dlInfo.dli_fbase);
        }
    }
    else
    {
        // Unsuccess
      label_unsuccess:
        g_pLogFile << "Program counter: Unknown Lib, 0x" << std::hex << std::uppercase << PC;
    }

    if(dlInfo.dli_sname)
    {
        g_pLogFile << std::nouppercase << " (" << dlInfo.dli_sname << ")" << std::endl;
    }
    else
    {
        g_pLogFile << std::endl;
    }
    g_pLogFile.flush();

    g_pLogFile << "\n----------------------------------------------------\nRegisters:" << std::endl;
    #define SHOWREG(__t, __v)   g_pLogFile << #__t ":\t" << std::dec << __v << "\t0x" << std::hex << std::uppercase << __v; \
                                if(dladdr((void*)(__v), &dlRegInfo) != 0 && dlRegInfo.dli_fname) { \
                                    g_pLogFile << " (" << GetFilenamePart(dlRegInfo.dli_fname) << " + 0x" << std::hex << std::uppercase << ((uintptr_t)(__v) - (uintptr_t)dlRegInfo.dli_fbase) << ")"; \
                                } g_pLogFile << std::endl

    #ifdef AML32
        if(g_bMoreRegsInfo)
        {
            Dl_info dlRegInfo;
            SHOWREG(R0, mcontext->arm_r0);
            SHOWREG(R1, mcontext->arm_r1);
            SHOWREG(R2, mcontext->arm_r2);
            SHOWREG(R3, mcontext->arm_r3);
            SHOWREG(R4, mcontext->arm_r4);
            SHOWREG(R5, mcontext->arm_r5);
            SHOWREG(R6, mcontext->arm_r6);
            SHOWREG(R7, mcontext->arm_r7);
            SHOWREG(R8, mcontext->arm_r8);
            SHOWREG(R9, mcontext->arm_r9);
            SHOWREG(R10, mcontext->arm_r10);
            SHOWREG(R11, mcontext->arm_fp);
            SHOWREG(R12, mcontext->arm_ip);
            SHOWREG(SP, mcontext->arm_sp);
            SHOWREG(LR, mcontext->arm_lr);
            SHOWREG(PC, mcontext->arm_pc);
            SHOWREG(CPSR, mcontext->arm_cpsr);
        }
        else
        {
            g_pLogFile << "R0:   " << std::dec << mcontext->arm_r0 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r0   << std::endl;
            g_pLogFile << "R1:   " << std::dec << mcontext->arm_r1 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r1   << std::endl;
            g_pLogFile << "R2:   " << std::dec << mcontext->arm_r2 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r2   << std::endl;
            g_pLogFile << "R3:   " << std::dec << mcontext->arm_r3 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r3   << std::endl;
            g_pLogFile << "R4:   " << std::dec << mcontext->arm_r4 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r4   << std::endl;
            g_pLogFile << "R5:   " << std::dec << mcontext->arm_r5 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r5   << std::endl;
            g_pLogFile << "R6:   " << std::dec << mcontext->arm_r6 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r6   << std::endl;
            g_pLogFile << "R7:   " << std::dec << mcontext->arm_r7 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r7   << std::endl;
            g_pLogFile << "R8:   " << std::dec << mcontext->arm_r8 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r8   << std::endl;
            g_pLogFile << "R9:   " << std::dec << mcontext->arm_r9 <<   " 0x" << std::hex << std::uppercase << mcontext->arm_r9   << std::endl;
            g_pLogFile << "R10:  " << std::dec << mcontext->arm_r10 <<  " 0x" << std::hex << std::uppercase << mcontext->arm_r10  << std::endl;
            g_pLogFile << "R11:  " << std::dec << mcontext->arm_fp <<   " 0x" << std::hex << std::uppercase << mcontext->arm_fp   << std::endl;
            g_pLogFile << "R12:  " << std::dec << mcontext->arm_ip <<   " 0x" << std::hex << std::uppercase << mcontext->arm_ip   << std::endl;
            g_pLogFile << "SP:   " << std::dec << mcontext->arm_sp <<   " 0x" << std::hex << std::uppercase << mcontext->arm_sp   << std::endl;
            g_pLogFile << "LR:   " << std::dec << mcontext->arm_lr <<   " 0x" << std::hex << std::uppercase << mcontext->arm_lr   << std::endl;
            g_pLogFile << "PC:   " << std::dec << mcontext->arm_pc <<   " 0x" << std::hex << std::uppercase << mcontext->arm_pc   << std::endl;
            g_pLogFile << "CPSR: " << std::dec << mcontext->arm_cpsr << " 0x" << std::hex << std::uppercase << mcontext->arm_cpsr << std::endl;
        }
    #else
        if(g_bMoreRegsInfo)
        {
            Dl_info dlRegInfo;
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
            g_pLogFile << "X0:   " << std::dec << mcontext->regs[0] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[0]  << std::endl;
            g_pLogFile << "X1:   " << std::dec << mcontext->regs[1] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[1]  << std::endl;
            g_pLogFile << "X2:   " << std::dec << mcontext->regs[2] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[2]  << std::endl;
            g_pLogFile << "X3:   " << std::dec << mcontext->regs[3] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[3]  << std::endl;
            g_pLogFile << "X4:   " << std::dec << mcontext->regs[4] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[4]  << std::endl;
            g_pLogFile << "X5:   " << std::dec << mcontext->regs[5] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[5]  << std::endl;
            g_pLogFile << "X6:   " << std::dec << mcontext->regs[6] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[6]  << std::endl;
            g_pLogFile << "X7:   " << std::dec << mcontext->regs[7] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[7]  << std::endl;
            g_pLogFile << "X8:   " << std::dec << mcontext->regs[8] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[8]  << std::endl;
            g_pLogFile << "X9:   " << std::dec << mcontext->regs[9] <<  " 0x" << std::hex << std::uppercase << mcontext->regs[9]  << std::endl;
            g_pLogFile << "X10:  " << std::dec << mcontext->regs[10] << " 0x" << std::hex << std::uppercase << mcontext->regs[10] << std::endl;
            g_pLogFile << "X11:  " << std::dec << mcontext->regs[11] << " 0x" << std::hex << std::uppercase << mcontext->regs[11] << std::endl;
            g_pLogFile << "X12:  " << std::dec << mcontext->regs[12] << " 0x" << std::hex << std::uppercase << mcontext->regs[12] << std::endl;
            g_pLogFile << "X13:  " << std::dec << mcontext->regs[13] << " 0x" << std::hex << std::uppercase << mcontext->regs[13] << std::endl;
            g_pLogFile << "X14:  " << std::dec << mcontext->regs[14] << " 0x" << std::hex << std::uppercase << mcontext->regs[14] << std::endl;
            g_pLogFile << "X15:  " << std::dec << mcontext->regs[15] << " 0x" << std::hex << std::uppercase << mcontext->regs[15] << std::endl;
            g_pLogFile << "X16:  " << std::dec << mcontext->regs[16] << " 0x" << std::hex << std::uppercase << mcontext->regs[16] << std::endl;
            g_pLogFile << "X17:  " << std::dec << mcontext->regs[17] << " 0x" << std::hex << std::uppercase << mcontext->regs[17] << std::endl;
            g_pLogFile << "X18:  " << std::dec << mcontext->regs[18] << " 0x" << std::hex << std::uppercase << mcontext->regs[18] << std::endl;
            g_pLogFile << "X19:  " << std::dec << mcontext->regs[19] << " 0x" << std::hex << std::uppercase << mcontext->regs[19] << std::endl;
            g_pLogFile << "X20:  " << std::dec << mcontext->regs[20] << " 0x" << std::hex << std::uppercase << mcontext->regs[20] << std::endl;
            g_pLogFile << "X21:  " << std::dec << mcontext->regs[21] << " 0x" << std::hex << std::uppercase << mcontext->regs[21] << std::endl;
            g_pLogFile << "X22:  " << std::dec << mcontext->regs[22] << " 0x" << std::hex << std::uppercase << mcontext->regs[22] << std::endl;
            g_pLogFile << "X23:  " << std::dec << mcontext->regs[23] << " 0x" << std::hex << std::uppercase << mcontext->regs[23] << std::endl;
            g_pLogFile << "X24:  " << std::dec << mcontext->regs[24] << " 0x" << std::hex << std::uppercase << mcontext->regs[24] << std::endl;
            g_pLogFile << "X25:  " << std::dec << mcontext->regs[25] << " 0x" << std::hex << std::uppercase << mcontext->regs[25] << std::endl;
            g_pLogFile << "X26:  " << std::dec << mcontext->regs[26] << " 0x" << std::hex << std::uppercase << mcontext->regs[26] << std::endl;
            g_pLogFile << "X27:  " << std::dec << mcontext->regs[27] << " 0x" << std::hex << std::uppercase << mcontext->regs[27] << std::endl;
            g_pLogFile << "X28:  " << std::dec << mcontext->regs[28] << " 0x" << std::hex << std::uppercase << mcontext->regs[28] << std::endl;
            g_pLogFile << "X29:  " << std::dec << mcontext->regs[29] << " 0x" << std::hex << std::uppercase << mcontext->regs[29] << std::endl;
            g_pLogFile << "X30:  " << std::dec << mcontext->regs[30] << " 0x" << std::hex << std::uppercase << mcontext->regs[30] << std::endl;
            g_pLogFile << "SP:   " << std::dec << mcontext->sp <<       " 0x" << std::hex << std::uppercase << mcontext->sp       << std::endl;
            g_pLogFile << "PC:   " << std::dec << mcontext->pc <<       " 0x" << std::hex << std::uppercase << mcontext->pc       << std::endl;
            g_pLogFile << "CPSR: " << std::dec << mcontext->pstate <<   " 0x" << std::hex << std::uppercase << mcontext->pstate   << std::endl;
        }
    #endif
    g_pLogFile.flush();

    #ifdef AML32
        stack = (char*)mcontext->arm_sp;
    #else
        stack = (char*)mcontext->sp;
    #endif

    if(!g_bNoSPInLog)
    {
        g_pLogFile << "\n----------------------------------------------------\nPrinting " << std::dec << STACKDUMP_SIZE << " bytes of stack:" << std::endl;
        g_pLogFile << std::hex << std::uppercase;
        for(int i = 1; i <= STACKDUMP_SIZE; ++i)
        {
            g_pLogFile << " " << std::setfill('0') << std::setw(2) << (int)(stack[i - 1]);
            if(i % 16 == 0)
            {
                g_pLogFile << " (SP+0x" << std::setfill('0') << std::setw(3) << 16 * ((i / 16) - 1) << ") [";
                int endv = i;
                for(int j = i-16; j < endv && j < STACKDUMP_SIZE; ++j)
                {
                    char spc = stack[j];
                    if(std::isalnum(spc) || std::ispunct(spc)) g_pLogFile << spc;
                    else g_pLogFile << '.';
                }
                g_pLogFile << "]" << std::endl;
                g_pLogFile.flush();
            }
        }
    }

    if(!g_bNoModsInLog)
    {
        modlist->PrintModsList(g_pLogFile);
        g_pLogFile.flush();
    }

    if(pLastModProcessed)
    {
        g_pLogFile << "\n----------------------------------------------------\nLatest mod processed:\n";
        g_pLogFile << pLastModProcessed->m_pInfo->Name() << " (" << pLastModProcessed->m_pInfo->Author() << ", version " << pLastModProcessed->m_pInfo->VersionString() << ")\n";
        g_pLogFile << " - GUID: " << pLastModProcessed->m_pInfo->GUID() << " | Base: 0x" << std::hex << std::uppercase << (uintptr_t)pLastModProcessed->m_pHandle << " | Path: " << pLastModProcessed->m_szLibPath << "\n";
        g_pLogFile.flush();
    }
        
    #ifdef IO_GITHUB_HEXHACKING_XUNWIND
        if(!g_bSimplerCrashLog)
        {
            if(g_bEHUnwind)
            {
                stackLog = NULL;

                size_t frames_sz = xunwind_eh_unwind(g_frames, sizeof(g_frames) / sizeof(g_frames[0]), ucontext);
                if(frames_sz > 0)
                {
                    stackLog = xunwind_frames_get(g_frames, frames_sz, "");
                }
            }
            else
            {
                stackLog = xunwind_cfi_get(XUNWIND_CURRENT_PROCESS, g_bDumpAllThreads ? XUNWIND_ALL_THREADS : XUNWIND_CURRENT_THREAD, ucontext, "");
            }
            if(stackLog && stackLog[0])
            {
                g_pLogFile << "\n----------------------------------------------------\n" << (g_bDumpAllThreads ? "Call stack (of all threads):\n" : "Call stack:\n") << stackLog;
                free(stackLog);
            }
            else
            {
                g_pLogFile << "\n----------------------------------------------------\nCall stack:\nA system returned no crash log!";
            }
            g_pLogFile.flush();
        }
    #endif

    g_pLogFile << "\n----------------------------------------------------\n\t\tEND OF REPORT\n----------------------------------------------------\n\n";
    g_pLogFile << "If you`re having problems using official mods, please report about this problem on our OFFICIAL server:\n\t\thttps://discord.gg/2MY7W39kBg\nPlease follow the rules and head to the #help section!";
    g_pLogFile.flush();
    
  skip_logging:
    logger->Info("Notifying mods about the crash...");
    modlist->ProcessCrash(dlInfo.dli_fname ? GetFilenamePart(dlInfo.dli_fname) : "", sig, si->si_code, (uintptr_t)dlInfo.dli_fbase, mcontext);
    modlist->ProcessUnloading();

    oldSigaction[SignalInnerId(sig)].sa_sigaction(sig, si, ptr);
    exit(0);
}

#define HANDLESIG(_code) sigbreak = newSigaction + SignalInnerId(_code); sigbreak->sa_sigaction = &Handler; \
                         sigbreak->sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND; sigaction(_code, sigbreak, oldSigaction + SignalInnerId(_code))
void StartSignalHandler()
{
    stackstruct.ss_sp = &signalstack[0];
    stackstruct.ss_size = SIGSTKSZ;
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
