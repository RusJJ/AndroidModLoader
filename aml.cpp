#include <include/aml.h>
#include <ARMPatch.h>
#include <include/modslist.h>

extern char g_szAppName[0xFF];
extern char g_szCfgPath[0xFF];
extern char g_szAndroidDataDir[0xFF];
extern const char* g_szDataDir;
const char* AML::GetCurrentGame()
{
    return g_szAppName;
}

const char* AML::GetConfigPath()
{
    return g_szCfgPath;
}

const char* AML::GetDataPath()
{
    return g_szDataDir;
}

const char* AML::GetAndroidDataPath()
{
    return g_szAndroidDataDir;
}

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

uintptr_t AML::GetSym(void* handle, const char* sym)
{
    return ARMPatch::getSym(handle, sym);
}

uintptr_t AML::GetSym(uintptr_t libAddr, const char* sym)
{
    return ARMPatch::getSym(libAddr, sym);
}

bool AML::Hook(void* handle, void* fnAddress, void** orgFnAddress)
{
    return ARMPatch::hookInternal(handle, fnAddress, orgFnAddress);
}

void AML::HookPLT(void* handle, void* fnAddress, void** orgFnAddress)
{
    ARMPatch::hookPLTInternal(handle, fnAddress, orgFnAddress);
}

int AML::Unprot(uintptr_t handle, size_t len)
{
    return ARMPatch::unprotect(handle, len);
}

void AML::Write(uintptr_t dest, uintptr_t src, size_t size)
{
    ARMPatch::write(dest, src, size);
}

void AML::Read(uintptr_t src, uintptr_t dest, size_t size)
{
    ARMPatch::read(src, dest, size);
}

void AML::PlaceNOP(uintptr_t addr, size_t count)
{
    ARMPatch::NOP(addr, count);
}

void AML::PlaceJMP(uintptr_t addr, uintptr_t dest)
{
    ARMPatch::B(addr, dest);
}

void AML::PlaceRET(uintptr_t addr)
{
    ARMPatch::RET(addr);
}

uintptr_t AML::GetLibLength(const char* szLib)
{
    return ARMPatch::getLibLength(szLib);
}

void AML::Redirect(uintptr_t addr, uintptr_t to)
{
    ARMPatch::redirect(addr, to);
}

void AML::PlaceBL(uintptr_t addr, uintptr_t dest)
{
    ARMPatch::BL(addr, dest);
}

void AML::PlaceBLX(uintptr_t addr, uintptr_t dest)
{
    ARMPatch::BLX(addr, dest);
}

uintptr_t AML::PatternScan(const char* pattern, const char* soLib)
{
    return ARMPatch::getAddressFromPattern(pattern, soLib);
}

uintptr_t AML::PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen)
{
    return ARMPatch::getAddressFromPattern(pattern, libStart, scanLen);
}

static AML amlLocal;
IAML* aml = (IAML*)&amlLocal;