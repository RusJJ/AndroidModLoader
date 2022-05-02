#include "ARMPatch.h"
#include <ctype.h>

using namespace std;
namespace ARMPatch
{
    uintptr_t getLib(const char* soLib)
    {
        FILE *fp = NULL;
        uintptr_t address = 0;
        char buffer[2048];

        fp = fopen( "/proc/self/maps", "rt" );
        if (fp != NULL)
        {
            while (fgets(buffer, sizeof(buffer)-1, fp))
            {
                if ( strstr( buffer, soLib ) )
                {
                    address = (uintptr_t)strtoul( buffer, NULL, 16 );
                    break;
                }
            }
            fclose(fp);
        }
        return address;
    }
    uintptr_t getLibLength(const char* soLib)
    {
        FILE *fp = NULL;
        uintptr_t address = 0, end_address = 0;
        char buffer[2048];

        fp = fopen( "/proc/self/maps", "rt" );
        if (fp != NULL)
        {
            while (fgets(buffer, sizeof(buffer)-1, fp))
            {
                if ( strstr( buffer, soLib ) )
                {
                    const char* secondPart = strchr(buffer, '-');
                    if(!address) end_address = address = (uintptr_t)strtoul( buffer, NULL, 16 );
                    if(secondPart != NULL) end_address = (uintptr_t)strtoul( secondPart + 1, NULL, 16 );
                }
            }
            fclose(fp);
        }
        return end_address - address;
    }
    uintptr_t getSym(void* handle, const char* sym)
    {
        return (uintptr_t)dlsym(handle, sym);
    }
    uintptr_t getSym(uintptr_t libAddr, const char* sym)
    {
        Dl_info info;
        if(dladdr((void*)libAddr, &info) == 0) return 0;
        return (uintptr_t)dlsym(info.dli_fbase, sym);
    }
    int unprotect(uintptr_t addr, size_t len)
    {
        return mprotect((void*)(addr & 0xFFFFF000), len, PROT_READ | PROT_WRITE | PROT_EXEC);
    }
    void write(uintptr_t dest, uintptr_t src, size_t size)
    {
        unprotect(dest);
        memcpy((void*)dest, (void*)src, size);
        cacheflush(CLEAR_BIT0(dest), CLEAR_BIT0(dest) + size, 0);
    }
    void read(uintptr_t src, uintptr_t dest, size_t size)
    {
        unprotect(src);
        memcpy((void*)src, (void*)dest, size);
    }
    void NOP(uintptr_t addr, size_t count)
    {
        unprotect(addr);
        for (uintptr_t p = addr; p != (addr + count*2); p += 2)
        {
            (*(char*)(p + 0)) = 0x00;
            (*(char*)(p + 1)) = 0xBF;
        }
        cacheflush(CLEAR_BIT0(addr), CLEAR_BIT0(addr) + count * 2, 0);
    }
    void JMP(uintptr_t addr, uintptr_t dest)
    {
        uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
                           ((((dest - addr - 4) >> 1) & 0x7FF | 0xB800) << 16);
        write(addr, (uintptr_t)&newDest, 4);
    }
    void BLX(uintptr_t addr, uintptr_t dest)
    {
        uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
                           ((((dest - addr - 4) >> 1) & 0x7FF | 0xF800) << 16);
        write(addr, (uintptr_t)&newDest, 4);
    }
    void RET(uintptr_t addr)
    {
        write(addr, (uintptr_t)"\xF7\x46", 2);
    }
    void redirect(uintptr_t addr, uintptr_t to) // Was taken from TheOfficialFloW's git repo, should not work on ARM64 rn
    {
    #ifdef __arm__
        if(!addr) return;
        uint32_t hook[2] = {0xE51FF004, to};
        if(addr & 1)
        {
            addr &= ~1;
            if (addr & 2)
            {
                NOP(addr, 1); 
                addr += 2;
            }
            hook[0] = 0xF000F8DF;
        }
        write(addr, (uintptr_t)hook, sizeof(hook));
    #elif defined __aarch64__
        // TODO:
    #endif
    }
    bool hookInternal(void* addr, void* func, void** original)
    {
        if (addr == NULL || func == NULL || addr == func) return false;
        unprotect((uintptr_t)addr);
        #ifdef __arm__
            return MSHookFunction(addr, func, original);
        #elif defined __aarch64__
            return A64HookFunction(addr, func, original);
        #endif
    }
    void hookPLTInternal(void* addr, void* func, void** original)
    {
        if (addr == NULL || func == NULL || addr == func) return;
        unprotect((uintptr_t)addr);
        if(original != NULL) *((uintptr_t*)original) = *(uintptr_t*)addr;
        *(uintptr_t*)addr = (uintptr_t)func;
    }
    
    bool compareData(const uint8_t* data, const bytePattern::byteEntry* pattern, size_t patternlength)
    {
        int index = 0;
        for (size_t i = 0; i < patternlength; i++)
        {
            auto byte = *pattern;
            if (!byte.bUnknown && *data != byte.nValue) return false;

            ++data;
            ++pattern;
            ++index;
        }
        return index == patternlength;
    }
    uintptr_t getAddressFromPattern(const char* pattern, const char* soLib)
    {
        bytePattern ret;
        const char* input = &pattern[0];
        while (*input)
        {
            bytePattern::byteEntry entry;
            if (isspace(*input)) ++input;
            if (isxdigit(*input))
            {
                entry.bUnknown = false;
                entry.nValue = (uint8_t)std::strtol(input, NULL, 16);
                input += 2;
            }
            else
            {
                entry.bUnknown = true;
                input += 2;
            }
            ret.vBytes.emplace_back(entry);
        }
        
        auto patternstart = ret.vBytes.data();
        auto length = ret.vBytes.size();

        uintptr_t pMemoryBase = getLib(soLib);
        size_t nMemorySize = getLibLength(soLib) - length;

        for (size_t i = 0; i < nMemorySize; i++)
        {
            uintptr_t addr = pMemoryBase + i;
            if (compareData((const uint8_t*)addr, patternstart, length)) return addr;
        }
        return (uintptr_t)0;
    }
    uintptr_t getAddressFromPattern(const char* pattern, uintptr_t libStart, uintptr_t scanLen)
    {
        bytePattern ret;
        const char* input = &pattern[0];
        while (*input)
        {
            bytePattern::byteEntry entry;
            if (isspace(*input)) ++input;
            if (isxdigit(*input))
            {
                entry.bUnknown = false;
                entry.nValue = (uint8_t)std::strtol(input, NULL, 16);
                input += 2;
            }
            else
            {
                entry.bUnknown = true;
                input += 2;
            }
            ret.vBytes.emplace_back(entry);
        }
        
        auto patternstart = ret.vBytes.data();
        auto length = ret.vBytes.size();

        uintptr_t scanSize = libStart + scanLen;
        for (size_t i = 0; i < scanSize; i++)
        {
            uintptr_t addr = libStart + i;
            if (compareData((const uint8_t*)addr, patternstart, length)) return addr;
        }
        return (uintptr_t)0;
    }
}