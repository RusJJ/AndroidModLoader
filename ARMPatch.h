#include <unistd.h>
#include <vector>
#include <dlfcn.h>
#include <sys/mman.h>

/* Just a hook declaration */
#define DECL_HOOK(_ret, _name, ...)                             \
    _ret (*_name)(__VA_ARGS__);	                                \
	_ret HookOf_##_name(__VA_ARGS__)
/* Just a hook declaration with return type = void */
#define DECL_HOOKv(_name, ...)                                  \
    void (*_name)(__VA_ARGS__);	                                \
	void HookOf_##_name(__VA_ARGS__)
/* Just a hook of a function */
#define HOOK(_name, _fnAddr)                                    \
    ARMPatch::hook((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));
/* Just a hook of a function located in PLT section (by address!) */
#define HOOKPLT(_name, _fnAddr)                                 \
    ARMPatch::hookPLT((void*)(_fnAddr), (void*)(&HookOf_##_name), (void**)(&_name));
/* Clear a bit of an address! */
#define CLEAR_BIT0(addr) (addr & 0xFFFFFFFE)

struct bytePattern
{
	struct byteEntry
	{
		uint8_t nValue;
		bool bUnknown;
	};
	std::vector<byteEntry> vBytes;
};

namespace ARMPatch
{
	/*
		Get library's start address
		soLib - name of a loaded library
	*/
	uintptr_t getLib(const char* soLib);
	/*
		Get library's end address
		soLib - name of a loaded library
	*/
	uintptr_t getLibLength(const char* soLib);
	/*
		Get library's function address by symbol (__unwind???)
		handle - HANDLE (NOTICE THIS!!!) of a library (u can obtain it using dlopen)
		sym - name of a function
	*/
	uintptr_t getSym(void* handle, const char* sym);
	/*
		Get library's function address by symbol (__unwind???)
		libAddr - ADDRESS (NOTICE THIS!!!) of a library (u can obtain it using getLib)
		sym - name of a function
		@XMDS requested this
	*/
	uintptr_t getSym(uintptr_t libAddr, const char* sym);
	
	/*
		Reprotect memory to allow reading/writing/executing
		addr - address
	*/
	int unprotect(uintptr_t addr, size_t len = PAGE_SIZE);
	
	/*
		Write to memory (reprotects it)
		dest - where to start?
		src - address of an info to write
		size - size of an info
	*/
	void write(uintptr_t dest, uintptr_t src, size_t size);
	
	/*
		Read memory (reprotects it)
		src - where to read from?
		dest - where to write a readed info?
		size - size of an info
	*/
	void read(uintptr_t src, uintptr_t dest, size_t size);
	
	/*
		Place NotOperator instruction (reprotects it)
		addr - where to put
		count - how much times to put
	*/
	void NOP(uintptr_t addr, size_t count = 1);
	
	/*
		Place JUMP instruction (reprotects it)
		addr - where to put
		dest - Jump to what?
	*/
	void JMP(uintptr_t addr, uintptr_t dest);
	
	/*
		Place RET instruction (RETURN, function end, reprotects it)
		addr - where to put
	*/
	void RET(uintptr_t addr);
	
	/*
		ByteScanner
		pattern - pattern.
		soLib - library's name
	*/
	uintptr_t getAddressFromPattern(const char* pattern, const char* soLib);
	
	/*
		Cydia's Substrate / Rprop's Inline Hook (use hook instead of hookInternal, ofc reprotects it!)
		addr - what to hook?
		func - Call that function instead of an original
		original - Original function!
	*/
	bool hookInternal(void* addr, void* func, void** original);
	template<class A, class B, class C>
	bool hook(A addr, B func, C original)
	{
		return hookInternal((void*)addr, (void*)func, (void**)original);
	}
	template<class A, class B>
	bool hook(A addr, B func)
	{
		return hookInternal((void*)addr, (void*)func, (void**)NULL);
	}
	
	/*
		A simple hook of a PLT-section functions (use hookPLT instead of hookPLTInternal, ofc reprotects it!)
		addr - what to hook?
		func - Call that function instead of an original
		original - Original function!
	*/
	void hookPLTInternal(void* addr, void* func, void** original);
	template<class A, class B, class C>
	void hookPLT(A addr, B func, C original)
	{
		hookPLTInternal((void*)addr, (void*)func, (void**)original);
	}
	template<class A, class B>
	void hookPLT(A addr, B func)
	{
		hookPLTInternal((void*)addr, (void*)func, (void**)NULL);
	}
}
