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
#include <unistd.h>
#include <ucontext.h>


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
    #define PR_SET_PTRACER 0x59616d61
#endif
#ifndef NT_PRSTATUS
    #define NT_PRSTATUS 1
#endif
#ifndef __WALL
    #define __WALL 0x40000000
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
    DUMP_ALL_THREADS   = (1u << 4)
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

static bool ReadTarget(pid_t pid, uintptr_t addr, void* dst, size_t len)
{
    if (!addr || !dst || !len) return false;
    struct iovec local;
    local.iov_base = dst;
    local.iov_len = len;
    struct iovec remote;
    remote.iov_base = (void*)addr;
    remote.iov_len = len;
    long r = syscall(SYS_process_vm_readv, pid, &local, 1, &remote, 1, 0);
    return r == (long)len;
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

static size_t RemoteUnwind(pid_t pid, const SRemoteRegs* regs, uintptr_t* frames, size_t cap)
{
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
    size_t n = RemoteUnwind(pid, &regs, frames, max_frames);
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
        if (tids[i] == g_nCrashTid) continue; // crashed thread regs come from ucontext
        if (SysPtrace(PTRACE_SEIZE, tids[i], 0, 0) == 0) {
            attached[i] = true;
            SysPtrace(PTRACE_INTERRUPT, tids[i], 0, 0);
        }
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
    return (int)syscall(SYS_pipe2, fds, O_CLOEXEC);
#else
    return -1;
#endif
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

} // namespace Private

inline int DumpFd(int fd, void* crashed_context, unsigned flags, size_t max_frames)
{
    if (fd < 0) return -1;
    Private::CaptureCrash(crashed_context, Private::SysGetTid());

    int sync_pipe[2] = {-1, -1};
    if (Private::RawPipe2(sync_pipe) != 0) return -1;

    pid_t target = Private::SysGetPid();
    pid_t child = Private::RawCloneForkLike();
    if (child < 0) {
        Private::SysClose(sync_pipe[0]);
        Private::SysClose(sync_pipe[1]);
        return -1;
    }

    if (child == 0) {
        Private::SysClose(sync_pipe[1]);
        char c = 0;
        (void)Private::SysRead(sync_pipe[0], &c, 1);
        Private::SysClose(sync_pipe[0]);
        unsigned forced = flags | DUMP_THREAD_HEADER | DUMP_SYMBOLS;
        if (forced & DUMP_ALL_THREADS) forced |= DUMP_CRASHED_FIRST;
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
    return 0;
}

inline int DumpPath(const char* path, void* crashed_context, unsigned flags,
    size_t max_frames)
{
    if (!path || !path[0]) return -1;
    int fd = (int)Private::SysOpenAt(path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0) return -1;
    int r = DumpFd(fd, crashed_context, flags, max_frames);
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
    DUMP_ALL_THREADS   = (1u << 4)
};

inline int DumpFd(int, void*, unsigned, size_t)
{ return -1; }
inline int DumpPath(const char*, void*, unsigned, size_t)
{ return -1; }
inline int DumpCurrentThreadFd(int, void*, unsigned, size_t)
{ return -1; }
inline int DumpAllThreadsFd(int, void*, unsigned, size_t)
{ return -1; }
inline int DumpCurrentThreadPath(const char*, void*, unsigned, size_t)
{ return -1; }
inline int DumpAllThreadsPath(const char*, void*, unsigned, size_t)
{ return -1; }

} // namespace JCrasher

#endif // arch

#endif // __JCRASHER_H