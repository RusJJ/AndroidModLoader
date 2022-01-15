#include "ARMPatch.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>

#ifdef __arm__
	extern "C" bool MSHookFunction(void* symbol, void* replace, void** result);
#elif defined __aarch64__
	extern "C" bool A64HookFunction(void *const symbol, void *const replace, void **result);
	#define cacheflush(c, n, zeroarg) __builtin___clear_cache((char*)(c), (char*)(n))
#else
	#error This lib is supposed to work on ARM only!
#endif

using namespace std;
namespace ARMPatch
{
	uintptr_t getLib(const char* soLib)
	{
		char filename[0xFF] = {0},
		buffer[2048] = {0};
		FILE *fp = 0;
		uintptr_t address = 0;

		fp = fopen( "/proc/self/maps", "rt" );
		if (fp != NULL)
		{
			while (fgets(buffer, sizeof(buffer), fp))
			{
				if ( strstr( buffer, soLib ) )
				{
					address = (uintptr_t)strtoul( buffer, 0, 16 );
					break;
				}
			}
			fclose(fp);
		}
		return address;
	}
	uintptr_t getLibLength(const char* soLib)
	{
		char filename[0xFF] = {0},
		buffer[2048] = {0};
		FILE *fp = 0;
		uintptr_t address = 0;

		fp = fopen( "/proc/self/maps", "rt" );
		if (fp != NULL)
		{
			while (fgets(buffer, sizeof(buffer), fp))
			{
				if ( strstr( buffer, soLib ) )
				{
					address = (uintptr_t)strtoul( buffer, 0, 16 );
					address = (uintptr_t)strtoul( &buffer[9], 0, 16 ) - address;
					break;
				}
			}
			fclose(fp);
		}
		return address;
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
			(*(char*)p) = 0x00;
			(*(char*)(p + 1)) = 0x46;
		}
		cacheflush(CLEAR_BIT0(addr), CLEAR_BIT0(addr) + count * 2, 0);
	}
	void JMP(uintptr_t addr, uintptr_t dest)
	{
		uint32_t newDest = ((dest - addr - 4) >> 12) & 0x7FF | 0xF000 |
		                   ((((dest - addr - 4) >> 1) & 0x7FF | 0xB800) << 16);
		write(addr, (uintptr_t)&newDest, 4);
	}
	void RET(uintptr_t addr)
	{
		write(addr, (uintptr_t)"\xF7\x46", 2);
	}
	bool hookInternal(void* addr, void* func, void** original)
	{
		if (addr == NULL || func == NULL) return false;
		unprotect((uintptr_t)addr);
		#ifdef __arm__
			return MSHookFunction(addr, func, original);
		#elif defined __aarch64__
			return A64HookFunction(addr, func, original);
		#endif
	}
	void hookPLTInternal(void* addr, void* func, void** original)
	{
		if (addr == NULL || func == NULL) return;
		unprotect((uintptr_t)addr);
		if(original != NULL)
			*((uintptr_t*)original) = *(uintptr_t*)addr;
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
}
