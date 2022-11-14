#define MAX_VTABLE_FUNCS 4096 // This is not a count of max instantiated!

void HookVtableFunc(void* ptr, unsigned int funcNum, void* fnAddress, void** orgFnAddress = (void**)0, bool instantiate = false);
