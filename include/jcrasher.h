#ifndef __JCRASHER_H
#define __JCRASHER_H



// MIT License
// 
// Copyright (c) 2026 RusJJ
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.



#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ucontext.h>
#include <errno.h>


#ifndef AT_FDCWD
    #define AT_FDCWD -100
#endif
#ifndef O_DIRECTORY
    #define O_DIRECTORY 0200000
#endif
#ifndef O_CLOEXEC
    #define O_CLOEXEC 02000000
#endif
#ifndef PTRACE_SEIZE
    #define PTRACE_SEIZE 0x4206
#endif
#ifndef PTRACE_INTERRUPT
    #define PTRACE_INTERRUPT 0x4207
#endif
#ifndef PTRACE_GETREGSET
    #define PTRACE_GETREGSET 0x4204
#endif
#ifndef PR_SET_PTRACER
    #define PR_SET_PTRACER 0x59616D61
#endif
#ifndef NT_PRSTATUS
    #define NT_PRSTATUS 1
#endif
#ifndef __WALL
    #define __WALL 0x40000000
#endif
#ifndef FUTEX_WAIT
    #define FUTEX_WAIT 0
#endif
#ifndef FUTEX_WAKE
    #define FUTEX_WAKE 1
#endif
#ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS 0x20
#endif

#if defined(__aarch64__) || defined(__arm__)

namespace JCrasher
{

enum eDumpFlags
{
    DUMP_THREAD_HEADER = (1u << 0),
    DUMP_REGISTERS     = (1u << 1),
    DUMP_SYMBOLS       = (1u << 2),
    DUMP_CRASHED_FIRST = (1u << 3),
    DUMP_ALL_THREADS   = (1u << 4),
    DUMP_DIAGNOSTICS   = (1u << 5)
};

enum eDumpResult
{
    DUMP_OK           = 0,
    DUMP_BAD_FD       = -1,
    DUMP_PIPE_FAILED  = -2,
    DUMP_CLONE_FAILED = -3,
    DUMP_CHILD_FAILED     = -4,
    DUMP_PATH_FAILED      = -5,
    DUMP_HELPER_NOT_INIT  = -6,
    DUMP_HELPER_BUSY      = -7,
    DUMP_MMAP_FAILED      = -8,
    DUMP_FD_SEND_FAILED   = -9
};

namespace Private
{

// Private minimal ELF definitions. Do not include <elf.h> and do not define ELF_*
// macros: Android NDK 20 can include both elf.h and linux/elf.h, and redefining
// ELF_ST_TYPE can create ELF_ST_TYPE <> ELF64_ST_TYPE macro recursion.
static const unsigned JCRASHER_EI_MAG0 = 0;
static const unsigned JCRASHER_EI_MAG1 = 1;
static const unsigned JCRASHER_EI_MAG2 = 2;
static const unsigned JCRASHER_EI_MAG3 = 3;
static const unsigned char JCRASHER_ELFMAG0 = 0x7F;
static const unsigned char JCRASHER_ELFMAG1 = 'E';
static const unsigned char JCRASHER_ELFMAG2 = 'L';
static const unsigned char JCRASHER_ELFMAG3 = 'F';
static const unsigned JCRASHER_EI_CLASS = 4;
static const unsigned char JCRASHER_ELFCLASS32 = 1;
static const unsigned char JCRASHER_ELFCLASS64 = 2;
static const uint16_t JCRASHER_EM_ARM = 40;
static const uint16_t JCRASHER_EM_AARCH64 = 183;
static const uint32_t JCRASHER_PT_LOAD = 1;
static const uint32_t JCRASHER_PT_DYNAMIC = 2;
static const uintptr_t JCRASHER_DT_NULL = 0;
static const uintptr_t JCRASHER_DT_HASH = 4;
static const uintptr_t JCRASHER_DT_STRTAB = 5;
static const uintptr_t JCRASHER_DT_SYMTAB = 6;
static const uintptr_t JCRASHER_DT_SONAME = 14;
static const uintptr_t JCRASHER_DT_SYMENT = 11;
static const uintptr_t JCRASHER_DT_GNU_HASH = 0x6FFFFEF5u;
static const uintptr_t JCRASHER_DT_GNU_EH_FRAME = 0x6FFFFEFCu;
static const uintptr_t JCRASHER_DT_ARM_EXIDX = 0x70000001u;
static const uintptr_t JCRASHER_DT_ARM_EXIDXSZ = 0x70000002u;
static const unsigned JCRASHER_STT_NOTYPE = 0;
static const unsigned JCRASHER_STT_FUNC = 2;
static const uint16_t JCRASHER_SHN_UNDEF = 0;

#if defined(__aarch64__)
struct SElfHeader
{
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};
struct SElfProgramHeader
{
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};
struct SElfDynamic
{
    int64_t d_tag;
    union { uint64_t d_val; uint64_t d_ptr; } d_un;
};
struct SElfSymbol
{
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};
#else
struct SElfHeader
{
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};
struct SElfProgramHeader
{
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};
struct SElfDynamic
{
    int32_t d_tag;
    union { uint32_t d_val; uint32_t d_ptr; } d_un;
};
struct SElfSymbol
{
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
};
#endif

static unsigned GetElfSymbolType(unsigned char st_info)
{
    return (unsigned)(st_info & 0x0F);
}

struct SRemoteRegs
{
#if defined(__aarch64__)
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
#else
    uint32_t uregs[18];  // r0..r15, cpsr, orig_r0
#endif
};

struct SMapRegion
{
    uintptr_t start;
    uintptr_t end;
    uintptr_t offset;
    char perms[5];
    char path[512];
};

struct SObjectInfo
{
    bool used;
    uintptr_t load_bias;
    uintptr_t start;
    uintptr_t end;
    uintptr_t symtab;
    uintptr_t strtab;
    uintptr_t hash;
    uintptr_t gnu_hash;
    uintptr_t eh_frame_hdr;
    uintptr_t arm_exidx;
    uintptr_t arm_exidx_size;
    uint32_t nsyms;
    uint32_t syment;
    char path[512];
    char soname[128];
};

struct SFrameRecord
{
    uintptr_t fp;
    uintptr_t lr;
};

struct SLinuxDirent64
{
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

static bool g_bHaveCrash;
static pid_t g_nCrashTid;
static SRemoteRegs g_CrashRegs;

static SMapRegion g_Maps[4096];
static size_t g_nMapCount;
static SObjectInfo g_Objects[256];
static size_t g_nObjectCount;
static pid_t g_nTargetPid;
static pid_t g_nPeekTid;
static int g_nLastUnwindMethod;
static int g_nLastMetadataReason;

static const uint32_t JCRASHER_PREINIT_MAGIC = 0x4A435250u; // JCRP

struct SPreinitRequest
{
    uint32_t magic;
    uint32_t use_path;
    uint32_t use_passed_fd;
    int fd;
    pid_t target;
    pid_t crash_tid;
    unsigned flags;
    size_t max_frames;
    SRemoteRegs regs;
    char path[512];
};

struct SPreinitShared
{
    volatile int state;
    volatile int result;
    SPreinitRequest request;
};

static SPreinitShared* g_pPreinitShared;
static pid_t g_nPreinitPid;
static int g_nPreinitFd = -1;
static int g_nPreinitSockParent = -1;
static int g_nPreinitSockChild = -1;
static char g_szPreinitPath[512];

static long SysOpenAt(const char* path, int flags, int mode = 0)
{
    return syscall(SYS_openat, AT_FDCWD, path, flags, mode);
}
static long SysClose(int fd)
{ return syscall(SYS_close, fd); }
static long SysRead(int fd, void* buf, size_t n)
{ return syscall(SYS_read, fd, buf, n); }
static long SysWrite(int fd, const void* buf, size_t n)
{ return syscall(SYS_write, fd, buf, n); }
static long SysWait4(pid_t pid, int* st, int options)
{ return syscall(SYS_wait4, pid, st, options, 0); }
static long SysPtrace(long req, pid_t pid, void* addr, void* data)
{ return syscall(SYS_ptrace, req, pid, addr, data); }
static long SysPrctl(long opt, long a2, long a3 = 0, long a4 = 0, long a5 = 0)
{ return syscall(SYS_prctl, opt, a2, a3, a4, a5); }
static long SysFutex(volatile int* addr, int op, int val)
{ return syscall(SYS_futex, addr, op, val, 0, 0, 0); }
static void* SysMmapShared(size_t len)
{
#if defined(SYS_mmap)
    return (void*)syscall(SYS_mmap, 0, len, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
#elif defined(SYS_mmap2)
    return (void*)syscall(SYS_mmap2, 0, len, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
#else
    return mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
#endif
}
static pid_t SysGetPid()
{ return (pid_t)syscall(SYS_getpid); }
static pid_t SysGetTid()
{ return (pid_t)syscall(SYS_gettid); }

static size_t StrLen(const char* s)
{
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

static bool StrEqual(const char* a, const char* b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a++ != *b++) return false;
    }
    return *a == *b;
}

static bool EndsWith(const char* s, const char* suffix)
{
    if (!s || !suffix) return false;
    size_t sl = StrLen(s);
    size_t tl = StrLen(suffix);
    if (tl > sl) return false;
    return StrEqual(s + sl - tl, suffix);
}

static void CopyString(char* dst, size_t cap, const char* src)
{
    if (!dst || cap == 0) return;
    size_t i = 0;
    if (src) {
        while (i + 1 < cap && src[i]) {
            dst[i] = src[i];
            ++i;
        }
    }
    dst[i] = 0;
}

static const char* BaseName(const char* p)
{
    const char* b = p;
    if (!p) return "";
    for (const char* s = p; *s; ++s) {
        if (*s == '/') b = s + 1;
    }
    return b;
}

static bool IsDigit(char c)
{ return c >= '0' && c <= '9'; }
static int GetHexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static const char* SkipSpaces(const char* p)
{
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

static const char* ParseHex(const char* p, uintptr_t* out)
{
    uintptr_t v = 0;
    int any = 0;
    for (;;) {
        int x = GetHexValue(*p);
        if (x < 0) break;
        v = (v << 4) | (uintptr_t)x;
        ++p;
        any = 1;
    }
    if (!any) return 0;
    *out = v;
    return p;
}

static const char* ParseDec(const char* p, pid_t* out)
{
    long v = 0;
    int any = 0;
    while (IsDigit(*p)) {
        v = v * 10 + (*p - '0');
        ++p;
        any = 1;
    }
    if (!any) return 0;
    *out = (pid_t)v;
    return p;
}

static void AppendChar(char* b, size_t cap, size_t* n, char c)
{
    if (*n + 1 < cap) b[(*n)++] = c;
    if (cap) b[*n < cap ? *n : cap - 1] = 0;
}

static void AppendString(char* b, size_t cap, size_t* n, const char* s)
{
    if (!s) return;
    while (*s) AppendChar(b, cap, n, *s++);
}

static void AppendDec(char* b, size_t cap, size_t* n, uint64_t v)
{
    char tmp[32];
    size_t m = 0;
    if (v == 0) tmp[m++] = '0';
    while (v && m < sizeof(tmp)) {
        tmp[m++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (m) AppendChar(b, cap, n, tmp[--m]);
}

static void AppendHex(char* b, size_t cap, size_t* n, uint64_t v, int width, bool prefix)
{
    static const char kHex[] = "0123456789ABCDEF";
    if (prefix) AppendString(b, cap, n, "0x");
    for (int i = width - 1; i >= 0; --i) {
        AppendChar(b, cap, n, kHex[(v >> (i * 4)) & 0xF]);
    }
}

static void AppendHexVar(char* b, size_t cap, size_t* n, uint64_t v, bool prefix)
{
    static const char kHex[] = "0123456789ABCDEF";
    if (prefix) AppendString(b, cap, n, "0x");
    bool seen = false;
    for (int i = 15; i >= 0; --i) {
        unsigned x = (unsigned)((v >> (i * 4)) & 0xF);
        if (x || seen || i == 0) {
            seen = true;
            AppendChar(b, cap, n, kHex[x]);
        }
    }
}

static void BuildPidPath(char* out, size_t cap, const char* a, pid_t pid, const char* b)
{
    size_t n = 0;
    AppendString(out, cap, &n, a);
    AppendDec(out, cap, &n, (uint64_t)pid);
    AppendString(out, cap, &n, b);
    if (cap) out[n < cap ? n : cap - 1] = 0;
}

static void BuildTidPath(char* out, size_t cap, const char* a, pid_t pid, const char* b,
                                                              pid_t tid, const char* c)
{
    size_t n = 0;
    AppendString(out, cap, &n, a);
    AppendDec(out, cap, &n, (uint64_t)pid);
    AppendString(out, cap, &n, b);
    AppendDec(out, cap, &n, (uint64_t)tid);
    AppendString(out, cap, &n, c);
    if (cap) out[n < cap ? n : cap - 1] = 0;
}

static void WriteAll(int fd, const void* p, size_t n)
{
    const char* s = (const char*)p;
    while (n) {
        long w = SysWrite(fd, s, n);
        if (w <= 0) break;
        s += (size_t)w;
        n -= (size_t)w;
    }
}

static void WriteString(int fd, const char* s)
{ WriteAll(fd, s, StrLen(s)); }

static bool ReadTargetPtrace(pid_t tid, uintptr_t addr, void* dst, size_t len)
{
    if (tid <= 0 || !addr || !dst || !len) return false;

    unsigned char* out = (unsigned char*)dst;
    size_t done = 0;
    const size_t word_size = sizeof(long);

    while (done < len) {
        uintptr_t cur = addr + done;
        uintptr_t aligned = cur & ~(uintptr_t)(word_size - 1);
        size_t shift = (size_t)(cur - aligned);

        errno = 0;
        long word = SysPtrace(PTRACE_PEEKDATA, tid, (void*)aligned, 0);
        if (word == -1 && errno != 0) return false;

        unsigned char* bytes = (unsigned char*)&word;
        size_t take = word_size - shift;
        if (take > len - done) take = len - done;

        for (size_t i = 0; i < take; ++i) out[done + i] = bytes[shift + i];
        done += take;
    }
    return true;
}

static bool ReadTarget(pid_t pid, uintptr_t addr, void* dst, size_t len)
{
    if (!addr || !dst || !len) return false;

    // OEM kernels/seccomp sometimes block process_vm_readv even for same-process
    // crash dumping. For the crashing thread path we can read our own mapped
    // memory directly, but only after /proc/self/maps has been loaded and the
    // requested span is known to be readable.
    // Idiots are modifying Android like it's kids toys... *facepalm*
    if (pid == SysGetPid() && g_nMapCount != 0) {
        bool readable = false;
        for (size_t i = 0; i < g_nMapCount; ++i) {
            if (g_Maps[i].perms[0] != 'r') continue;
            if (addr >= g_Maps[i].start && addr + len >= addr && addr + len <= g_Maps[i].end) {
                readable = true;
                break;
            }
        }
        if (!readable) return false;

        volatile const unsigned char* src = (volatile const unsigned char*)addr;
        unsigned char* out = (unsigned char*)dst;
        for (size_t i = 0; i < len; ++i) out[i] = src[i];
        return true;
    }

    struct iovec local;
    local.iov_base = dst;
    local.iov_len = len;
    struct iovec remote;
    remote.iov_base = (void*)addr;
    remote.iov_len = len;
    long r = syscall(SYS_process_vm_readv, pid, &local, 1, &remote, 1, 0);
    if (r == (long)len) return true;

    // If the dumper attached at least one thread from the target process,
    // use ptrace word reads as a second remote-memory backend. This helps
    // on OEM ROMs where process_vm_readv() is denied or crippled.
    if (g_nPeekTid > 0) return ReadTargetPtrace(g_nPeekTid, addr, dst, len);
    return false;
}

static bool ReadU32(pid_t pid, uintptr_t addr, uint32_t* out)
{
    return ReadTarget(pid, addr, out, sizeof(*out));
}

static bool ReadStringTarget(pid_t pid, uintptr_t addr, char* out, size_t cap)
{
    if (!out || cap == 0) return false;
    out[0] = 0;
    for (size_t i = 0; i + 1 < cap; ++i) {
        char c = 0;
        if (!ReadTarget(pid, addr + i, &c, 1)) return i != 0;
        out[i] = c;
        if (c == 0) return true;
    }
    out[cap - 1] = 0;
    return true;
}

static uintptr_t StripPc(uintptr_t a)
{
#if defined(__aarch64__)
    register uintptr_t x30 __asm__("x30") = a;
    __asm__("xpaclri" : "+r"(x30));
    return x30;
#else
    return a & ~(uintptr_t)1;
#endif
}

static uintptr_t RewindLr(uintptr_t lr)
{
#if defined(__aarch64__)
    lr = StripPc(lr);
    return lr > 4 ? lr - 4 : lr;
#else
    bool thumb = (lr & 1) != 0;
    lr = StripPc(lr);
    return lr > (thumb ? 2u : 4u) ? lr - (thumb ? 2u : 4u) : lr;
#endif
}

static void CaptureCrash(void* ctx, pid_t tid)
{
    g_bHaveCrash = false;
    g_nCrashTid = tid;
    if (!ctx) return;

    mcontext_t* mc = &((ucontext_t*)ctx)->uc_mcontext;
#if defined(__aarch64__)
    for (int i = 0; i < 31; ++i) g_CrashRegs.regs[i] = (uint64_t)mc->regs[i];
    g_CrashRegs.sp = (uint64_t)mc->sp;
    g_CrashRegs.pc = (uint64_t)mc->pc;
    g_CrashRegs.pstate = (uint64_t)mc->pstate;
#else
    g_CrashRegs.uregs[0] = (uint32_t)mc->arm_r0;
    g_CrashRegs.uregs[1] = (uint32_t)mc->arm_r1;
    g_CrashRegs.uregs[2] = (uint32_t)mc->arm_r2;
    g_CrashRegs.uregs[3] = (uint32_t)mc->arm_r3;
    g_CrashRegs.uregs[4] = (uint32_t)mc->arm_r4;
    g_CrashRegs.uregs[5] = (uint32_t)mc->arm_r5;
    g_CrashRegs.uregs[6] = (uint32_t)mc->arm_r6;
    g_CrashRegs.uregs[7] = (uint32_t)mc->arm_r7;
    g_CrashRegs.uregs[8] = (uint32_t)mc->arm_r8;
    g_CrashRegs.uregs[9] = (uint32_t)mc->arm_r9;
    g_CrashRegs.uregs[10] = (uint32_t)mc->arm_r10;
    g_CrashRegs.uregs[11] = (uint32_t)mc->arm_fp;
    g_CrashRegs.uregs[12] = (uint32_t)mc->arm_ip;
    g_CrashRegs.uregs[13] = (uint32_t)mc->arm_sp;
    g_CrashRegs.uregs[14] = (uint32_t)mc->arm_lr;
    g_CrashRegs.uregs[15] = (uint32_t)mc->arm_pc;
    g_CrashRegs.uregs[16] = (uint32_t)mc->arm_cpsr;
    g_CrashRegs.uregs[17] = 0;
#endif
    g_bHaveCrash = true;
}

static void SeedRegs(const SRemoteRegs* r, uintptr_t* pc, uintptr_t* sp, uintptr_t* fp,
                                            uintptr_t* lr, uintptr_t* fp_alt)
{
#if defined(__aarch64__)
    *pc = (uintptr_t)r->pc;
    *sp = (uintptr_t)r->sp;
    *fp = (uintptr_t)r->regs[29];
    *lr = (uintptr_t)r->regs[30];
    *fp_alt = 0;
#else
    *pc = (uintptr_t)r->uregs[15];
    *sp = (uintptr_t)r->uregs[13];
    *fp = (uintptr_t)r->uregs[11];      // r11 frame pointer
    *lr = (uintptr_t)r->uregs[14];
    *fp_alt = (uintptr_t)r->uregs[7];   // r7 frame pointer for Thumb builds
#endif
}

static bool GetRegs(pid_t tid, SRemoteRegs* out)
{
    struct iovec iov;
    iov.iov_base = out;
    iov.iov_len = sizeof(*out);
    return SysPtrace(PTRACE_GETREGSET, tid, (void*)NT_PRSTATUS, &iov) == 0;
}

static bool ParseMapsLine(const char* line, SMapRegion* m)
{
    const char* p = line;
    p = ParseHex(p, &m->start);
    if (!p || *p != '-') return false;
    ++p;
    p = ParseHex(p, &m->end);
    if (!p) return false;
    p = SkipSpaces(p);
    for (int i = 0; i < 4; ++i) m->perms[i] = p[i] ? p[i] : '-';
    m->perms[4] = 0;
    while (*p && *p != ' ' && *p != '\t') ++p;
    p = SkipSpaces(p);
    p = ParseHex(p, &m->offset);
    if (!p) return false;
    // skip dev
    p = SkipSpaces(p);
    while (*p && *p != ' ' && *p != '\t') ++p;
    // skip inode
    p = SkipSpaces(p);
    while (*p && *p != ' ' && *p != '\t') ++p;
    p = SkipSpaces(p);
    size_t i = 0;
    while (i + 1 < sizeof(m->path) && p[i] && p[i] != '\n' && p[i] != '\r') {
        m->path[i] = p[i];
        ++i;
    }
    m->path[i] = 0;
    return m->start < m->end;
}

static bool LoadMapsFor(pid_t pid)
{
    g_nMapCount = 0;
    g_nObjectCount = 0;

    char path[64];
    BuildPidPath(path, sizeof(path), "/proc/", pid, "/maps");
    int fd = (int)SysOpenAt(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0 && pid != SysGetPid()) {
        BuildPidPath(path, sizeof(path), "/proc/", SysGetPid(), "/maps");
        fd = (int)SysOpenAt(path, O_RDONLY | O_CLOEXEC);
    }
    if (fd < 0) return false;

    char rb[4096];
    char line[1024];
    size_t ln = 0;
    for (;;) {
        long r = SysRead(fd, rb, sizeof(rb));
        if (r <= 0) break;
        for (long i = 0; i < r; ++i) {
            char c = rb[i];
            if (ln + 1 < sizeof(line)) line[ln++] = c;
            if (c == '\n') {
                line[ln] = 0;
                if (g_nMapCount < sizeof(g_Maps) / sizeof(g_Maps[0])) {
                    SMapRegion m;
                    if (ParseMapsLine(line, &m)) g_Maps[g_nMapCount++] = m;
                }
                ln = 0;
            }
        }
    }
    if (ln && g_nMapCount < sizeof(g_Maps) / sizeof(g_Maps[0])) {
        line[ln] = 0;
        SMapRegion m;
        if (ParseMapsLine(line, &m)) g_Maps[g_nMapCount++] = m;
    }
    SysClose(fd);
    return g_nMapCount != 0;
}

static int FindMap(uintptr_t pc)
{
    for (size_t i = 0; i < g_nMapCount; ++i) {
        if (pc >= g_Maps[i].start && pc < g_Maps[i].end) return (int)i;
    }
    return -1;
}

static bool MapContains(uintptr_t addr, const SMapRegion* m, size_t len)
{
    return m && addr >= m->start && addr + len >= addr && addr + len <= m->end;
}

static bool ReadFrame(pid_t pid, uintptr_t fp, const SMapRegion* stack, SFrameRecord* out)
{
    if (!MapContains(fp, stack, sizeof(SFrameRecord))) return false;
    return ReadTarget(pid, fp, out, sizeof(*out));
}

static size_t UnwindOneFp(pid_t pid, uintptr_t pc, uintptr_t sp, uintptr_t fp, uintptr_t lr,
                                                        uintptr_t* frames, size_t cap)
{
    size_t n = 0;
    if (cap == 0) return 0;
    if (pc) frames[n++] = StripPc(pc);
    if (n < cap && lr) {
        uintptr_t call = RewindLr(lr);
        if (call && call != frames[0]) frames[n++] = call;
    }

    int stack_i = FindMap(sp);
    if (stack_i < 0) return n;
    const SMapRegion* stack = &g_Maps[stack_i];
    if (stack->perms[0] != 'r') return n;

    uintptr_t prev = 0;
    while (n < cap) {
        if (!fp || (fp & (sizeof(uintptr_t) - 1)) != 0 || fp <= prev) break;
        if (!MapContains(fp, stack, sizeof(SFrameRecord))) break;

        SFrameRecord rec;
        if (!ReadFrame(pid, fp, stack, &rec)) break;
        uintptr_t call = RewindLr(rec.lr);
        if (call) {
            bool dup = false;
            for (size_t i = 0; i < n; ++i) {
                if (frames[i] == call) { dup = true; break; }
            }
            if (!dup) frames[n++] = call;
        }
        prev = fp;
        fp = rec.fp;
    }
    return n;
}

static bool IsExecutablePc(uintptr_t pc)
{
    pc = StripPc(pc);
    int mi = FindMap(pc);
    if (mi < 0) return false;
    return g_Maps[mi].perms[2] == 'x';
}

static bool AppendUniqueFrame(uintptr_t* frames, size_t* n, size_t cap, uintptr_t pc)
{
    pc = StripPc(pc);
    if (!pc || !IsExecutablePc(pc)) return false;
    for (size_t i = 0; i < *n; ++i) {
        if (frames[i] == pc) return false;
        uintptr_t a = frames[i] > pc ? frames[i] - pc : pc - frames[i];
        if (a < 4) return false;
    }
    if (*n >= cap) return false;
    frames[(*n)++] = pc;
    return true;
}

static size_t StackScanFallback(pid_t pid, uintptr_t sp, uintptr_t* frames, size_t n, size_t cap)
{
    if (n >= cap || !sp) return n;

    int stack_i = FindMap(sp);
    if (stack_i < 0) return n;
    const SMapRegion* stack = &g_Maps[stack_i];
    if (stack->perms[0] != 'r') return n;

    uintptr_t start = sp;
    uintptr_t end = stack->end;

    // Keep this bounded. It is a fallback, not a full conservative GC scan.
    const uintptr_t kMaxScan = 256u * 1024u;
    if (end > start + kMaxScan) end = start + kMaxScan;

    start = (start + sizeof(uintptr_t) - 1) & ~(uintptr_t)(sizeof(uintptr_t) - 1);

    for (uintptr_t p = start; p + sizeof(uintptr_t) <= end && n < cap; p += sizeof(uintptr_t)) {
        uintptr_t v = 0;
        if (!ReadTarget(pid, p, &v, sizeof(v))) break;

        uintptr_t call = RewindLr(v);
        if (!IsExecutablePc(call)) continue;

        // Avoid dumping dense junk from literal tables or unrelated pointers:
        // require sane module mapping and keep first unique callsite only.
        AppendUniqueFrame(frames, &n, cap, call);
    }

    return n;
}


static SObjectInfo* GetObjectForPc(pid_t pid, uintptr_t pc);

static bool ReadByteTarget(pid_t pid, uintptr_t addr, unsigned char* out)
{ return ReadTarget(pid, addr, out, 1); }

static bool ReadU16Target(pid_t pid, uintptr_t addr, uint16_t* out)
{ return ReadTarget(pid, addr, out, sizeof(*out)); }

static bool ReadU64Target(pid_t pid, uintptr_t addr, uint64_t* out)
{ return ReadTarget(pid, addr, out, sizeof(*out)); }

static bool ReadSleb(pid_t pid, uintptr_t* p, uintptr_t end, int64_t* out)
{
    uint64_t result = 0;
    unsigned shift = 0;
    unsigned char byte = 0;
    uintptr_t cur = *p;

    for (;;) {
        if (cur >= end || shift >= 64) return false;
        if (!ReadByteTarget(pid, cur++, &byte)) return false;
        result |= ((uint64_t)(byte & 0x7F)) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) break;
    }
    if ((byte & 0x40) && shift < 64) result |= (~0ULL) << shift;
    *p = cur;
    *out = (int64_t)result;
    return true;
}

static bool ReadUleb(pid_t pid, uintptr_t* p, uintptr_t end, uint64_t* out)
{
    uint64_t result = 0;
    unsigned shift = 0;
    uintptr_t cur = *p;

    for (;;) {
        unsigned char byte = 0;
        if (cur >= end || shift >= 64) return false;
        if (!ReadByteTarget(pid, cur++, &byte)) return false;
        result |= ((uint64_t)(byte & 0x7F)) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    *p = cur;
    *out = result;
    return true;
}

static int64_t SignExtend(uint64_t v, unsigned bits)
{
    uint64_t m = 1ULL << (bits - 1);
    return (int64_t)((v ^ m) - m);
}

static uint32_t Prel31ToAddr(uintptr_t place, uint32_t v)
{
    int32_t off = (int32_t)((v & 0x40000000u) ? (v | 0x80000000u) : (v & 0x7FFFFFFFu));
    return (uint32_t)(place + off);
}

static bool ReadEncodedPointer(pid_t pid, uintptr_t* p, unsigned char enc, uintptr_t data_base,
                               bool apply_relative, uintptr_t* out)
{
    static const unsigned char DW_EH_PE_omit    = 0xFF;
    static const unsigned char DW_EH_PE_absptr  = 0x00;
    static const unsigned char DW_EH_PE_uleb128 = 0x01;
    static const unsigned char DW_EH_PE_udata2  = 0x02;
    static const unsigned char DW_EH_PE_udata4  = 0x03;
    static const unsigned char DW_EH_PE_udata8  = 0x04;
    static const unsigned char DW_EH_PE_sleb128 = 0x09;
    static const unsigned char DW_EH_PE_sdata2  = 0x0A;
    static const unsigned char DW_EH_PE_sdata4  = 0x0B;
    static const unsigned char DW_EH_PE_sdata8  = 0x0C;
    static const unsigned char DW_EH_PE_pcrel   = 0x10;
    static const unsigned char DW_EH_PE_datarel = 0x30;
    static const unsigned char DW_EH_PE_indirect = 0x80;

    if (enc == DW_EH_PE_omit) return false;

    uintptr_t cur = *p;
    uintptr_t start = cur;
    uint64_t u = 0;
    int64_t s = 0;
    bool is_signed = false;

    switch (enc & 0x0F) {
        case DW_EH_PE_absptr:
            if (!ReadTarget(pid, cur, &u, sizeof(uintptr_t))) return false;
            u &= sizeof(uintptr_t) == 4 ? 0xFFFFFFFFULL : ~0ULL;
            cur += sizeof(uintptr_t);
            break;
        case DW_EH_PE_uleb128:
            if (!ReadUleb(pid, &cur, cur + 16, &u)) return false;
            break;
        case DW_EH_PE_udata2: {
            uint16_t v = 0; if (!ReadU16Target(pid, cur, &v)) return false; u = v; cur += 2; break;
        }
        case DW_EH_PE_udata4: {
            uint32_t v = 0; if (!ReadU32(pid, cur, &v)) return false; u = v; cur += 4; break;
        }
        case DW_EH_PE_udata8: {
            uint64_t v = 0; if (!ReadU64Target(pid, cur, &v)) return false; u = v; cur += 8; break;
        }
        case DW_EH_PE_sleb128:
            if (!ReadSleb(pid, &cur, cur + 16, &s)) return false;
            is_signed = true;
            break;
        case DW_EH_PE_sdata2: {
            uint16_t v = 0; if (!ReadU16Target(pid, cur, &v)) return false; s = SignExtend(v, 16); cur += 2; is_signed = true; break;
        }
        case DW_EH_PE_sdata4: {
            uint32_t v = 0; if (!ReadU32(pid, cur, &v)) return false; s = SignExtend(v, 32); cur += 4; is_signed = true; break;
        }
        case DW_EH_PE_sdata8: {
            uint64_t v = 0; if (!ReadU64Target(pid, cur, &v)) return false; s = (int64_t)v; cur += 8; is_signed = true; break;
        }
        default:
            return false;
    }

    uintptr_t value = is_signed ? (uintptr_t)s : (uintptr_t)u;
    if (apply_relative) {
        switch (enc & 0x70) {
            case 0x00: break;
            case DW_EH_PE_pcrel: value += start; break;
            case DW_EH_PE_datarel: value += data_base; break;
            default: return false;
        }
    }

    if (enc & DW_EH_PE_indirect) {
        uintptr_t tmp = 0;
        if (!ReadTarget(pid, value, &tmp, sizeof(tmp))) return false;
        value = tmp;
    }

    *p = cur;
    *out = value;
    return true;
}

static int EhEncodedSize(unsigned char enc)
{
    switch (enc & 0x0F) {
        case 0x00: return (int)sizeof(uintptr_t);
        case 0x02: case 0x0A: return 2;
        case 0x03: case 0x0B: return 4;
        case 0x04: case 0x0C: return 8;
        default: return 0;
    }
}

struct SCfiRule
{
    unsigned char type; // 0 same, 1 offset, 2 undefined, 3 register, 4 val_offset
    int64_t offset;
    unsigned reg;
};

struct SCfiState
{
    unsigned cfa_reg;
    int64_t cfa_off;
    SCfiRule regs[32];
};

struct SCieInfo
{
    uintptr_t cie_start;
    uintptr_t instr_start;
    uintptr_t instr_end;
    uint64_t code_align;
    int64_t data_align;
    unsigned return_reg;
    unsigned char fde_enc;
    bool z_aug;
};

static void CfiInitState(SCfiState* st)
{
    st->cfa_reg = 0;
    st->cfa_off = 0;
    for (unsigned i = 0; i < 32; ++i) {
        st->regs[i].type = 0;
        st->regs[i].offset = 0;
        st->regs[i].reg = i;
    }
}

static bool CfiSkipEncoded(pid_t pid, uintptr_t* p, unsigned char enc, uintptr_t data_base)
{
    uintptr_t ignored = 0;
    return ReadEncodedPointer(pid, p, enc, data_base, true, &ignored);
}


struct SDwarfLength
{
    uintptr_t body;
    uintptr_t end;
    bool is64;
};

static bool ReadDwarfLength(pid_t pid, uintptr_t addr, SDwarfLength* out)
{
    uint32_t len32 = 0;
    if (!ReadU32(pid, addr, &len32) || len32 == 0) return false;

    if (len32 == 0xFFFFFFFFu) {
        uint64_t len64 = 0;
        if (!ReadU64Target(pid, addr + 4, &len64)) return false;
        if (len64 == 0 || len64 > (64ULL * 1024ULL * 1024ULL)) return false;
        out->body = addr + 12;
        out->end = out->body + (uintptr_t)len64;
        out->is64 = true;
        return out->end > out->body;
    }

    if (len32 > (64u * 1024u * 1024u)) return false;
    out->body = addr + 4;
    out->end = out->body + (uintptr_t)len32;
    out->is64 = false;
    return out->end > out->body;
}

static bool ReadDwarfUnsigned(pid_t pid, uintptr_t addr, bool is64, uint64_t* out)
{
    if (is64) return ReadU64Target(pid, addr, out);
    uint32_t v = 0;
    if (!ReadU32(pid, addr, &v)) return false;
    *out = v;
    return true;
}

static bool ParseCie(pid_t pid, uintptr_t cie_addr, SCieInfo* out)
{
    SDwarfLength dl;
    if (!ReadDwarfLength(pid, cie_addr, &dl)) return false;

    uint64_t id = 1;
    if (!ReadDwarfUnsigned(pid, dl.body, dl.is64, &id) || id != 0) return false;

    uintptr_t p = dl.body + (dl.is64 ? 8 : 4);
    uintptr_t end = dl.end;
    unsigned char version = 0;
    if (p >= end || !ReadByteTarget(pid, p++, &version)) return false;
    if (version != 1 && version != 3 && version != 4) return false;

    char aug[64];
    size_t an = 0;
    for (; p < end && an + 1 < sizeof(aug); ++p) {
        unsigned char c = 0;
        if (!ReadByteTarget(pid, p, &c)) return false;
        aug[an++] = (char)c;
        if (c == 0) break;
    }
    if (an == 0 || aug[an - 1] != 0) return false;

    uint64_t code_align = 0;
    int64_t data_align = 0;
    uint64_t ret = 0;
    if (!ReadUleb(pid, &p, end, &code_align)) return false;
    if (!ReadSleb(pid, &p, end, &data_align)) return false;
    if (version == 1) {
        unsigned char r = 0;
        if (p >= end || !ReadByteTarget(pid, p++, &r)) return false;
        ret = r;
    } else {
        if (!ReadUleb(pid, &p, end, &ret)) return false;
    }

    unsigned char fde_enc = 0x00;
    bool z_aug = (aug[0] == 'z');
    if (z_aug) {
        uint64_t aug_len = 0;
        if (!ReadUleb(pid, &p, end, &aug_len)) return false;
        uintptr_t aug_end = p + (uintptr_t)aug_len;
        if (aug_end < p || aug_end > end) return false;
        for (size_t i = 1; aug[i] && p < aug_end; ++i) {
            if (aug[i] == 'P') {
                unsigned char enc = 0;
                if (!ReadByteTarget(pid, p++, &enc)) return false;
                if (!CfiSkipEncoded(pid, &p, enc, cie_addr)) return false;
            } else if (aug[i] == 'L') {
                if (p >= aug_end) return false;
                ++p;
            } else if (aug[i] == 'R') {
                if (p >= aug_end || !ReadByteTarget(pid, p++, &fde_enc)) return false;
            } else if (aug[i] == 'S') {
                // Signal frame marker. No payload.
            } else if (aug[i] == 'B') {
                // AArch64 BTI marker. No payload.
            } else {
                // Unknown augmentation. The length is authoritative, skip it.
                p = aug_end;
                break;
            }
        }
        p = aug_end;
    }
    if (p > end) return false;

    out->cie_start = cie_addr;
    out->instr_start = p;
    out->instr_end = end;
    out->code_align = code_align ? code_align : 1;
    out->data_align = data_align ? data_align : -(int)sizeof(uintptr_t);
    out->return_reg = (unsigned)ret;
    out->fde_enc = fde_enc;
    out->z_aug = z_aug;
    return true;
}

static bool EvalCfiInstructions(pid_t pid, uintptr_t pc, uintptr_t code_start,
                                uintptr_t p, uintptr_t end, const SCieInfo* cie, SCfiState* st)
{
    uintptr_t loc = code_start;
    SCfiState stack[32];
    unsigned stack_n = 0;

    while (p < end) {
        unsigned char op = 0;
        if (!ReadByteTarget(pid, p++, &op)) return false;

        unsigned primary = op & 0xC0;
        if (primary == 0x40) { // DW_CFA_advance_loc
            loc += (uintptr_t)(op & 0x3F) * (uintptr_t)cie->code_align;
            if (loc > pc) break;
            continue;
        }
        if (primary == 0x80) { // DW_CFA_offset
            uint64_t off = 0;
            if (!ReadUleb(pid, &p, end, &off)) return false;
            unsigned reg = op & 0x3F;
            if (reg < 32) {
                st->regs[reg].type = 1;
                st->regs[reg].offset = (int64_t)off * cie->data_align;
            }
            continue;
        }
        if (primary == 0xC0) { // DW_CFA_restore
            unsigned reg = op & 0x3F;
            if (reg < 32) {
                st->regs[reg].type = 0;
                st->regs[reg].offset = 0;
                st->regs[reg].reg = reg;
            }
            continue;
        }

        switch (op) {
            case 0x00: break; // DW_CFA_nop
            case 0x01: { // DW_CFA_set_loc: address-sized absolute target address.
                uintptr_t v = 0;
                if (!ReadTarget(pid, p, &v, sizeof(v))) return false;
                p += sizeof(v);
                loc = v;
                if (loc > pc) return true;
                break;
            }
            case 0x02: { unsigned char v=0; if(p>=end||!ReadByteTarget(pid,p++,&v)) return false; loc += (uintptr_t)v * cie->code_align; if(loc > pc) return true; break; }
            case 0x03: { uint16_t v=0; if(p+2>end||!ReadU16Target(pid,p,&v)) return false; p+=2; loc += (uintptr_t)v * cie->code_align; if(loc > pc) return true; break; }
            case 0x04: { uint32_t v=0; if(p+4>end||!ReadU32(pid,p,&v)) return false; p+=4; loc += (uintptr_t)v * cie->code_align; if(loc > pc) return true; break; }
            case 0x05: { uint64_t reg=0, off=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadUleb(pid,&p,end,&off)) return false; if(reg<32){st->regs[reg].type=1; st->regs[reg].offset=(int64_t)off*cie->data_align;} break; }
            case 0x06: { uint64_t reg=0; if(!ReadUleb(pid,&p,end,&reg)) return false; if(reg<32){st->regs[reg].type=0; st->regs[reg].offset=0; st->regs[reg].reg=(unsigned)reg;} break; }
            case 0x07: { uint64_t reg=0; if(!ReadUleb(pid,&p,end,&reg)) return false; if(reg<32) st->regs[reg].type=2; break; }
            case 0x08: { uint64_t reg=0; if(!ReadUleb(pid,&p,end,&reg)) return false; if(reg<32) st->regs[reg].type=0; break; }
            case 0x09: { uint64_t reg=0, reg2=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadUleb(pid,&p,end,&reg2)) return false; if(reg<32){st->regs[reg].type=3; st->regs[reg].reg=(unsigned)reg2;} break; }
            case 0x0A: if (stack_n < sizeof(stack) / sizeof(stack[0])) stack[stack_n++] = *st; else return false; break;
            case 0x0B: if (stack_n) *st = stack[--stack_n]; else return false; break;
            case 0x0C: { uint64_t reg=0, off=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadUleb(pid,&p,end,&off)) return false; st->cfa_reg=(unsigned)reg; st->cfa_off=(int64_t)off; break; }
            case 0x0D: { uint64_t reg=0; if(!ReadUleb(pid,&p,end,&reg)) return false; st->cfa_reg=(unsigned)reg; break; }
            case 0x0E: { uint64_t off=0; if(!ReadUleb(pid,&p,end,&off)) return false; st->cfa_off=(int64_t)off; break; }
            case 0x0F: { uint64_t len=0; if(!ReadUleb(pid,&p,end,&len)) return false; if (p + (uintptr_t)len < p || p + (uintptr_t)len > end) return false; p += (uintptr_t)len; break; } // DW_CFA_def_cfa_expression: unsupported, but skip expression.
            case 0x10: { uint64_t reg=0,len=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadUleb(pid,&p,end,&len)) return false; if (p + (uintptr_t)len < p || p + (uintptr_t)len > end) return false; p += (uintptr_t)len; if (reg < 32) st->regs[reg].type = 2; break; } // expression unsupported.
            case 0x11: { uint64_t reg=0; int64_t off=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadSleb(pid,&p,end,&off)) return false; if(reg<32){st->regs[reg].type=1; st->regs[reg].offset=off*cie->data_align;} break; }
            case 0x12: { uint64_t reg=0; int64_t off=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadSleb(pid,&p,end,&off)) return false; st->cfa_reg=(unsigned)reg; st->cfa_off=off*cie->data_align; break; }
            case 0x13: { int64_t off=0; if(!ReadSleb(pid,&p,end,&off)) return false; st->cfa_off=off*cie->data_align; break; }
            case 0x14: { uint64_t reg=0, off=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadUleb(pid,&p,end,&off)) return false; if(reg<32){st->regs[reg].type=4; st->regs[reg].offset=(int64_t)off*cie->data_align;} break; }
            case 0x15: { uint64_t reg=0; int64_t off=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadSleb(pid,&p,end,&off)) return false; if(reg<32){st->regs[reg].type=4; st->regs[reg].offset=off*cie->data_align;} break; }
            case 0x16: { uint64_t reg=0,len=0; if(!ReadUleb(pid,&p,end,&reg)||!ReadUleb(pid,&p,end,&len)) return false; if (p + (uintptr_t)len < p || p + (uintptr_t)len > end) return false; p += (uintptr_t)len; if (reg < 32) st->regs[reg].type = 2; break; } // val_expression unsupported.
            case 0x1D: break; // DW_CFA_GNU_window_save, irrelevant for ARM/AArch64.
            case 0x2D: break; // DW_CFA_AARCH64_negate_ra_state, PAC state only; StripPc handles it.
            case 0x2E: { uint64_t ignored=0; if(!ReadUleb(pid,&p,end,&ignored)) return false; break; } // DW_CFA_GNU_args_size
            default:
                return false;
        }
        if (p > end) return false;
    }
    return true;
}

#if defined(__aarch64__)
static bool FindFdeFromEhFrameHdr(pid_t pid, const SObjectInfo* o, uintptr_t pc,
                                  uintptr_t* fde_addr_out)
{
    if (!o || !o->eh_frame_hdr) return false;

    uintptr_t hdr = o->eh_frame_hdr;
    unsigned char b[4];
    if (!ReadTarget(pid, hdr, b, sizeof(b))) return false;
    if (b[0] != 1) return false;

    uintptr_t p = hdr + 4;
    uintptr_t eh_frame = 0;
    uintptr_t count_v = 0;
    if (!ReadEncodedPointer(pid, &p, b[1], hdr, true, &eh_frame)) return false;
    if (!ReadEncodedPointer(pid, &p, b[2], hdr, true, &count_v)) return false;
    if (!eh_frame || count_v == 0 || count_v > 262144) return false;

    unsigned char table_enc = b[3];
    int esz = EhEncodedSize(table_enc);
    if (!esz) return false;
    uintptr_t table = p;
    size_t lo = 0;
    size_t hi = (size_t)count_v;
    uintptr_t best_start = 0;
    uintptr_t best_fde = 0;

    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        uintptr_t ep = table + (uintptr_t)mid * (uintptr_t)(esz * 2);
        uintptr_t start = 0;
        uintptr_t fde = 0;
        if (!ReadEncodedPointer(pid, &ep, table_enc, hdr, true, &start)) return false;
        if (!ReadEncodedPointer(pid, &ep, table_enc, hdr, true, &fde)) return false;
        if (start <= pc) {
            best_start = start;
            best_fde = fde;
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (!best_start || !best_fde) return false;
    *fde_addr_out = best_fde;
    return true;
}

static bool CfiUnwindStepA64(pid_t pid, const SRemoteRegs* cur, SRemoteRegs* next)
{
    uintptr_t pc = StripPc((uintptr_t)cur->pc);
    if (!pc) return false;
    SObjectInfo* o = GetObjectForPc(pid, pc);
    if (!o || !o->eh_frame_hdr) return false;

    uintptr_t fde = 0;
    if (!FindFdeFromEhFrameHdr(pid, o, pc, &fde)) return false;

    SDwarfLength fdl;
    if (!ReadDwarfLength(pid, fde, &fdl)) return false;
    uint64_t cie_off = 0;
    if (!ReadDwarfUnsigned(pid, fdl.body, fdl.is64, &cie_off) || cie_off == 0) return false;
    uintptr_t fde_end = fdl.end;
    uintptr_t cie_addr = fdl.body - (uintptr_t)cie_off;

    SCieInfo cie;
    if (!ParseCie(pid, cie_addr, &cie)) return false;

    uintptr_t p = fdl.body + (fdl.is64 ? 8 : 4);
    uintptr_t start = 0;
    uintptr_t range = 0;
    if (!ReadEncodedPointer(pid, &p, cie.fde_enc, o->eh_frame_hdr, true, &start)) return false;
    if (!ReadEncodedPointer(pid, &p, cie.fde_enc & 0x0F, 0, false, &range)) return false;
    if (!range || pc < start || pc >= start + range) return false;

    if (cie.z_aug) {
        uint64_t aug_len = 0;
        if (!ReadUleb(pid, &p, fde_end, &aug_len)) return false;
        if (p + (uintptr_t)aug_len < p || p + (uintptr_t)aug_len > fde_end) return false;
        p += (uintptr_t)aug_len;
    }

    SCfiState st;
    CfiInitState(&st);
    if (!EvalCfiInstructions(pid, start, start, cie.instr_start, cie.instr_end, &cie, &st)) return false;
    if (!EvalCfiInstructions(pid, pc, start, p, fde_end, &cie, &st)) return false;

    uint64_t vals[32];
    for (unsigned i = 0; i < 31; ++i) vals[i] = cur->regs[i];
    vals[31] = cur->sp;

    if (st.cfa_reg >= 32) return false;
    uintptr_t cfa = (uintptr_t)vals[st.cfa_reg] + (uintptr_t)st.cfa_off;
    if (!cfa || cfa < cur->sp || cfa - cur->sp > (1024u * 1024u)) return false;

    *next = *cur;
    next->sp = cfa;

    for (unsigned reg = 0; reg < 31; ++reg) {
        SCfiRule* rule = &st.regs[reg];
        if (rule->type == 1) {
            uintptr_t addr = cfa + (uintptr_t)rule->offset;
            uintptr_t v = 0;
            if (ReadTarget(pid, addr, &v, sizeof(v))) next->regs[reg] = (uint64_t)v;
        } else if (rule->type == 3 && rule->reg < 32) {
            next->regs[reg] = vals[rule->reg];
        } else if (rule->type == 4) {
            next->regs[reg] = (uint64_t)(cfa + (uintptr_t)rule->offset);
        }
    }

    uintptr_t ra = 0;
    if (cie.return_reg < 31) {
        SCfiRule* rr = &st.regs[cie.return_reg];
        if (rr->type == 1) {
            uintptr_t addr = cfa + (uintptr_t)rr->offset;
            ReadTarget(pid, addr, &ra, sizeof(ra));
        } else if (rr->type == 3 && rr->reg < 32) {
            ra = (uintptr_t)vals[rr->reg];
        } else if (rr->type == 4) {
            ra = cfa + (uintptr_t)rr->offset;
        } else if (rr->type != 2) {
            ra = (uintptr_t)next->regs[cie.return_reg];
        }
    }
    if (!ra) ra = (uintptr_t)cur->regs[30];
    uintptr_t caller_pc = RewindLr(ra);
    if (!caller_pc || caller_pc == pc || !IsExecutablePc(caller_pc)) return false;
    next->pc = caller_pc;
    next->regs[30] = (uint64_t)ra;
    return true;
}
#endif

#if defined(__arm__)
static bool ExidxReadWord(pid_t pid, uintptr_t addr, uint32_t* out)
{ return ReadU32(pid, addr, out); }

static bool ExidxAppendWordBytes(unsigned char* insn, size_t* n, size_t cap, uint32_t w)
{
    if (*n + 4 > cap) return false;
    insn[(*n)++] = (unsigned char)((w >> 24) & 0xFF);
    insn[(*n)++] = (unsigned char)((w >> 16) & 0xFF);
    insn[(*n)++] = (unsigned char)((w >> 8) & 0xFF);
    insn[(*n)++] = (unsigned char)(w & 0xFF);
    return true;
}

static bool ExidxPopRegs(pid_t pid, uint32_t* r, uint32_t* vsp, unsigned first, unsigned last, bool include_lr)
{
    for (unsigned reg = first; reg <= last; ++reg) {
        if (reg > 15) return false;
        uint32_t val = 0;
        if (!ReadTarget(pid, *vsp, &val, sizeof(val))) return false;
        r[reg] = val;
        *vsp += 4;
    }
    if (include_lr) {
        uint32_t val = 0;
        if (!ReadTarget(pid, *vsp, &val, sizeof(val))) return false;
        r[14] = val;
        *vsp += 4;
    }
    return true;
}

static bool ExecuteExidx(pid_t pid, unsigned char* insn, size_t count, SRemoteRegs* regs)
{
    uint32_t r[16];
    for (int i = 0; i < 16; ++i) r[i] = regs->uregs[i];
    uint32_t vsp = r[13];
    bool pc_set = false;

    for (size_t i = 0; i < count; ++i) {
        unsigned char b = insn[i];
        if (b == 0x00) {
            vsp += 4;
        } else if ((b & 0xC0) == 0x00) {
            vsp += ((uint32_t)(b & 0x3F) << 2) + 4;
        } else if ((b & 0xC0) == 0x40) {
            vsp -= ((uint32_t)(b & 0x3F) << 2) + 4;
        } else if ((b & 0xF0) == 0x80) {
            if (i + 1 >= count) return false;
            uint16_t mask = (uint16_t)(((b & 0x0F) << 8) | insn[++i]);
            if (mask == 0) return false;
            for (int reg = 4; reg <= 15; ++reg) {
                if (mask & (1u << (reg - 4))) {
                    uint32_t val = 0;
                    if (!ReadTarget(pid, vsp, &val, sizeof(val))) return false;
                    r[reg] = val;
                    vsp += 4;
                    if (reg == 15) pc_set = true;
                }
            }
        } else if ((b & 0xF0) == 0x90) {
            unsigned reg = b & 0x0F;
            if (reg == 13 || reg == 15) return false;
            vsp = r[reg];
        } else if ((b & 0xF0) == 0xA0) {
            unsigned last = 4 + (b & 0x07);
            bool lr = (b & 0x08) != 0;
            if (!ExidxPopRegs(pid, r, &vsp, 4, last, lr)) return false;
        } else if (b == 0xB0) {
            break;
        } else if (b == 0xB1) {
            if (i + 1 >= count) return false;
            unsigned char mask = insn[++i];
            if (mask & 0xF0) return false;
            for (int reg = 0; reg <= 3; ++reg) {
                if (mask & (1u << reg)) {
                    uint32_t val = 0;
                    if (!ReadTarget(pid, vsp, &val, sizeof(val))) return false;
                    r[reg] = val;
                    vsp += 4;
                }
            }
        } else if (b == 0xB2) {
            uint64_t u = 0;
            unsigned shift = 0;
            do {
                if (++i >= count || shift >= 32) return false;
                unsigned char c = insn[i];
                u |= (uint64_t)(c & 0x7F) << shift;
                shift += 7;
                if ((c & 0x80) == 0) break;
            } while (true);
            vsp += 0x204 + ((uint32_t)u << 2);
        } else if (b == 0xB3) {
            if (i + 1 >= count) return false;
            unsigned char x = insn[++i];
            unsigned count_d = (x & 0x0F) + 1;
            vsp += count_d * 8 + 4; // VFP FSTMFDX D[ssss]-D[ssss+cccc]
        } else if ((b & 0xF8) == 0xB8) {
            unsigned count_d = (b & 0x07) + 1;
            vsp += count_d * 8 + 4; // VFP FSTMFDX D[8]-D[8+nnn]
        } else if ((b & 0xF8) == 0xD0) {
            unsigned count_d = (b & 0x07) + 1;
            vsp += count_d * 8; // VFP FSTMFDD D[8]-D[8+nnn]
        } else if (b == 0xC6) {
            if (i + 1 >= count) return false;
            unsigned char x = insn[++i];
            unsigned count_w = (x & 0x0F) + 1;
            vsp += count_w * 8; // WMMX wR pop approximation.
        } else if (b == 0xC7) {
            if (i + 1 >= count) return false;
            unsigned char mask = insn[++i];
            for (unsigned reg = 0; reg < 4; ++reg) if (mask & (1u << reg)) vsp += 4;
        } else if (b >= 0xC0 && b <= 0xC5) {
            if (i + 1 >= count) return false;
            unsigned char x = insn[++i];
            unsigned count_d = (x & 0x0F) + 1;
            vsp += count_d * 8; // VFP/WMMX double-register pop.
        } else if (b == 0xB4 || b == 0xB5 || b == 0xB6 || b == 0xB7 || b == 0xC8 || b == 0xC9 || b == 0xCA || b == 0xCB || b == 0xCC || b == 0xCD || b == 0xCE || b == 0xCF) {
            return false; // Reserved/Intel WMMX forms we cannot safely model.
        } else {
            return false;
        }
    }

    for (int i = 0; i < 16; ++i) regs->uregs[i] = r[i];
    regs->uregs[13] = vsp;
    if (!pc_set) regs->uregs[15] = r[14];
    return regs->uregs[15] != 0;
}

static bool ExidxCollectInstructions(pid_t pid, uintptr_t entry_addr, uint32_t data, unsigned char* insn, size_t* n, size_t cap)
{
    *n = 0;
    if (data == 1) return false; // EXIDX_CANTUNWIND

    if (data & 0x80000000u) {
        unsigned personality = (data >> 24) & 0x0F;
        if (personality > 2) return false;
        if (personality == 0) {
            if (*n + 3 > cap) return false;
            insn[(*n)++] = (unsigned char)((data >> 16) & 0xFF);
            insn[(*n)++] = (unsigned char)((data >> 8) & 0xFF);
            insn[(*n)++] = (unsigned char)(data & 0xFF);
            return true;
        }

        unsigned extra_words = (data >> 16) & 0xFF;
        if (*n + 2 > cap) return false;
        insn[(*n)++] = (unsigned char)((data >> 8) & 0xFF);
        insn[(*n)++] = (unsigned char)(data & 0xFF);
        uintptr_t p = entry_addr + 8;
        for (unsigned i = 0; i < extra_words; ++i) {
            uint32_t w = 0;
            if (!ExidxReadWord(pid, p + i * 4, &w)) return false;
            if (!ExidxAppendWordBytes(insn, n, cap, w)) return false;
        }
        return true;
    }

    uintptr_t extab = (uintptr_t)Prel31ToAddr(entry_addr + 4, data);
    uint32_t w = 0;
    if (!ExidxReadWord(pid, extab, &w)) return false;

    if (w & 0x80000000u) {
        unsigned personality = (w >> 24) & 0x0F;
        if (personality > 2) return false;
        if (personality == 0) {
            if (*n + 3 > cap) return false;
            insn[(*n)++] = (unsigned char)((w >> 16) & 0xFF);
            insn[(*n)++] = (unsigned char)((w >> 8) & 0xFF);
            insn[(*n)++] = (unsigned char)(w & 0xFF);
            return true;
        }

        unsigned extra_words = (w >> 16) & 0xFF;
        if (*n + 2 > cap) return false;
        insn[(*n)++] = (unsigned char)((w >> 8) & 0xFF);
        insn[(*n)++] = (unsigned char)(w & 0xFF);
        uintptr_t p = extab + 4;
        for (unsigned i = 0; i < extra_words; ++i) {
            uint32_t x = 0;
            if (!ExidxReadWord(pid, p + i * 4, &x)) return false;
            if (!ExidxAppendWordBytes(insn, n, cap, x)) return false;
        }
        return true;
    }

    // Generic extab: first word is a prel31 personality routine pointer. The next
    // word carries compact model bits, followed by optional extra instruction words.
    uint32_t hdr = 0;
    if (!ExidxReadWord(pid, extab + 4, &hdr) || !(hdr & 0x80000000u)) return false;
    unsigned personality = (hdr >> 24) & 0x0F;
    if (personality > 2) return false;
    if (personality == 0) {
        if (*n + 3 > cap) return false;
        insn[(*n)++] = (unsigned char)((hdr >> 16) & 0xFF);
        insn[(*n)++] = (unsigned char)((hdr >> 8) & 0xFF);
        insn[(*n)++] = (unsigned char)(hdr & 0xFF);
        return true;
    }
    unsigned extra_words = (hdr >> 16) & 0xFF;
    if (*n + 2 > cap) return false;
    insn[(*n)++] = (unsigned char)((hdr >> 8) & 0xFF);
    insn[(*n)++] = (unsigned char)(hdr & 0xFF);
    for (unsigned i = 0; i < extra_words; ++i) {
        uint32_t x = 0;
        if (!ExidxReadWord(pid, extab + 8 + i * 4, &x)) return false;
        if (!ExidxAppendWordBytes(insn, n, cap, x)) return false;
    }
    return true;
}

static bool ExidxUnwindStepArm(pid_t pid, const SRemoteRegs* cur, SRemoteRegs* next)
{
    uintptr_t pc = StripPc((uintptr_t)cur->uregs[15]);
    SObjectInfo* o = GetObjectForPc(pid, pc);
    if (!o || !o->arm_exidx || o->arm_exidx_size < 8) return false;

    size_t count = (size_t)(o->arm_exidx_size / 8);
    if (count > 262144) count = 262144;
    size_t lo = 0, hi = count;
    uintptr_t best_entry = 0;
    uint32_t best_data = 0;
    bool found = false;

    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        uintptr_t ea = o->arm_exidx + (uintptr_t)mid * 8;
        uint32_t prel = 0, data = 0;
        if (!ReadU32(pid, ea, &prel) || !ReadU32(pid, ea + 4, &data)) return false;
        uintptr_t fn = (uintptr_t)Prel31ToAddr(ea, prel);
        if (fn <= pc) {
            best_entry = ea;
            best_data = data;
            found = true;
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (!found || best_data == 1) return false;

    unsigned char insn[128];
    size_t n = 0;
    if (!ExidxCollectInstructions(pid, best_entry, best_data, insn, &n, sizeof(insn))) return false;

    *next = *cur;
    if (!ExecuteExidx(pid, insn, n, next)) return false;
    next->uregs[15] = (uint32_t)RewindLr(next->uregs[15]);
    return IsExecutablePc((uintptr_t)next->uregs[15]);
}
#endif

static size_t MetadataUnwind(pid_t pid, const SRemoteRegs* regs, uintptr_t* frames, size_t cap)
{
    if (cap == 0) return 0;
    SRemoteRegs cur = *regs;
    size_t n = 0;

    for (;;) {
        uintptr_t pc = 0;
#if defined(__aarch64__)
        pc = StripPc((uintptr_t)cur.pc);
#else
        pc = StripPc((uintptr_t)cur.uregs[15]);
#endif
        if (!AppendUniqueFrame(frames, &n, cap, pc)) break;
        if (n >= cap) break;

        SRemoteRegs next;
        bool ok = false;
#if defined(__aarch64__)
        ok = CfiUnwindStepA64(pid, &cur, &next);
#else
        ok = ExidxUnwindStepArm(pid, &cur, &next);
#endif
        if (!ok) break;
        cur = next;
    }
    return n;
}

static size_t RemoteUnwind(pid_t pid, const SRemoteRegs* regs, uintptr_t* frames, size_t cap)
{
    g_nLastUnwindMethod = 1;
    uintptr_t pc = 0, sp = 0, fp = 0, lr = 0, fp_alt = 0;
    SeedRegs(regs, &pc, &sp, &fp, &lr, &fp_alt);
    size_t n = UnwindOneFp(pid, pc, sp, fp, lr, frames, cap);
#if defined(__arm__)
    if (n < 3 && fp_alt && fp_alt != fp) {
        uintptr_t alt[256];
        size_t m = UnwindOneFp(pid, pc, sp, fp_alt, lr, alt, cap > 256 ? 256 : cap);
        if (m > n) {
            for (size_t i = 0; i < m; ++i) frames[i] = alt[i];
            n = m;
        }
    }
#endif
    if (n < 3) {
        uintptr_t meta[256];
        size_t m = MetadataUnwind(pid, regs, meta, cap > 256 ? 256 : cap);
        if (m > n) {
            for (size_t i = 0; i < m; ++i) frames[i] = meta[i];
            n = m;
#if defined(__aarch64__)
            g_nLastUnwindMethod = 2;
#else
            g_nLastUnwindMethod = 3;
#endif
        }
    }
    if (n < 3) {
        size_t before = n;
        n = StackScanFallback(pid, sp, frames, n, cap);
        if (n > before) g_nLastUnwindMethod = 4;
    }
    return n;
}

static bool IsValidElfHeader(const SElfHeader* eh)
{
    return eh->e_ident[JCRASHER_EI_MAG0] == JCRASHER_ELFMAG0 && eh->e_ident[JCRASHER_EI_MAG1] == JCRASHER_ELFMAG1 &&
                  eh->e_ident[JCRASHER_EI_MAG2] == JCRASHER_ELFMAG2 && eh->e_ident[JCRASHER_EI_MAG3] == JCRASHER_ELFMAG3 &&
#if defined(__aarch64__)
                  eh->e_ident[JCRASHER_EI_CLASS] == JCRASHER_ELFCLASS64 && eh->e_machine == JCRASHER_EM_AARCH64;
#else
                  eh->e_ident[JCRASHER_EI_CLASS] == JCRASHER_ELFCLASS32 && eh->e_machine == JCRASHER_EM_ARM;
#endif
}

static uintptr_t NormalizeDynPtr(const SObjectInfo* o, uintptr_t p)
{
    if (!p) return 0;
    if (p >= o->start && p < o->end) return p;
    return o->load_bias + p;
}

static bool DiscoverObject(pid_t pid, uintptr_t pc, SObjectInfo* out)
{
    int mi = FindMap(pc);
    if (mi < 0) return false;
    const char* path = g_Maps[mi].path;
    if (!path[0]) return false;

    for (size_t j = 0; j < g_nMapCount; ++j) {
        if (!StrEqual(g_Maps[j].path, path)) continue;

        SElfHeader eh;
        if (!ReadTarget(pid, g_Maps[j].start, &eh, sizeof(eh))) continue;
        if (!IsValidElfHeader(&eh)) continue;
        if (eh.e_phnum == 0 || eh.e_phnum > 128 || eh.e_phentsize != sizeof(SElfProgramHeader)) continue;

        SElfProgramHeader ph[128];
        size_t ph_size = (size_t)eh.e_phnum * sizeof(SElfProgramHeader);
        if (!ReadTarget(pid, g_Maps[j].start + eh.e_phoff, ph, ph_size)) continue;

        uintptr_t min_vaddr = ~(uintptr_t)0;
        uintptr_t max_vaddr = 0;
        uintptr_t dyn_vaddr = 0;
        uintptr_t dyn_size = 0;
        uintptr_t load_align = 4096;
        bool has_load = false;
        for (int k = 0; k < eh.e_phnum; ++k) {
            if (ph[k].p_type == JCRASHER_PT_LOAD) {
                has_load = true;
                uintptr_t lo = (uintptr_t)ph[k].p_vaddr;
                uintptr_t hi = (uintptr_t)(ph[k].p_vaddr + ph[k].p_memsz);
                uintptr_t al = (uintptr_t)ph[k].p_align;
                if (al >= 4096 && al <= 65536 && (al & (al - 1)) == 0 && al > load_align) load_align = al;
                if (lo < min_vaddr) min_vaddr = lo;
                if (hi > max_vaddr) max_vaddr = hi;
            } else if (ph[k].p_type == JCRASHER_PT_DYNAMIC) {
                dyn_vaddr = (uintptr_t)ph[k].p_vaddr;
                dyn_size = (uintptr_t)ph[k].p_memsz;
            }
        }
        if (!has_load) continue;
        uintptr_t page = load_align - 1;
        uintptr_t min_page = min_vaddr & ~page;
        uintptr_t load_bias = g_Maps[j].start - min_page;
        uintptr_t obj_start = load_bias + min_vaddr;
        uintptr_t obj_end = load_bias + max_vaddr;
        if (pc < obj_start || pc >= obj_end) continue;

        SObjectInfo o;
        for (size_t z = 0; z < sizeof(o); ++z) ((char*)&o)[z] = 0;
        o.used = true;
        o.load_bias = load_bias;
        o.start = obj_start;
        o.end = obj_end;
        o.syment = sizeof(SElfSymbol);
        CopyString(o.path, sizeof(o.path), path);

        uintptr_t soname_off = 0;
        if (dyn_vaddr && dyn_size) {
            uintptr_t dyn_addr = load_bias + dyn_vaddr;
            size_t max_dyn = dyn_size / sizeof(SElfDynamic);
            if (max_dyn > 512) max_dyn = 512;
            for (size_t di = 0; di < max_dyn; ++di) {
                SElfDynamic d;
                if (!ReadTarget(pid, dyn_addr + di * sizeof(SElfDynamic), &d, sizeof(d))) break;
                if (d.d_tag == JCRASHER_DT_NULL) break;
                uintptr_t val = (uintptr_t)d.d_un.d_ptr;
                switch (d.d_tag) {
                    case JCRASHER_DT_SYMTAB: o.symtab = NormalizeDynPtr(&o, val); break;
                    case JCRASHER_DT_STRTAB: o.strtab = NormalizeDynPtr(&o, val); break;
                    case JCRASHER_DT_SONAME: soname_off = (uintptr_t)d.d_un.d_val; break;
                    case JCRASHER_DT_HASH: o.hash = NormalizeDynPtr(&o, val); break;
                    case JCRASHER_DT_GNU_HASH: o.gnu_hash = NormalizeDynPtr(&o, val); break;
                    case JCRASHER_DT_GNU_EH_FRAME: o.eh_frame_hdr = NormalizeDynPtr(&o, val); break;
                    case JCRASHER_DT_ARM_EXIDX: o.arm_exidx = NormalizeDynPtr(&o, val); break;
                    case JCRASHER_DT_ARM_EXIDXSZ: o.arm_exidx_size = (uintptr_t)d.d_un.d_val; break;
                    case JCRASHER_DT_SYMENT: o.syment = (uint32_t)d.d_un.d_val; break;
                    default: break;
                }
            }
        }

        if (o.strtab && soname_off) {
            (void)ReadStringTarget(pid, o.strtab + soname_off, o.soname, sizeof(o.soname));
            if (o.soname[0] && EndsWith(path, ".apk")) {
                size_t pn = 0;
                o.path[0] = 0;
                AppendString(o.path, sizeof(o.path), &pn, path);
#if defined(__aarch64__)
                AppendString(o.path, sizeof(o.path), &pn, "!/lib/arm64-v8a/");
#else
                AppendString(o.path, sizeof(o.path), &pn, "!/lib/armeabi-v7a/");
#endif
                AppendString(o.path, sizeof(o.path), &pn, o.soname);
            }
        }

        if (o.hash) {
            uint32_t h[2];
            if (ReadTarget(pid, o.hash, h, sizeof(h))) o.nsyms = h[1];
        }
        if (!o.nsyms && o.gnu_hash) {
            uint32_t hdr[4];
            if (ReadTarget(pid, o.gnu_hash, hdr, sizeof(hdr))) {
                uint32_t nbuckets = hdr[0];
                uint32_t symoffset = hdr[1];
                uint32_t bloom_size = hdr[2];
                uintptr_t buckets = o.gnu_hash + 16 + (uintptr_t)bloom_size * sizeof(uintptr_t);
                uint32_t maxsym = symoffset;
                if (nbuckets > 65536) nbuckets = 65536;
                for (uint32_t bi = 0; bi < nbuckets; ++bi) {
                    uint32_t b = 0;
                    if (!ReadU32(pid, buckets + bi * 4, &b)) break;
                    if (!b) continue;
                    if (b > maxsym) maxsym = b;
                    uintptr_t chains = buckets + (uintptr_t)nbuckets * 4;
                    if (b < symoffset) continue;
                    for (uint32_t ci = b - symoffset; ci < 262144; ++ci) {
                        uint32_t chain = 0;
                        if (!ReadU32(pid, chains + ci * 4, &chain)) break;
                        uint32_t idx = symoffset + ci;
                        if (idx > maxsym) maxsym = idx;
                        if (chain & 1) break;
                    }
                }
                o.nsyms = maxsym + 1;
            }
        }
        if (o.nsyms > 262144) o.nsyms = 262144;
        if (o.syment != sizeof(SElfSymbol)) o.syment = sizeof(SElfSymbol);
        *out = o;
        return true;
    }
    return false;
}

static SObjectInfo* GetObjectForPc(pid_t pid, uintptr_t pc)
{
    for (size_t i = 0; i < g_nObjectCount; ++i) {
        if (g_Objects[i].used && pc >= g_Objects[i].start && pc < g_Objects[i].end) return &g_Objects[i];
    }
    if (g_nObjectCount >= sizeof(g_Objects) / sizeof(g_Objects[0])) return 0;
    SObjectInfo o;
    if (!DiscoverObject(pid, pc, &o)) return 0;
    g_Objects[g_nObjectCount] = o;
    return &g_Objects[g_nObjectCount++];
}

static bool ResolveSymbol(pid_t pid, const SObjectInfo* o, uintptr_t pc, char* name, size_t name_cap,
                                                      uintptr_t* sym_off)
{
    if (!o || !o->symtab || !o->strtab || !o->nsyms) return false;
    uintptr_t best = 0;
    uint32_t best_name = 0;
    uintptr_t best_off = ~(uintptr_t)0;

    for (uint32_t i = 0; i < o->nsyms; ++i) {
        SElfSymbol s;
        if (!ReadTarget(pid, o->symtab + (uintptr_t)i * o->syment, &s, sizeof(s))) break;
        if (!s.st_name || !s.st_value || s.st_shndx == JCRASHER_SHN_UNDEF) continue;
        unsigned type = GetElfSymbolType(s.st_info);
        if (type != JCRASHER_STT_FUNC && type != JCRASHER_STT_NOTYPE) continue;

        uintptr_t addr = (uintptr_t)s.st_value;
        if (!(addr >= o->start && addr < o->end)) addr = o->load_bias + addr;
        if (pc < addr) continue;
        uintptr_t off = pc - addr;
        if (s.st_size && off >= (uintptr_t)s.st_size) continue;
        if (off < best_off) {
            best = addr;
            best_name = s.st_name;
            best_off = off;
            if (off == 0) break;
        }
    }

    if (!best || !best_name) return false;
    if (best_off > 0x100000 && best_off != 0) return false;
    if (!ReadStringTarget(pid, o->strtab + best_name, name, name_cap)) return false;
    *sym_off = pc - best;
    return name[0] != 0;
}

static void WriteFrameLine(int fd, size_t idx, uintptr_t pc, unsigned flags)
{
    char line[1024];
    size_t n = 0;
    const int w = (int)(sizeof(uintptr_t) * 2);

    int mi = FindMap(pc);
    SObjectInfo* o = 0;
    uintptr_t base = 0;
    uintptr_t rel = pc;
    const char* path = "<unknown>";
    bool have_base = false;

    if (mi >= 0) {
        SMapRegion* m = &g_Maps[mi];
        base = m->start - m->offset;
        rel = pc - base;
        path = m->path[0] ? m->path : "<anonymous>";
        have_base = (base != 0 && m->path[0] != 0);

        o = GetObjectForPc(g_nTargetPid, pc);
        if (o) {
            base = o->load_bias;
            rel = pc - o->load_bias;
            path = o->path[0] ? o->path : path;
            have_base = (base != 0);
        }
    }

    AppendChar(line, sizeof(line), &n, '#');
    if (idx < 10) AppendChar(line, sizeof(line), &n, '0');
    AppendDec(line, sizeof(line), &n, (uint64_t)idx);

    // Tombstone-like: pc is module-relative offset when the module is known.
    // Keep 0x prefix and fixed pointer width; hex digits are uppercase.
    AppendString(line, sizeof(line), &n, " pc ");
    AppendHex(line, sizeof(line), &n, (uint64_t)rel, w, true);

    if (have_base) {
        AppendString(line, sizeof(line), &n, " base ");
        AppendHex(line, sizeof(line), &n, (uint64_t)base, w, true);
    }

    AppendChar(line, sizeof(line), &n, ' ');
    AppendString(line, sizeof(line), &n, path);

    if ((flags & DUMP_SYMBOLS) && o) {
        char sym[256];
        uintptr_t so = 0;
        if (ResolveSymbol(g_nTargetPid, o, pc, sym, sizeof(sym), &so)) {
            AppendString(line, sizeof(line), &n, " (");
            AppendString(line, sizeof(line), &n, sym);
            AppendChar(line, sizeof(line), &n, '+');
            AppendHexVar(line, sizeof(line), &n, (uint64_t)so, true);
            AppendChar(line, sizeof(line), &n, ')');
        }
    }

    AppendChar(line, sizeof(line), &n, '\n');
    WriteAll(fd, line, n);
}

static void WriteThreadName(int fd, pid_t pid, pid_t tid)
{
    char path[96];
    BuildTidPath(path, sizeof(path), "/proc/", pid, "/task/", tid, "/comm");
    char comm[80];
    CopyString(comm, sizeof(comm), "unknown");
    int cfd = (int)SysOpenAt(path, O_RDONLY | O_CLOEXEC);
    if (cfd >= 0) {
        long r = SysRead(cfd, comm, sizeof(comm) - 1);
        if (r > 0) {
            comm[r] = 0;
            for (long i = 0; i < r; ++i) {
                if (comm[i] == '\n' || comm[i] == '\r') { comm[i] = 0; break; }
            }
        }
        SysClose(cfd);
    }

    char line[192];
    size_t n = 0;
    AppendString(line, sizeof(line), &n, "\n--- tid ");
    AppendDec(line, sizeof(line), &n, (uint64_t)tid);
    AppendString(line, sizeof(line), &n, " (");
    AppendString(line, sizeof(line), &n, comm);
    AppendString(line, sizeof(line), &n, ") ---\n");
    WriteAll(fd, line, n);
}

static void WriteRegs(int fd, const SRemoteRegs* r)
{
    char line[96];
    const int w = (int)(sizeof(uintptr_t) * 2);
#if defined(__aarch64__)
    for (int i = 0; i <= 30; ++i) {
        size_t n = 0;
        AppendString(line, sizeof(line), &n, "  x");
        AppendDec(line, sizeof(line), &n, (uint64_t)i);
        if (i < 10) AppendChar(line, sizeof(line), &n, ' ');
        AppendChar(line, sizeof(line), &n, ' ');
        AppendHex(line, sizeof(line), &n, r->regs[i], w, true);
        AppendChar(line, sizeof(line), &n, '\n');
        WriteAll(fd, line, n);
    }
    size_t n = 0;
    AppendString(line, sizeof(line), &n, "  sp  "); AppendHex(line, sizeof(line), &n, r->sp, w, true); AppendChar(line, sizeof(line), &n, '\n'); WriteAll(fd, line, n);
    n = 0;
    AppendString(line, sizeof(line), &n, "  pc  "); AppendHex(line, sizeof(line), &n, r->pc, w, true); AppendChar(line, sizeof(line), &n, '\n'); WriteAll(fd, line, n);
#else
    for (int i = 0; i <= 15; ++i) {
        size_t n = 0;
        AppendString(line, sizeof(line), &n, "  r");
        AppendDec(line, sizeof(line), &n, (uint64_t)i);
        if (i < 10) AppendChar(line, sizeof(line), &n, ' ');
        AppendChar(line, sizeof(line), &n, ' ');
        AppendHex(line, sizeof(line), &n, r->uregs[i], w, true);
        AppendChar(line, sizeof(line), &n, '\n');
        WriteAll(fd, line, n);
    }
#endif
}


static void WriteShortUnwindDiagnostics(int fd, pid_t pid, pid_t tid, bool attached,
    const SRemoteRegs* regs, size_t frame_count)
{
    uintptr_t pc = 0, sp = 0, fp = 0, lr = 0, fp_alt = 0;
    SeedRegs(regs, &pc, &sp, &fp, &lr, &fp_alt);

    int stack_i = FindMap(sp);
    int fp_i = FindMap(fp);

    char line[512];
    size_t n = 0;
    AppendString(line, sizeof(line), &n, "<JCrasher: short unwind; frames=");
    AppendDec(line, sizeof(line), &n, (uint64_t)frame_count);
    AppendString(line, sizeof(line), &n, " attached=");
    AppendDec(line, sizeof(line), &n, attached ? 1 : 0);
    AppendString(line, sizeof(line), &n, " pid=");
    AppendDec(line, sizeof(line), &n, (uint64_t)pid);
    AppendString(line, sizeof(line), &n, " tid=");
    AppendDec(line, sizeof(line), &n, (uint64_t)tid);
    AppendString(line, sizeof(line), &n, " method=");
    AppendDec(line, sizeof(line), &n, (uint64_t)g_nLastUnwindMethod);
    AppendString(line, sizeof(line), &n, ">\n");
    WriteAll(fd, line, n);

    n = 0;
    AppendString(line, sizeof(line), &n, "<JCrasher: pc=");
    AppendHex(line, sizeof(line), &n, (uint64_t)pc, (int)(sizeof(uintptr_t) * 2), true);
    AppendString(line, sizeof(line), &n, " lr=");
    AppendHex(line, sizeof(line), &n, (uint64_t)lr, (int)(sizeof(uintptr_t) * 2), true);
    AppendString(line, sizeof(line), &n, " sp=");
    AppendHex(line, sizeof(line), &n, (uint64_t)sp, (int)(sizeof(uintptr_t) * 2), true);
    AppendString(line, sizeof(line), &n, " fp=");
    AppendHex(line, sizeof(line), &n, (uint64_t)fp, (int)(sizeof(uintptr_t) * 2), true);
    AppendString(line, sizeof(line), &n, ">\n");
    WriteAll(fd, line, n);

    n = 0;
    AppendString(line, sizeof(line), &n, "<JCrasher: stack_map=");
    AppendDec(line, sizeof(line), &n, stack_i >= 0 ? 1 : 0);
    AppendString(line, sizeof(line), &n, " fp_map=");
    AppendDec(line, sizeof(line), &n, fp_i >= 0 ? 1 : 0);
    if (stack_i >= 0) {
        AppendString(line, sizeof(line), &n, " stack=[");
        AppendHex(line, sizeof(line), &n, (uint64_t)g_Maps[stack_i].start, (int)(sizeof(uintptr_t) * 2), true);
        AppendString(line, sizeof(line), &n, "-");
        AppendHex(line, sizeof(line), &n, (uint64_t)g_Maps[stack_i].end, (int)(sizeof(uintptr_t) * 2), true);
        AppendString(line, sizeof(line), &n, "]");
    }
    AppendString(line, sizeof(line), &n, ">\n");
    WriteAll(fd, line, n);
}

static void DumpThread(int fd, pid_t pid, pid_t tid, bool attached, unsigned flags, size_t max_frames)
{
    SRemoteRegs regs;
    bool have = false;
    if (g_bHaveCrash && tid == g_nCrashTid) {
        regs = g_CrashRegs;
        have = true;
    } else if (attached && GetRegs(tid, &regs)) {
        have = true;
    }

    if (flags & DUMP_THREAD_HEADER) WriteThreadName(fd, pid, tid);
    if (!have) {
        WriteString(fd, "  <registers unavailable>\n");
        return;
    }
    if (flags & DUMP_REGISTERS) WriteRegs(fd, &regs);

    uintptr_t frames[256];
    if (max_frames == 0 || max_frames > 256) max_frames = 256;

    pid_t old_peek = g_nPeekTid;
    g_nPeekTid = attached ? tid : 0;
    size_t n = RemoteUnwind(pid, &regs, frames, max_frames);
    g_nPeekTid = old_peek;

    if ((flags & DUMP_DIAGNOSTICS) && n <= 2) {
        WriteShortUnwindDiagnostics(fd, pid, tid, attached, &regs, n);
    }

    for (size_t i = 0; i < n; ++i) WriteFrameLine(fd, i, frames[i], flags);
}

static size_t EnumerateTids(pid_t pid, pid_t* tids, size_t cap)
{
    char path[64];
    BuildPidPath(path, sizeof(path), "/proc/", pid, "/task");
    int fd = (int)SysOpenAt(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return 0;

    size_t count = 0;
    char buf[8192];
    for (;;) {
        long nread = syscall(SYS_getdents64, fd, buf, sizeof(buf));
        if (nread <= 0) break;
        for (long bpos = 0; bpos < nread;) {
            SLinuxDirent64* d = (SLinuxDirent64*)(buf + bpos);
            pid_t tid = 0;
            if (ParseDec(d->d_name, &tid) && count < cap) tids[count++] = tid;
            if (d->d_reclen == 0) break;
            bpos += d->d_reclen;
        }
    }
    SysClose(fd);
    return count;
}

static void DumpCurrentThreadImpl(int fd, pid_t target, unsigned flags, size_t max_frames)
{
    if (g_nCrashTid <= 0) g_nCrashTid = SysGetTid();
    DumpThread(fd, target, g_nCrashTid, false, flags, max_frames);
}

static bool AttachThread(pid_t tid)
{
    if (tid <= 0) return false;

    if (SysPtrace(PTRACE_SEIZE, tid, 0, 0) == 0) {
        SysPtrace(PTRACE_INTERRUPT, tid, 0, 0);
        return true;
    }

    // Some OEM kernels reject SEIZE but still allow classic ATTACH.
    if (SysPtrace(PTRACE_ATTACH, tid, 0, 0) == 0) return true;
    return false;
}

static void DumpAllThreadsImpl(int fd, pid_t target, unsigned flags, size_t max_frames)
{
    pid_t tids[1024];
    bool attached[1024];
    for (size_t i = 0; i < 1024; ++i) attached[i] = false;
    size_t count = EnumerateTids(target, tids, 1024);
    if (count == 0 && g_nCrashTid > 0) {
        tids[count++] = g_nCrashTid;
    }

    for (size_t i = 0; i < count; ++i) {
        // Attach the crashed thread too. Registers still come from ucontext,
        // but ptrace attachment gives the dumper a PTRACE_PEEKDATA fallback
        // for stack reads when process_vm_readv() is blocked by the ROM.
        if (AttachThread(tids[i])) attached[i] = true;
    }
    for (size_t i = 0; i < count; ++i) {
        if (attached[i]) {
            int st = 0;
            SysWait4(tids[i], &st, __WALL);
        }
    }

    if ((flags & DUMP_CRASHED_FIRST) && g_nCrashTid > 0) {
        DumpThread(fd, target, g_nCrashTid, false, flags, max_frames);
    }

    for (size_t i = 0; i < count; ++i) {
        if ((flags & DUMP_CRASHED_FIRST) && tids[i] == g_nCrashTid) continue;
        DumpThread(fd, target, tids[i], attached[i], flags, max_frames);
    }

    for (size_t i = 0; i < count; ++i) {
        if (attached[i]) SysPtrace(PTRACE_DETACH, tids[i], 0, 0);
    }
}

static int ChildMain(int fd, pid_t target, unsigned flags, size_t max_frames)
{
    g_nTargetPid = target;
    if (!LoadMapsFor(target)) WriteString(fd, "<failed to read maps>\n");

    if (flags & DUMP_ALL_THREADS) {
        DumpAllThreadsImpl(fd, target, flags, max_frames);
    } else {
        DumpCurrentThreadImpl(fd, target, flags, max_frames);
    }
    return 0;
}

static int RawPipe2(int fds[2])
{
#ifdef SYS_pipe2
    long r = syscall(SYS_pipe2, fds, O_CLOEXEC);
    if (r == 0) return 0;
#endif
#ifdef SYS_pipe
    return (int)syscall(SYS_pipe, fds);
#else
    return -1;
#endif
}

static int RawSocketPair(int fds[2])
{
#ifdef SYS_socketpair
    return (int)syscall(SYS_socketpair, AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, fds);
#else
    return socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, fds);
#endif
}

static int SendFd(int sock, int fd)
{
    if (sock < 0 || fd < 0) return -1;

    char ch = 'F';
    struct iovec iov;
    iov.iov_base = &ch;
    iov.iov_len = 1;

    union {
        struct cmsghdr hdr;
        char buf[CMSG_SPACE(sizeof(int))];
    } control;

    struct msghdr msg;
    for (size_t i = 0; i < sizeof(msg); ++i) ((char*)&msg)[i] = 0;
    for (size_t i = 0; i < sizeof(control); ++i) control.buf[i] = 0;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control.buf;
    msg.msg_controllen = sizeof(control.buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    *((int*)CMSG_DATA(cmsg)) = fd;

#ifdef SYS_sendmsg
    return syscall(SYS_sendmsg, sock, &msg, MSG_NOSIGNAL) == 1 ? 0 : -1;
#else
    return sendmsg(sock, &msg, MSG_NOSIGNAL) == 1 ? 0 : -1;
#endif
}

static int RecvFd(int sock)
{
    if (sock < 0) return -1;

    char ch = 0;
    struct iovec iov;
    iov.iov_base = &ch;
    iov.iov_len = 1;

    union {
        struct cmsghdr hdr;
        char buf[CMSG_SPACE(sizeof(int))];
    } control;

    struct msghdr msg;
    for (size_t i = 0; i < sizeof(msg); ++i) ((char*)&msg)[i] = 0;
    for (size_t i = 0; i < sizeof(control); ++i) control.buf[i] = 0;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control.buf;
    msg.msg_controllen = sizeof(control.buf);

#ifdef SYS_recvmsg
    long r = syscall(SYS_recvmsg, sock, &msg, 0);
#else
    long r = recvmsg(sock, &msg, 0);
#endif
    if (r != 1) return -1;

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) return -1;
    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) return -1;
    return *((int*)CMSG_DATA(cmsg));
}

static pid_t RawCloneForkLike()
{
    // fork-like clone. No libc fork(), no pthread_atfork handlers, no malloc.
#if defined(__aarch64__)
    return (pid_t)syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0);
#else
    return (pid_t)syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0);
#endif
}

static void PreinitChildMain()
{
    if (!g_pPreinitShared) syscall(SYS_exit, 1);

    while (g_pPreinitShared->state == 0) {
        SysFutex(&g_pPreinitShared->state, FUTEX_WAIT, 0);
    }

    if (g_pPreinitShared->state != 1) syscall(SYS_exit, 1);

    SPreinitRequest* r = &g_pPreinitShared->request;
    if (r->magic != JCRASHER_PREINIT_MAGIC) {
        g_pPreinitShared->result = DUMP_CHILD_FAILED;
        syscall(SYS_exit, 1);
    }

    g_nTargetPid = r->target;
    g_nCrashTid = r->crash_tid;
    g_CrashRegs = r->regs;
    g_bHaveCrash = true;

    int fd = r->fd;
    bool close_fd = false;
    if (r->use_path) {
        fd = (int)SysOpenAt(r->path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
        close_fd = true;
    } else if (r->use_passed_fd) {
        fd = RecvFd(g_nPreinitSockChild);
        close_fd = true;
    }

    if (fd < 0) {
        g_pPreinitShared->result = r->use_path ? DUMP_PATH_FAILED : DUMP_BAD_FD;
        syscall(SYS_exit, 1);
    }

    unsigned forced = r->flags | DUMP_ALL_THREADS | DUMP_CRASHED_FIRST |
        DUMP_THREAD_HEADER | DUMP_SYMBOLS;

    g_pPreinitShared->result = DUMP_CHILD_FAILED;
    ChildMain(fd, r->target, forced, r->max_frames ? r->max_frames : 64);

    if (close_fd) SysClose(fd);
    g_pPreinitShared->result = DUMP_OK;
    syscall(SYS_exit, 0);
}

static int InitPreinitDumper(int fd, const char* path)
{
    if (g_nPreinitPid > 0 && g_pPreinitShared) return DUMP_OK;

    if (path && path[0]) {
        CopyString(g_szPreinitPath, sizeof(g_szPreinitPath), path);
        g_nPreinitFd = -1;
    } else if (fd >= 0) {
        g_nPreinitFd = fd;
        g_szPreinitPath[0] = 0;
    } else {
        g_nPreinitFd = -1;
        g_szPreinitPath[0] = 0;
    }

    int socks[2] = {-1, -1};
    if (RawSocketPair(socks) != 0) return DUMP_PIPE_FAILED;
    g_nPreinitSockParent = socks[0];
    g_nPreinitSockChild = socks[1];

    void* mem = SysMmapShared(sizeof(SPreinitShared));
    if (mem == (void*)-1 || !mem) {
        SysClose(g_nPreinitSockParent);
        SysClose(g_nPreinitSockChild);
        g_nPreinitSockParent = -1;
        g_nPreinitSockChild = -1;
        return DUMP_MMAP_FAILED;
    }

    g_pPreinitShared = (SPreinitShared*)mem;
    g_pPreinitShared->state = 0;
    g_pPreinitShared->result = DUMP_HELPER_NOT_INIT;

    pid_t child = RawCloneForkLike();
    if (child < 0) {
        SysClose(g_nPreinitSockParent);
        SysClose(g_nPreinitSockChild);
        g_nPreinitSockParent = -1;
        g_nPreinitSockChild = -1;
        g_pPreinitShared = 0;
        return DUMP_CLONE_FAILED;
    }

    if (child == 0) {
        SysClose(g_nPreinitSockParent);
        g_nPreinitSockParent = -1;
        PreinitChildMain();
        syscall(SYS_exit, 1);
        __builtin_unreachable();
    }

    SysClose(g_nPreinitSockChild);
    g_nPreinitSockChild = -1;
    g_nPreinitPid = child;
    SysPrctl(PR_SET_PTRACER, child);
    return DUMP_OK;
}

static int DumpWithPreinitDumper(void* crashed_context, unsigned flags, size_t max_frames, int out_fd, const char* out_path)
{
    if (!g_pPreinitShared || g_nPreinitPid <= 0) return DUMP_HELPER_NOT_INIT;
    if (g_pPreinitShared->state != 0) return DUMP_HELPER_BUSY;

    CaptureCrash(crashed_context, SysGetTid());

    SPreinitRequest* r = &g_pPreinitShared->request;
    r->magic = JCRASHER_PREINIT_MAGIC;
    r->target = SysGetPid();
    r->crash_tid = g_nCrashTid;
    r->flags = flags | DUMP_ALL_THREADS | DUMP_CRASHED_FIRST |
        DUMP_THREAD_HEADER | DUMP_SYMBOLS;
    r->max_frames = max_frames ? max_frames : 64;
    r->regs = g_CrashRegs;

    r->use_path = 0;
    r->use_passed_fd = 0;
    r->fd = -1;
    r->path[0] = 0;

    if (out_path && out_path[0]) {
        r->use_path = 1;
        CopyString(r->path, sizeof(r->path), out_path);
    } else if (out_fd >= 0) {
        r->use_passed_fd = 1;
    } else if (g_szPreinitPath[0]) {
        r->use_path = 1;
        CopyString(r->path, sizeof(r->path), g_szPreinitPath);
    } else if (g_nPreinitFd >= 0) {
        r->fd = g_nPreinitFd;
    } else {
        return DUMP_BAD_FD;
    }

    g_pPreinitShared->result = DUMP_CHILD_FAILED;
    __sync_synchronize();

    if (r->use_passed_fd && SendFd(g_nPreinitSockParent, out_fd) != 0) {
        return DUMP_FD_SEND_FAILED;
    }

    g_pPreinitShared->state = 1;
    SysFutex(&g_pPreinitShared->state, FUTEX_WAKE, 1);

    int st = 0;
    long wr = SysWait4(g_nPreinitPid, &st, 0);
    SysPrctl(PR_SET_PTRACER, 0);
    if (wr != g_nPreinitPid) return DUMP_CHILD_FAILED;
    if (st != 0 && g_pPreinitShared->result == DUMP_CHILD_FAILED) return DUMP_CHILD_FAILED;
    return g_pPreinitShared->result;
}

} // namespace Private

inline int DumpFd(int fd, void* crashed_context, unsigned flags, size_t max_frames, bool fallback_to_current);

inline int InitAllThreadsDumper()
{
    return Private::InitPreinitDumper(-1, 0);
}

inline int InitAllThreadsDumperFd(int fd)
{
    return Private::InitPreinitDumper(fd, 0);
}

inline int InitAllThreadsDumperPath(const char* path)
{
    if (!path || !path[0]) return DUMP_PATH_FAILED;
    return Private::InitPreinitDumper(-1, path);
}

inline bool HasAllThreadsDumper()
{
    return Private::g_pPreinitShared && Private::g_nPreinitPid > 0;
}

inline bool HasAllThreadsDumperFd(int fd)
{
    return HasAllThreadsDumper() && fd >= 0;
}

inline bool HasAllThreadsDumperPath(const char* path)
{
    return HasAllThreadsDumper() && path && path[0];
}

inline int DumpAllThreadsPreinit(void* crashed_context, unsigned flags,
    size_t max_frames)
{
    int r = Private::DumpWithPreinitDumper(crashed_context, flags, max_frames, -1, 0);
    if (r == DUMP_OK) return r;

    if (Private::g_nPreinitFd >= 0) {
        DumpFd(Private::g_nPreinitFd, crashed_context, flags & ~DUMP_ALL_THREADS,
            max_frames, true);
    } else if (Private::g_szPreinitPath[0]) {
        int fd = (int)Private::SysOpenAt(Private::g_szPreinitPath,
            O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
        if (fd >= 0) {
            DumpFd(fd, crashed_context, flags & ~DUMP_ALL_THREADS, max_frames, true);
            Private::SysClose(fd);
        }
    }
    return r;
}

inline int DumpFd(int fd, void* crashed_context, unsigned flags, size_t max_frames, bool fallback_to_current = true)
{
    if (fd < 0) return DUMP_BAD_FD;

    if (fallback_to_current && (flags & DUMP_ALL_THREADS) && HasAllThreadsDumperFd(fd)) {
        int r = Private::DumpWithPreinitDumper(crashed_context, flags, max_frames, fd, 0);
        if (r == DUMP_OK) return r;

        Private::WriteString(fd, "<JCrasher: preinit helper failed; fallback to current thread>\n");
        return DumpFd(fd, crashed_context, flags & ~DUMP_ALL_THREADS, max_frames, true);
    }

    Private::CaptureCrash(crashed_context, Private::SysGetTid());

    pid_t target = Private::SysGetPid();
    unsigned forced = flags | DUMP_THREAD_HEADER | DUMP_SYMBOLS;
    if (forced & DUMP_ALL_THREADS) forced |= DUMP_CRASHED_FIRST;

    // Current-thread mode must not depend on clone/ptrace/process_vm_readv.
    // Some OEM ROMs block or harden that path, which produced empty logs.
    // This direct path uses only the provided ucontext, raw /proc/self/maps
    // reads, validated local memory reads, and write().
    if (!(forced & DUMP_ALL_THREADS)) {
        Private::g_nTargetPid = target;
        if (!Private::LoadMapsFor(target)) Private::WriteString(fd, "<JCrasher: failed to read /proc/self/maps>\n");
        Private::DumpCurrentThreadImpl(fd, target, forced, max_frames ? max_frames : 64);
        return 0;
    }

    int sync_pipe[2] = {-1, -1};
    if (Private::RawPipe2(sync_pipe) != 0) {
        Private::WriteString(fd, fallback_to_current ?
            "<JCrasher: pipe failed; fallback to current thread>\n" :
            "<JCrasher: pipe failed>\n");
        if (fallback_to_current) {
            forced &= ~DUMP_ALL_THREADS;
            Private::g_nTargetPid = target;
            if (!Private::LoadMapsFor(target)) Private::WriteString(fd, "<JCrasher: failed to read /proc/self/maps>\n");
            Private::DumpCurrentThreadImpl(fd, target, forced, max_frames ? max_frames : 64);
        }
        return DUMP_PIPE_FAILED;
    }

    pid_t child = Private::RawCloneForkLike();
    if (child < 0) {
        Private::SysClose(sync_pipe[0]);
        Private::SysClose(sync_pipe[1]);
        Private::WriteString(fd, fallback_to_current ?
            "<JCrasher: clone failed; fallback to current thread>\n" :
            "<JCrasher: clone failed>\n");
        if (fallback_to_current) {
            forced &= ~DUMP_ALL_THREADS;
            Private::g_nTargetPid = target;
            if (!Private::LoadMapsFor(target)) Private::WriteString(fd, "<JCrasher: failed to read /proc/self/maps>\n");
            Private::DumpCurrentThreadImpl(fd, target, forced, max_frames ? max_frames : 64);
        }
        return DUMP_CLONE_FAILED;
    }

    if (child == 0) {
        Private::SysClose(sync_pipe[1]);
        char c = 0;
        (void)Private::SysRead(sync_pipe[0], &c, 1);
        Private::SysClose(sync_pipe[0]);
        Private::ChildMain(fd, target, forced, max_frames ? max_frames : 64);
        syscall(SYS_exit, 0);
        __builtin_unreachable();
    }

    Private::SysClose(sync_pipe[0]);
    Private::SysPrctl(PR_SET_PTRACER, child);
    char c = 'x';
    (void)Private::SysWrite(sync_pipe[1], &c, 1);
    Private::SysClose(sync_pipe[1]);
    int st = 0;
    Private::SysWait4(child, &st, 0);
    Private::SysPrctl(PR_SET_PTRACER, 0);

    // If the dumper child was killed by OEM policy/seccomp before writing, do
    // not leave an empty file. Fall back to the current thread only.
    if (st != 0) {
        Private::WriteString(fd, fallback_to_current ?
            "<JCrasher: dumper child failed; fallback to current thread>\n" :
            "<JCrasher: dumper child failed>\n");
        if (fallback_to_current) {
            forced &= ~DUMP_ALL_THREADS;
            Private::g_nTargetPid = target;
            if (!Private::LoadMapsFor(target)) Private::WriteString(fd, "<JCrasher: failed to read /proc/self/maps>\n");
            Private::DumpCurrentThreadImpl(fd, target, forced, max_frames ? max_frames : 64);
        }
        return DUMP_CHILD_FAILED;
    }

    return 0;
}

inline int DumpPath(const char* path, void* crashed_context, unsigned flags,
    size_t max_frames)
{
    if (!path || !path[0]) return DUMP_PATH_FAILED;

    if ((flags & DUMP_ALL_THREADS) && HasAllThreadsDumperPath(path)) {
        int r = Private::DumpWithPreinitDumper(crashed_context, flags, max_frames, -1, path);
        if (r == DUMP_OK) return r;

        int fd = (int)Private::SysOpenAt(path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
        if (fd >= 0) {
            Private::WriteString(fd, "<JCrasher: preinit helper failed; fallback to current thread>\n");
            DumpFd(fd, crashed_context, flags & ~DUMP_ALL_THREADS, max_frames, true);
            Private::SysClose(fd);
        }
        return r;
    }

    int fd = (int)Private::SysOpenAt(path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0) return DUMP_PATH_FAILED;
    int r = DumpFd(fd, crashed_context, flags, max_frames, true);
    Private::SysClose(fd);
    return r;
}

inline int DumpCurrentThreadFd(int fd, void* crashed_context, unsigned flags,
    size_t max_frames)
{
    return DumpFd(fd, crashed_context, flags & ~DUMP_ALL_THREADS, max_frames);
}

inline int DumpAllThreadsFd(int fd, void* crashed_context, unsigned flags,
    size_t max_frames)
{
    return DumpFd(fd, crashed_context, flags | DUMP_ALL_THREADS | DUMP_CRASHED_FIRST,
        max_frames);
}


inline int TryDumpAllThreadsFd(int fd, void* crashed_context, unsigned flags,
    size_t max_frames)
{
    return DumpFd(fd, crashed_context, flags | DUMP_ALL_THREADS | DUMP_CRASHED_FIRST,
        max_frames, false);
}

inline int DumpCurrentThreadPath(const char* path, void* crashed_context, unsigned flags,
    size_t max_frames)
{
    return DumpPath(path, crashed_context, flags & ~DUMP_ALL_THREADS, max_frames);
}

inline int DumpAllThreadsPath(const char* path, void* crashed_context, unsigned flags,
    size_t max_frames)
{
    return DumpPath(path, crashed_context, flags | DUMP_ALL_THREADS | DUMP_CRASHED_FIRST,
        max_frames);
}


} // namespace JCrasher

#else // unsupported arch

namespace JCrasher
{

enum eDumpFlags
{
    DUMP_THREAD_HEADER = (1u << 0),
    DUMP_REGISTERS     = (1u << 1),
    DUMP_SYMBOLS       = (1u << 2),
    DUMP_CRASHED_FIRST = (1u << 3),
    DUMP_ALL_THREADS   = (1u << 4),
    DUMP_DIAGNOSTICS   = (1u << 5)
};

enum eDumpResult
{
    DUMP_OK               =  0,
    DUMP_BAD_FD           = -1,
    DUMP_PIPE_FAILED      = -2,
    DUMP_CLONE_FAILED     = -3,
    DUMP_CHILD_FAILED     = -4,
    DUMP_PATH_FAILED      = -5,
    DUMP_HELPER_NOT_INIT  = -6,
    DUMP_HELPER_BUSY      = -7,
    DUMP_MMAP_FAILED      = -8,
    DUMP_FD_SEND_FAILED   = -9
};

inline int DumpFd(int, void*, unsigned, size_t)
{ return DUMP_BAD_FD; }
inline int DumpPath(const char*, void*, unsigned, size_t)
{ return DUMP_PATH_FAILED; }
inline int DumpCurrentThreadFd(int, void*, unsigned, size_t)
{ return DUMP_BAD_FD; }
inline int DumpAllThreadsFd(int, void*, unsigned, size_t)
{ return DUMP_BAD_FD; }
inline int TryDumpAllThreadsFd(int, void*, unsigned, size_t)
{ return DUMP_BAD_FD; }
inline int DumpCurrentThreadPath(const char*, void*, unsigned, size_t)
{ return DUMP_PATH_FAILED; }
inline int DumpAllThreadsPath(const char*, void*, unsigned, size_t)
{ return DUMP_PATH_FAILED; }
inline int InitAllThreadsDumper()
{ return DUMP_CLONE_FAILED; }
inline int InitAllThreadsDumperFd(int)
{ return DUMP_BAD_FD; }
inline int InitAllThreadsDumperPath(const char*)
{ return DUMP_PATH_FAILED; }
inline bool HasAllThreadsDumper()
{ return false; }
inline bool HasAllThreadsDumperFd(int)
{ return false; }
inline bool HasAllThreadsDumperPath(const char*)
{ return false; }
inline int DumpAllThreadsPreinit(void*, unsigned, size_t)
{ return DUMP_HELPER_NOT_INIT; }

} // namespace JCrasher

#endif // arch

#endif // __JCRASHER_H