#include <aml.h>
#include <ARMPatch.h>
#include <modslist.h>

bool AML::HasMod(const char* szGUID)
{
    return modlist->HasMod(szGUID);
}

bool AML::HasModOfVersion(const char* szGUID, const char* szVersion)
{
    return modlist->HasModOfVersion(szGUID, szVersion);
}

uintptr_t AML::GetLib(const char* szLib)
{
    return ARMPatch::getLib(szLib);
}

void AML::Hook(void* handle, void* fnAddress, void** orgFnAddress)
{
    ARMPatch::hook(handle, fnAddress, orgFnAddress);
}

static AML amlLocal;
IAML* aml = (IAML*)&amlLocal;