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

// Backtraces for arm64 and arm32, from inside a crash signal handler.
//
//   JCrasher::Init()                     once, outside of a signal handler
//   JCrasher::DumpCurrentThread(fd, uc)  the crashed thread
//   JCrasher::DumpOtherThreads(fd, ms)   all the others, call it after DumpCurrentThread
//   JCrasher::ResumeOtherThreads()       before running anything else that may take a lock
//   JCrasher::NestedFaultRecover()       first thing in the host's signal handler
//
// DumpOtherThreads leaves the other threads frozen. A host that exits right after
// the dump never has to resume them.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <link.h>
#include <linux/futex.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

namespace JCrasher
{
namespace Private
{

#if defined(__aarch64__)
    enum { kNumRegs = 33, kFP = 29, kLR = 30, kSP = 31, kPC = 32 };
    static const uint32_t kUnwindPhdr = PT_GNU_EH_FRAME;
#else
    enum { kNumRegs = 16, kSP = 13, kLR = 14, kPC = 15 };
    static const uint32_t kUnwindPhdr = 0x70000001; // PT_ARM_EXIDX. There is no such DT_ tag.
#endif

static const size_t kMaxFrames = 64;

struct SRegs { uintptr_t r[kNumRegs]; };

struct SMap
{
    uintptr_t start, end, offset;
    char perms[5];
    char path[227];
};

struct SModule
{
    uintptr_t start, end, bias, unwind, unwindSize, dynamic;
    const ElfW(Sym)* syms;
    const char* strs;
    size_t numSyms;
    bool symsLoaded;
    char path[256];
};

struct SThread
{
    pid_t tid;
    volatile int ready;
    SRegs regs;
};

static SMap g_Maps[8192];
static size_t g_nMaps;
static SModule g_Modules[512];
static size_t g_nModules;
static SThread g_Threads[512];
static size_t g_nThreads;

static int g_nParkSignal;
static volatile int g_nResumeThreads;
static sigjmp_buf g_FaultJmp;
static volatile int g_bFaultGuard;
static pid_t g_nFaultGuardTid;

// bionic caches the tid and pid, and a raw clone() child inherits the stale values.
static pid_t SelfTid()
{
    return (pid_t)syscall(SYS_gettid);
}

// ---------------------------------------------------------------- output

__attribute__((format(printf, 2, 3)))
static void Write(int fd, const char* fmt, ...)
{
    char line[768];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if(n <= 0) return;
    if(n >= (int)sizeof(line)) n = sizeof(line) - 1;
    write(fd, line, n);
}

static void WriteThreadHeader(int fd, pid_t tid, const char* note)
{
    char path[64], comm[32] = "unknown";
    snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
    int f = open(path, O_RDONLY | O_CLOEXEC);
    if(f >= 0)
    {
        ssize_t n = read(f, comm, sizeof(comm) - 1);
        if(n > 0) comm[comm[n - 1] == '\n' ? n - 1 : n] = 0;
        close(f);
    }
    Write(fd, "\n--- tid %d (%s)%s ---\n", tid, comm, note);
}

static void WriteRegs(int fd, const SRegs& regs)
{
    const int width = sizeof(uintptr_t) * 2;
    const char prefix = sizeof(uintptr_t) == 8 ? 'x' : 'r';
    char line[192];
    size_t len = 0;
    for(int i = 0; i < kNumRegs; ++i)
    {
        char name[8];
        if(sizeof(uintptr_t) == 8 && i == kSP) strcpy(name, "sp");
        else if(sizeof(uintptr_t) == 8 && i == kPC) strcpy(name, "pc");
        else snprintf(name, sizeof(name), "%c%d", prefix, i);

        len += snprintf(line + len, sizeof(line) - len, "%s%-3s 0x%0*" PRIXPTR, len ? " " : "", name, width, regs.r[i]);
        if(i % 6 == 5 || i == kNumRegs - 1)
        {
            Write(fd, "%s\n", line);
            len = 0;
        }
    }
    Write(fd, "\n");
}

// ---------------------------------------------------------------- memory

static void LoadMaps()
{
    g_nMaps = 0;
    int f = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if(f < 0) return;

    static char buf[8192];
    static char line[512];
    const size_t maxMaps = sizeof(g_Maps) / sizeof(g_Maps[0]);
    size_t lineLen = 0;
    ssize_t got;
    while((got = read(f, buf, sizeof(buf))) > 0)
    {
        for(ssize_t i = 0; i < got; ++i)
        {
            if(buf[i] != '\n')
            {
                if(lineLen + 1 < sizeof(line)) line[lineLen++] = buf[i];
                continue;
            }
            line[lineLen] = 0;
            lineLen = 0;
            if(g_nMaps >= maxMaps) continue;

            SMap& m = g_Maps[g_nMaps];
            m.path[0] = 0;
            if(sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*s %*s %226s",
                      &m.start, &m.end, m.perms, &m.offset, m.path) >= 4) ++g_nMaps;
        }
    }
    close(f);
}

static const SMap* FindMap(uintptr_t addr)
{
    size_t lo = 0, hi = g_nMaps;
    while(lo < hi)
    {
        const size_t mid = lo + (hi - lo) / 2;
        if(addr < g_Maps[mid].start) hi = mid;
        else if(addr >= g_Maps[mid].end) lo = mid + 1;
        else return &g_Maps[mid];
    }
    return NULL;
}

static bool Readable(uintptr_t addr, size_t len)
{
    const SMap* m = FindMap(addr);
    return m && m->perms[0] == 'r' && addr + len >= addr && addr + len <= m->end;
}

static bool Read(uintptr_t addr, void* out, size_t len)
{
    if(!Readable(addr, len)) return false;
    __builtin_memcpy(out, (const void*)addr, len);
    return true;
}

static bool IsCode(uintptr_t pc)
{
    const SMap* m = FindMap(pc);
    return m && m->perms[2] == 'x';
}

static uintptr_t StripPc(uintptr_t pc)
{
#if defined(__aarch64__)
    register uintptr_t x30 __asm__("x30") = pc;
    __asm__("xpaclri" : "+r"(x30));
    return x30;
#else
    return pc & ~(uintptr_t)1;
#endif
}

// A return address points after the call. The call is what the frame is doing.
static uintptr_t CallSite(uintptr_t ret)
{
#if defined(__aarch64__)
    const uintptr_t size = 4;
#else
    const uintptr_t size = (ret & 1) ? 2 : 4;
#endif
    ret = StripPc(ret);
    return ret > size ? ret - size : 0;
}

static void FillRegs(SRegs& out, void* uc)
{
    const mcontext_t& mc = ((ucontext_t*)uc)->uc_mcontext;
#if defined(__aarch64__)
    __builtin_memcpy(out.r, mc.regs, sizeof(mc.regs));
    out.r[kSP] = mc.sp;
    out.r[kPC] = mc.pc;
#else
    __builtin_memcpy(out.r, &mc.arm_r0, sizeof(out.r));
#endif
}

// ---------------------------------------------------------------- modules

static int ModuleCallback(dl_phdr_info* info, size_t, void*)
{
    if(g_nModules >= sizeof(g_Modules) / sizeof(g_Modules[0])) return 1;

    SModule& m = g_Modules[g_nModules];
    memset(&m, 0, sizeof(m));
    uintptr_t lo = ~(uintptr_t)0, hi = 0;
    for(int i = 0; i < info->dlpi_phnum; ++i)
    {
        const ElfW(Phdr)& ph = info->dlpi_phdr[i];
        if(ph.p_type == PT_LOAD)
        {
            if(ph.p_vaddr < lo) lo = ph.p_vaddr;
            if(ph.p_vaddr + ph.p_memsz > hi) hi = ph.p_vaddr + ph.p_memsz;
        }
        else if(ph.p_type == PT_DYNAMIC)
        {
            m.dynamic = info->dlpi_addr + ph.p_vaddr;
        }
        else if(ph.p_type == kUnwindPhdr)
        {
            m.unwind = info->dlpi_addr + ph.p_vaddr;
            m.unwindSize = ph.p_memsz;
        }
    }
    if(!hi) return 0;

    m.bias = info->dlpi_addr;
    m.start = m.bias + lo;
    m.end = m.bias + hi;
    if(info->dlpi_name) strncpy(m.path, info->dlpi_name, sizeof(m.path) - 1);
    ++g_nModules;
    return 0;
}

static const SModule* FindModuleConst(uintptr_t pc)
{
    for(size_t i = 0; i < g_nModules; ++i)
    {
        if(pc >= g_Modules[i].start && pc < g_Modules[i].end) return &g_Modules[i];
    }
    return NULL;
}

static size_t CountDynSyms(uintptr_t sysvHash, uintptr_t gnuHash)
{
    uint32_t h[4];
    if(sysvHash && Read(sysvHash, h, 8)) return h[1];
    if(!gnuHash || !Read(gnuHash, h, 16)) return 0;

    const uint32_t nbuckets = h[0], symoffset = h[1], bloom = h[2];
    const uintptr_t buckets = gnuHash + 16 + (uintptr_t)bloom * sizeof(uintptr_t);
    const uintptr_t chains = buckets + (uintptr_t)nbuckets * 4;

    uint32_t last = 0;
    for(uint32_t i = 0; i < nbuckets; ++i)
    {
        uint32_t b;
        if(!Read(buckets + (uintptr_t)i * 4, &b, 4)) return 0;
        if(b > last) last = b;
    }
    if(last < symoffset) return symoffset;
    for(;; ++last)
    {
        uint32_t c;
        if(!Read(chains + (uintptr_t)(last - symoffset) * 4, &c, 4)) return 0;
        if(c & 1) return (size_t)last + 1;
    }
}

static void LoadSymbols(SModule& m)
{
    m.symsLoaded = true;
    if(!m.dynamic) return;

    uintptr_t symtab = 0, strtab = 0, sysvHash = 0, gnuHash = 0;
    for(uintptr_t p = m.dynamic;; p += sizeof(ElfW(Dyn)))
    {
        ElfW(Dyn) d;
        if(!Read(p, &d, sizeof(d)) || d.d_tag == DT_NULL) break;
        const uintptr_t addr = m.bias + d.d_un.d_ptr;
        if(d.d_tag == DT_SYMTAB) symtab = addr;
        else if(d.d_tag == DT_STRTAB) strtab = addr;
        else if(d.d_tag == DT_HASH) sysvHash = addr;
        else if(d.d_tag == DT_GNU_HASH) gnuHash = addr;
    }
    if(!symtab || !strtab) return;

    const size_t count = CountDynSyms(sysvHash, gnuHash);
    if(!Readable(symtab, count * sizeof(ElfW(Sym)))) return;
    m.syms = (const ElfW(Sym)*)symtab;
    m.strs = (const char*)strtab;
    m.numSyms = count;
}

// Only a function that really contains pc. "Nearest symbol before pc" names some
// unrelated export whenever the real function was stripped.
static const char* FindSymbol(const SModule* constModule, uintptr_t pc, uintptr_t* offset)
{
    SModule& m = *const_cast<SModule*>(constModule);
    if(!m.symsLoaded) LoadSymbols(m);

    const uintptr_t vaddr = pc - m.bias;
    for(size_t i = 0; i < m.numSyms; ++i)
    {
        const ElfW(Sym)& s = m.syms[i];
        if((s.st_info & 0xF) != STT_FUNC || !s.st_shndx || !s.st_name) continue;
        const uintptr_t start = StripPc(s.st_value);
        if(vaddr - start < s.st_size)
        {
            *offset = vaddr - start;
            return m.strs + s.st_name;
        }
    }
    return NULL;
}

static void WriteFrame(int fd, const char* prefix, size_t index, uintptr_t pc)
{
    const int width = sizeof(uintptr_t) * 2;
    uintptr_t base = 0, rel = pc;
    const char* path = "<unmapped>";
    char symbol[256] = "";

    if(const SModule* m = FindModuleConst(pc))
    {
        base = m->bias;
        rel = pc - base;
        if(m->path[0]) path = m->path;
        uintptr_t offset = 0;
        const char* name = FindSymbol(m, pc, &offset);
        if(name) snprintf(symbol, sizeof(symbol), " (%.200s+0x%" PRIXPTR ")", name, offset);
    }
    else if(const SMap* map = FindMap(pc))
    {
        path = map->path[0] ? map->path : "<anonymous>";
        if(map->path[0])
        {
            base = map->start - map->offset;
            rel = pc - base;
        }
    }
    Write(fd, "%s#%02u pc 0x%0*" PRIXPTR " base 0x%0*" PRIXPTR " %s%s\n",
          prefix, (unsigned)index, width, rel, width, base, path, symbol);
}

// ---------------------------------------------------------------- unwinding

#if defined(__aarch64__)

template<typename T>
static T Take(const uint8_t*& p)
{
    T v;
    __builtin_memcpy(&v, p, sizeof(T));
    p += sizeof(T);
    return v;
}

static uint64_t Uleb(const uint8_t*& p, const uint8_t* end)
{
    uint64_t v = 0;
    for(unsigned shift = 0; p < end && shift < 64; shift += 7)
    {
        const uint8_t b = *p++;
        v |= (uint64_t)(b & 0x7F) << shift;
        if(!(b & 0x80)) break;
    }
    return v;
}

static int64_t Sleb(const uint8_t*& p, const uint8_t* end)
{
    uint64_t v = 0;
    unsigned shift = 0;
    uint8_t b = 0;
    while(p < end && shift < 64)
    {
        b = *p++;
        v |= (uint64_t)(b & 0x7F) << shift;
        shift += 7;
        if(!(b & 0x80)) break;
    }
    if((b & 0x40) && shift < 64) v |= ~(uint64_t)0 << shift;
    return (int64_t)v;
}

static bool ReadEncoded(const uint8_t*& p, uint8_t enc, uintptr_t& out)
{
    const uintptr_t at = (uintptr_t)p;
    switch(enc & 0xF)
    {
        case 0x0: case 0x4: case 0xC: out = Take<uint64_t>(p); break;
        case 0x2: out = Take<uint16_t>(p); break;
        case 0x3: out = Take<uint32_t>(p); break;
        case 0xA: out = (uintptr_t)(int64_t)Take<int16_t>(p); break;
        case 0xB: out = (uintptr_t)(int64_t)Take<int32_t>(p); break;
        default: return false;
    }
    if((enc & 0x70) == 0x10) out += at;
    return true;
}

struct SCie
{
    uint64_t codeAlign;
    int64_t dataAlign;
    unsigned returnReg;
    uint8_t fdeEnc;
    bool hasAugData;
    const uint8_t* insn;
    const uint8_t* end;
};

static bool ParseCie(uintptr_t cie, SCie& c)
{
    uint32_t len, id;
    if(!Read(cie, &len, 4) || !Read(cie + 4, &id, 4)) return false;
    if(id != 0 || len == 0xFFFFFFFFu || !Readable(cie, (size_t)len + 4)) return false;

    const uint8_t* p = (const uint8_t*)cie + 8;
    const uint8_t* end = (const uint8_t*)cie + 4 + len;
    const uint8_t version = *p++;
    const char* aug = (const char*)p;
    p += strlen(aug) + 1;

    c.codeAlign = Uleb(p, end);
    c.dataAlign = Sleb(p, end);
    c.returnReg = version == 1 ? *p++ : (unsigned)Uleb(p, end);
    c.fdeEnc = 0;
    c.hasAugData = aug[0] == 'z';
    if(c.hasAugData)
    {
        const uint64_t augLen = Uleb(p, end);
        const uint8_t* augEnd = p + augLen;
        for(const char* a = aug + 1; *a; ++a)
        {
            if(*a == 'R') c.fdeEnc = *p++;
            else if(*a == 'L') ++p;
            else if(*a == 'P')
            {
                const uint8_t enc = *p++;
                uintptr_t ignored;
                if(!ReadEncoded(p, enc, ignored)) return false;
            }
        }
        p = augEnd;
    }
    c.insn = p;
    c.end = end;
    return p <= end;
}

enum eRule : uint8_t { RULE_SAME, RULE_UNDEFINED, RULE_OFFSET, RULE_VAL_OFFSET, RULE_REGISTER };

struct SCfi
{
    uint64_t cfaReg;
    int64_t cfaOffset;
    bool cfaIsExpression;
    struct { eRule kind; int64_t value; } rules[32];
};

static bool RunCfi(const uint8_t* p, const uint8_t* end, uintptr_t loc, uintptr_t pc,
                   const SCie& c, SCfi& st, const SCfi* initial)
{
    SCfi saved[4];
    unsigned numSaved = 0;

    const auto setRule = [&st](uint64_t reg, eRule kind, int64_t value)
    {
        if(reg >= 32) return;
        st.rules[reg].kind = kind;
        st.rules[reg].value = value;
    };
    const auto restoreRule = [&](uint64_t reg)
    {
        if(reg < 32 && initial) st.rules[reg] = initial->rules[reg];
    };
    const auto advance = [&](uint64_t delta)
    {
        loc += delta * c.codeAlign;
        return loc <= pc;
    };

    while(p < end)
    {
        const uint8_t op = *p++;
        const uint8_t low = op & 0x3F;
        switch(op & 0xC0)
        {
            case 0x40: if(!advance(low)) return true; continue;
            case 0x80: setRule(low, RULE_OFFSET, (int64_t)Uleb(p, end) * c.dataAlign); continue;
            case 0xC0: restoreRule(low); continue;
        }

        uint64_t reg, value;
        switch(op)
        {
            case 0x00: break;
            case 0x02: if(!advance(Take<uint8_t>(p))) return true; break;
            case 0x03: if(!advance(Take<uint16_t>(p))) return true; break;
            case 0x04: if(!advance(Take<uint32_t>(p))) return true; break;
            case 0x05:
                reg = Uleb(p, end);
                setRule(reg, RULE_OFFSET, (int64_t)Uleb(p, end) * c.dataAlign);
                break;
            case 0x06: restoreRule(Uleb(p, end)); break;
            case 0x07: setRule(Uleb(p, end), RULE_UNDEFINED, 0); break;
            case 0x08: setRule(Uleb(p, end), RULE_SAME, 0); break;
            case 0x09:
                reg = Uleb(p, end);
                setRule(reg, RULE_REGISTER, (int64_t)Uleb(p, end));
                break;
            case 0x0A:
                if(numSaved >= 4) return false;
                saved[numSaved++] = st;
                break;
            case 0x0B:
                if(!numSaved) return false;
                st = saved[--numSaved];
                break;
            case 0x0C:
                st.cfaReg = Uleb(p, end);
                st.cfaOffset = (int64_t)Uleb(p, end);
                st.cfaIsExpression = false;
                break;
            case 0x0D: st.cfaReg = Uleb(p, end); st.cfaIsExpression = false; break;
            case 0x0E: st.cfaOffset = (int64_t)Uleb(p, end); break;
            case 0x0F:
                value = Uleb(p, end);
                p += value;
                st.cfaIsExpression = true;
                break;
            case 0x10: case 0x16:
                reg = Uleb(p, end);
                value = Uleb(p, end);
                p += value;
                setRule(reg, RULE_UNDEFINED, 0);
                break;
            case 0x11:
                reg = Uleb(p, end);
                setRule(reg, RULE_OFFSET, Sleb(p, end) * c.dataAlign);
                break;
            case 0x12:
                st.cfaReg = Uleb(p, end);
                st.cfaOffset = Sleb(p, end) * c.dataAlign;
                st.cfaIsExpression = false;
                break;
            case 0x13: st.cfaOffset = Sleb(p, end) * c.dataAlign; break;
            case 0x14:
                reg = Uleb(p, end);
                setRule(reg, RULE_VAL_OFFSET, (int64_t)Uleb(p, end) * c.dataAlign);
                break;
            case 0x15:
                reg = Uleb(p, end);
                setRule(reg, RULE_VAL_OFFSET, Sleb(p, end) * c.dataAlign);
                break;
            case 0x2D: break; // PAC state, StripPc already handles it
            case 0x2E: Uleb(p, end); break;
            default: return false;
        }
    }
    return true;
}

static uintptr_t FindFde(const SModule& m, uintptr_t pc)
{
    const uint8_t* hdr = (const uint8_t*)m.unwind;
    if(!Readable(m.unwind, 12)) return 0;
    if(hdr[0] != 1 || (hdr[1] & 0xF) != 0xB || hdr[2] != 0x03 || hdr[3] != 0x3B) return 0;

    uint32_t count;
    __builtin_memcpy(&count, hdr + 8, 4);
    const int32_t* table = (const int32_t*)(hdr + 12);
    if(!count || !Readable((uintptr_t)table, (size_t)count * 8)) return 0;

    size_t lo = 0, hi = count;
    uintptr_t fde = 0;
    while(lo < hi)
    {
        const size_t mid = lo + (hi - lo) / 2;
        if(m.unwind + (intptr_t)table[mid * 2] <= pc)
        {
            fde = m.unwind + (intptr_t)table[mid * 2 + 1];
            lo = mid + 1;
        }
        else hi = mid;
    }
    return fde;
}

static bool TableStep(const SModule& m, SRegs& r)
{
    const uintptr_t pc = r.r[kPC];
    const uintptr_t fde = FindFde(m, pc);
    if(!fde) return false;

    uint32_t len, cieOffset;
    if(!Read(fde, &len, 4) || !Read(fde + 4, &cieOffset, 4)) return false;
    if(!cieOffset || len == 0xFFFFFFFFu || !Readable(fde, (size_t)len + 4)) return false;

    SCie cie;
    if(!ParseCie(fde + 4 - cieOffset, cie) || cie.returnReg >= 32) return false;

    const uint8_t* p = (const uint8_t*)fde + 8;
    const uint8_t* end = (const uint8_t*)fde + 4 + len;
    uintptr_t funcStart, funcSize;
    if(!ReadEncoded(p, cie.fdeEnc, funcStart) || !ReadEncoded(p, cie.fdeEnc & 0xF, funcSize)) return false;
    if(pc < funcStart || pc - funcStart >= funcSize) return false;
    if(cie.hasAugData)
    {
        const uint64_t augLen = Uleb(p, end);
        p += augLen;
    }

    SCfi st;
    memset(&st, 0, sizeof(st));
    if(!RunCfi(cie.insn, cie.end, 0, ~(uintptr_t)0, cie, st, NULL)) return false;
    const SCfi initial = st;
    if(!RunCfi(p, end, funcStart, pc, cie, st, &initial)) return false;
    if(st.cfaIsExpression || st.cfaReg >= 32) return false;

    const uintptr_t sp = r.r[kSP];
    const uintptr_t cfa = r.r[st.cfaReg] + st.cfaOffset;
    if(cfa < sp || cfa - sp > 16 * 1024 * 1024) return false;

    SRegs next = r;
    for(unsigned i = 0; i < 32; ++i)
    {
        const int64_t value = st.rules[i].value;
        switch(st.rules[i].kind)
        {
            case RULE_OFFSET:
                if(!Read(cfa + value, &next.r[i], sizeof(uintptr_t))) return false;
                break;
            case RULE_VAL_OFFSET: next.r[i] = cfa + value; break;
            case RULE_REGISTER:
                if((uint64_t)value >= 32) return false;
                next.r[i] = r.r[value];
                break;
            case RULE_UNDEFINED:
                if(i == cie.returnReg) return false;
                break;
            case RULE_SAME: break;
        }
    }
    next.r[kSP] = cfa;
    next.r[kPC] = CallSite(next.r[cie.returnReg]);
    if(!IsCode(next.r[kPC])) return false;
    if(next.r[kPC] == pc && cfa == sp) return false;
    r = next;
    return true;
}

// For code with no tables (JIT, ART stubs). x29 is reserved on Android arm64, and
// once on it the real SP is unknown, so tables cannot be used for the outer frames.
static bool FramePointerStep(SRegs& r)
{
    const uintptr_t fp = r.r[kFP];
    if(!fp || (fp & 7) || fp < r.r[kSP]) return false;

    uintptr_t frame[2];
    if(!Read(fp, frame, sizeof(frame))) return false;
    if(frame[0] && frame[0] <= fp) return false;

    const uintptr_t pc = CallSite(frame[1]);
    if(!IsCode(pc)) return false;
    r.r[kFP] = frame[0];
    r.r[kSP] = fp + sizeof(frame);
    r.r[kPC] = pc;
    return true;
}

#else // arm32

static uintptr_t Prel31(uintptr_t addr)
{
    uint32_t v = 0;
    if(!Read(addr, &v, 4)) return 0;
    return addr + (intptr_t)((int32_t)(v << 1) >> 1);
}

static bool Pop(SRegs& r, unsigned reg, uintptr_t& vsp)
{
    if(!Read(vsp, &r.r[reg], 4)) return false;
    vsp += 4;
    return true;
}

static bool TableStep(const SModule& m, SRegs& r)
{
    const uintptr_t pc = r.r[kPC];

    uintptr_t entry = 0;
    size_t lo = 0, hi = m.unwindSize / 8;
    while(lo < hi)
    {
        const size_t mid = lo + (hi - lo) / 2;
        const uintptr_t e = m.unwind + mid * 8;
        const uintptr_t fn = Prel31(e);
        if(!fn) return false;
        if((fn & ~(uintptr_t)1) <= pc) { entry = e; lo = mid + 1; }
        else hi = mid;
    }

    uint32_t data;
    if(!entry || !Read(entry + 4, &data, 4) || data == 1) return false; // 1 = EXIDX_CANTUNWIND

    uintptr_t table = (data & 0x80000000u) ? entry + 4 : Prel31(entry + 4);
    uint32_t first;
    if(!Read(table, &first, 4)) return false;

    // Byte stream, big-endian inside each word. Bytes before `pos` are not instructions.
    size_t pos, numBytes;
    if(first & 0x80000000u)
    {
        const unsigned personality = (first >> 24) & 0xF;
        if(personality > 2) return false;
        pos = personality == 0 ? 1 : 2;
        numBytes = personality == 0 ? 4 : (((first >> 16) & 0xFF) + 1) * 4;
    }
    else
    {
        // Generic model: personality routine, then count byte + instructions.
        table += 4;
        if(!Read(table, &first, 4)) return false;
        pos = 1;
        numBytes = ((first >> 24) + 1) * 4;
    }
    if(!Readable(table, numBytes)) return false;

    const uint32_t* words = (const uint32_t*)table;
    const auto next = [&]() -> uint32_t
    {
        if(pos >= numBytes) return 0xB0;
        const uint32_t b = (words[pos / 4] >> (24 - 8 * (pos % 4))) & 0xFF;
        ++pos;
        return b;
    };

    SRegs n = r;
    uintptr_t vsp = r.r[kSP];
    bool popsPc = false;
    for(;;)
    {
        const uint32_t b = next();
        if(b < 0x40) vsp += (b << 2) + 4;
        else if(b < 0x80) vsp -= ((b & 0x3F) << 2) + 4;
        else if(b < 0x90)
        {
            const uint32_t mask = ((b & 0xF) << 8) | next();
            if(!mask) return false;
            for(unsigned i = 0; i < 12; ++i)
            {
                if((mask & (1u << i)) && !Pop(n, 4 + i, vsp)) return false;
            }
            if(mask & (1u << 9)) vsp = n.r[13];
            if(mask & (1u << 11)) popsPc = true;
        }
        else if(b < 0xA0)
        {
            const unsigned reg = b & 0xF;
            if(reg == 13 || reg == 15) return false;
            vsp = n.r[reg];
        }
        else if(b < 0xB0)
        {
            for(unsigned reg = 4; reg <= 4 + (b & 7); ++reg)
            {
                if(!Pop(n, reg, vsp)) return false;
            }
            if((b & 8) && !Pop(n, 14, vsp)) return false;
        }
        else if(b == 0xB0) break;
        else if(b == 0xB1)
        {
            const uint32_t mask = next();
            if(!mask || (mask & 0xF0)) return false;
            for(unsigned i = 0; i < 4; ++i)
            {
                if((mask & (1u << i)) && !Pop(n, i, vsp)) return false;
            }
        }
        else if(b == 0xB2)
        {
            uint32_t value = 0, c;
            unsigned shift = 0;
            do
            {
                c = next();
                value |= (c & 0x7F) << shift;
                shift += 7;
            } while((c & 0x80) && shift < 32);
            vsp += 0x204 + (value << 2);
        }
        else if(b == 0xB3) vsp += ((next() & 0xF) + 1) * 8 + 4;
        else if(b >= 0xB8 && b < 0xC0) vsp += ((b & 7) + 1) * 8 + 4;
        else if(b == 0xC8 || b == 0xC9) vsp += ((next() & 0xF) + 1) * 8;
        else if(b >= 0xD0 && b < 0xD8) vsp += ((b & 7) + 1) * 8;
        else return false;
    }

    const uintptr_t caller = CallSite(popsPc ? n.r[15] : n.r[14]);
    if(!IsCode(caller)) return false;
    if(caller == pc && vsp == r.r[kSP]) return false;
    n.r[kSP] = vsp;
    n.r[kPC] = caller;
    r = n;
    return true;
}

#endif

static size_t Unwind(SRegs r, uintptr_t* frames)
{
    r.r[kPC] = StripPc(r.r[kPC]);
    frames[0] = r.r[kPC];

    size_t n = 1;
#if defined(__aarch64__)
    bool framePointerOnly = false;
#else
    const bool framePointerOnly = false;
#endif
    for(; n < kMaxFrames; frames[n++] = r.r[kPC])
    {
        const SModule* m = FindModuleConst(r.r[kPC]);
        if(!framePointerOnly && m && m->unwind && TableStep(*m, r)) continue;

        // A call to NULL or to garbage: nothing was pushed, so LR is the caller.
        if(n == 1)
        {
            const uintptr_t caller = CallSite(r.r[kLR]);
            if(caller != r.r[kPC] && IsCode(caller))
            {
                r.r[kPC] = caller;
                continue;
            }
        }
#if defined(__aarch64__)
        framePointerOnly = true;
        if(FramePointerStep(r)) continue;
#endif
        break;
    }
    return n;
}

// The stack holds stale return addresses too, so this is a hint, never a backtrace.
static void WriteStackHints(int fd, uintptr_t sp)
{
    const SMap* stack = FindMap(sp);
    if(!stack || stack->perms[0] != 'r') return;

    Write(fd, "<unwind stopped early. Code addresses found on the stack, may be stale, NOT a call stack:>\n");
    const uintptr_t aligned = (sp + sizeof(uintptr_t) - 1) & ~(uintptr_t)(sizeof(uintptr_t) - 1);
    const size_t maxWords = 8192 / sizeof(uintptr_t);
    size_t found = 0;
    for(size_t i = 0; i < maxWords && found < 16; ++i)
    {
        const uintptr_t slot = aligned + i * sizeof(uintptr_t);
        if(slot + sizeof(uintptr_t) > stack->end) break;

        const uintptr_t pc = CallSite(*(const uintptr_t*)slot);
        if(pc && IsCode(pc) && FindModuleConst(pc)) WriteFrame(fd, "? ", found++, pc);
    }
}

static void DumpThread(int fd, pid_t tid, const SRegs& regs, bool crashed, bool writeRegs)
{
    static uintptr_t frames[kMaxFrames];

    WriteThreadHeader(fd, tid, crashed ? " CRASHED" : "");
    if(writeRegs) WriteRegs(fd, regs);

    const size_t n = Unwind(regs, frames);
    for(size_t i = 0; i < n; ++i) WriteFrame(fd, "", i, frames[i]);
    if(crashed && n < 3) WriteStackHints(fd, regs.r[kSP]);
}

// ---------------------------------------------------------------- fault guard

static void UnblockFaults()
{
    // Raw: libc's sigprocmask goes through ART's libsigchain.
    const uint64_t set = (1ull << (SIGSEGV - 1)) | (1ull << (SIGBUS - 1));
    syscall(SYS_rt_sigprocmask, SIG_UNBLOCK, &set, NULL, sizeof(set));
}

// Corrupted memory can fault even behind the maps check. Loses one thread, not the log.
static void GuardedDumpThread(int fd, pid_t tid, const SRegs& regs, bool crashed, bool writeRegs)
{
    g_nFaultGuardTid = SelfTid();
    UnblockFaults();
    if(sigsetjmp(g_FaultJmp, 0) == 0)
    {
        g_bFaultGuard = 1;
        DumpThread(fd, tid, regs, crashed, writeRegs);
    }
    else
    {
        UnblockFaults();
        Write(fd, "<fault while reading this thread, rest of it skipped>\n");
    }
    g_bFaultGuard = 0;
}

// ---------------------------------------------------------------- other threads

static uint64_t NowMs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void ParkHandler(int, siginfo_t*, void* uc)
{
    const int savedErrno = errno;
    const pid_t self = SelfTid();
    for(size_t i = 0; i < g_nThreads; ++i)
    {
        if(g_Threads[i].tid != self) continue;
        FillRegs(g_Threads[i].regs, uc);
        __atomic_store_n(&g_Threads[i].ready, 1, __ATOMIC_RELEASE);
        break;
    }
    while(!__atomic_load_n(&g_nResumeThreads, __ATOMIC_ACQUIRE))
    {
        syscall(SYS_futex, &g_nResumeThreads, FUTEX_WAIT_PRIVATE, 0, NULL, NULL, 0);
    }
    errno = savedErrno;
}

static void ListOtherThreads(pid_t self)
{
    g_nThreads = 0;
    int f = open("/proc/self/task", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if(f < 0) return;

    static char buf[8192];
    long got;
    while((got = syscall(SYS_getdents64, f, buf, sizeof(buf))) > 0)
    {
        for(long pos = 0; pos < got; pos += ((dirent64*)(buf + pos))->d_reclen)
        {
            const pid_t tid = (pid_t)strtol(((dirent64*)(buf + pos))->d_name, NULL, 10);
            if(tid <= 0 || tid == self) continue;
            if(g_nThreads >= sizeof(g_Threads) / sizeof(g_Threads[0])) break;
            g_Threads[g_nThreads].tid = tid;
            g_Threads[g_nThreads].ready = 0;
            ++g_nThreads;
        }
    }
    close(f);
}

static void ParkOtherThreads(pid_t self)
{
    ListOtherThreads(self);

    const pid_t pid = (pid_t)syscall(SYS_getpid);
    for(size_t i = 0; i < g_nThreads; ++i) syscall(SYS_tgkill, pid, g_Threads[i].tid, g_nParkSignal);

    // A thread stuck in D-state or masking every signal never answers.
    const uint64_t giveUpAt = NowMs() + 300;
    const timespec tick = { 0, 1000000 };
    for(;;)
    {
        size_t numReady = 0;
        for(size_t i = 0; i < g_nThreads; ++i) numReady += __atomic_load_n(&g_Threads[i].ready, __ATOMIC_ACQUIRE);
        if(numReady == g_nThreads || NowMs() >= giveUpAt) break;
        nanosleep(&tick, NULL);
    }
}

static void Prepare()
{
    LoadMaps();
    g_nModules = 0;
    dl_iterate_phdr(ModuleCallback, NULL);
}

} // namespace Private

// Installs the handler that freezes the other threads. sigaction from a crash handler isn't safe.
inline void Init()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = Private::ParkHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigfillset(&sa.sa_mask);

    const int sig = SIGRTMIN + 4; // past libc's reserved signals and ART's
    if(sigaction(sig, &sa, NULL) == 0) Private::g_nParkSignal = sig;
}

// Does not return when the fault came from inside a dump.
inline void NestedFaultRecover()
{
    if(Private::g_bFaultGuard && Private::SelfTid() == Private::g_nFaultGuardTid)
    {
        Private::g_bFaultGuard = 0;
        siglongjmp(Private::g_FaultJmp, 1);
    }
}

// How many of len bytes at addr are mapped readable, for a log that dumps raw memory.
inline size_t ReadableBytes(uintptr_t addr, size_t len)
{
    using namespace Private;
    if(!g_nMaps) LoadMaps();
    const SMap* m = FindMap(addr);
    if(!m || m->perms[0] != 'r') return 0;
    const size_t left = m->end - addr;
    return left < len ? left : len;
}

inline void DumpCurrentThread(int fd, void* ucontext)
{
    using namespace Private;
    Prepare();
    if(!g_nMaps) Write(fd, "<cannot read /proc/self/maps, backtraces will be cut short>\n");
    else if(g_nMaps >= sizeof(g_Maps) / sizeof(g_Maps[0])) Write(fd, "<too many memory maps, high addresses are unknown>\n");
    SRegs regs;
    FillRegs(regs, ucontext);
    GuardedDumpThread(fd, SelfTid(), regs, true, false);
}

inline void DumpOtherThreads(int fd, unsigned budgetMs, bool writeRegs = true)
{
    using namespace Private;
    if(!g_nParkSignal)
    {
        Write(fd, "<JCrasher::Init was not called, other threads skipped>\n");
        return;
    }
    const uint64_t deadline = NowMs() + budgetMs;
    if(!g_nModules) Prepare();

    // Frozen threads may hold the linker lock: no dl_iterate_phdr/dladdr after this.
    ParkOtherThreads(SelfTid());
    LoadMaps();

    for(size_t i = 0; i < g_nThreads; ++i)
    {
        if(NowMs() >= deadline)
        {
            Write(fd, "\n<time budget exhausted, %u threads skipped>\n", (unsigned)(g_nThreads - i));
            break;
        }
        if(__atomic_load_n(&g_Threads[i].ready, __ATOMIC_ACQUIRE))
        {
            GuardedDumpThread(fd, g_Threads[i].tid, g_Threads[i].regs, false, writeRegs);
        }
        else
        {
            WriteThreadHeader(fd, g_Threads[i].tid, " did not stop, no registers");
        }
    }
}

inline void ResumeOtherThreads()
{
    __atomic_store_n(&Private::g_nResumeThreads, 1, __ATOMIC_RELEASE);
    syscall(SYS_futex, &Private::g_nResumeThreads, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}

} // namespace JCrasher

#endif // __JCRASHER_H
