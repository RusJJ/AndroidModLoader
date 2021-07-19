#include <unistd.h>
#include <vector>
#include <dlfcn.h>
#include <sys/mman.h>

#define DECLFN(_name, _ret, ...)			\
		_ret (*orgnl_##_name)(__VA_ARGS__);	\
		_ret hooked_##_name(__VA_ARGS__)
#define USEFN(_name)	\
		&hooked_##_name, &orgnl_##_name
#define CALLFN(_name, ...) orgnl_##_name(__VA_ARGS__)

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
		handle - handle of a library (getLib for example)
		sym - name of a function
	*/
	uintptr_t getSym(uintptr_t handle, const char* sym);
	
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
		Cydia's Substrate (use hook instead of hookInternal, ofc reprotects it!)
		addr - what to hook?
		func - Call that function instead of an original
		original - Original function!
	*/
	void hookInternal(void* addr, void* func, void** original);
	template<class A, class B, class C>
	void hook(A addr, B func, C original)
	{
		hookInternal((void*)addr, (void*)func, (void**)original);
	}
	template<class A, class B>
	void hook(A addr, B func)
	{
		hookInternal((void*)addr, (void*)func, (void**)NULL);
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
