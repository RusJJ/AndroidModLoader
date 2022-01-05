#include <mod/iaml.h>

class AML : public IAML
{
public:
    const char* GetCurrentGame();
    const char* GetConfigPath();
    bool HasMod(const char* szGUID);
    bool HasModOfVersion(const char* szGUID, const char* szVersion);
    uintptr_t GetLib(const char* szLib);
    uintptr_t GetSym(void* handle, const char* sym);
    bool Hook(void* handle, void* fnAddress, void** orgFnAddress = nullptr);
    void HookPLT(void* handle, void* fnAddress, void** orgFnAddress = nullptr);
    int Unprot(uintptr_t handle, size_t len = PAGE_SIZE);
    void Write(uintptr_t dest, uintptr_t src, size_t size);
    void Read(uintptr_t src, uintptr_t dest, size_t size);
    void PlaceNOP(uintptr_t addr, size_t count = 1);
    void PlaceJMP(uintptr_t addr, uintptr_t dest);
    void PlaceRET(uintptr_t addr);
    const char* GetDataPath();
    const char* GetAndroidDataPath();
};