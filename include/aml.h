#include <mod/iaml.h>

class AML : public IAML
{
public:
    const char* GetCurrentGame();
    const char* GetConfigPath();
    bool HasMod(const char* szGUID);
    bool HasModOfVersion(const char* szGUID, const char* szVersion);
    uintptr_t GetLib(const char* szLib);
    uintptr_t GetSym(uintptr_t handle, const char* sym);
    void Hook(void* handle, void* fnAddress, void** orgFnAddress = nullptr);
    void HookPLT(void* handle, void* fnAddress, void** orgFnAddress = nullptr);
    int Unprot(uintptr_t handle, size_t len = PAGE_SIZE);
    void Write(uintptr_t dest, uintptr_t src, size_t size);
};