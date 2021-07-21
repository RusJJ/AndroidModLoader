#include <mod/iaml.h>

class AML : public IAML
{
public:
    bool HasMod(const char* szGUID);
    bool HasModOfVersion(const char* szGUID, const char* szVersion);
    uintptr_t GetLib(const char* szLib);
    void Hook(void* handle, void* fnAddress, void** orgFnAddress = nullptr);
};