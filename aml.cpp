#include <include/aml.h>
#include <armpatch_src/ARMPatch.h>
#include <vtable_hooker.h>
#include <include/modslist.h>

extern char g_szAppName[0xFF], g_szFakeAppName[0xFF];
extern char g_szCfgPath[0xFF];
extern char g_szAndroidDataDir[0xFF];
extern const char* g_szDataDir;

inline bool HasFakeAppName()
{
    return (g_szFakeAppName[0] != 0 && strlen(g_szFakeAppName) > 5);
}

const char* AML::GetCurrentGame()
{
    return HasFakeAppName() ? g_szFakeAppName : g_szAppName;
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
    return ARMPatch::GetLib(szLib);
}

uintptr_t AML::GetSym(void* handle, const char* sym)
{
    return ARMPatch::GetSym(handle, sym);
}

uintptr_t AML::GetSym(uintptr_t libAddr, const char* sym)
{
    return ARMPatch::GetSym(libAddr, sym);
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
    return ARMPatch::Unprotect(handle, len);
}

void AML::Write(uintptr_t dest, uintptr_t src, size_t size)
{
    ARMPatch::Write(dest, src, size);
}

void AML::Read(uintptr_t src, uintptr_t dest, size_t size)
{
    ARMPatch::Read(src, dest, size);
}

void AML::PlaceNOP(uintptr_t addr, size_t count)
{
    ARMPatch::WriteNOP(addr, count);
}

void AML::PlaceJMP(uintptr_t addr, uintptr_t dest)
{
    ARMPatch::WriteB(addr, dest);
}

void AML::PlaceRET(uintptr_t addr)
{
    ARMPatch::WriteRET(addr);
}

uintptr_t AML::GetLibLength(const char* szLib)
{
    return ARMPatch::GetLibLength(szLib);
}

void AML::Redirect(uintptr_t addr, uintptr_t to)
{
    ARMPatch::Redirect(addr, to);
}

void AML::PlaceBL(uintptr_t addr, uintptr_t dest)
{
    ARMPatch::WriteBL(addr, dest);
}

void AML::PlaceBLX(uintptr_t addr, uintptr_t dest)
{
    ARMPatch::WriteBLX(addr, dest);
}

uintptr_t AML::PatternScan(const char* pattern, const char* soLib)
{
    return ARMPatch::GetAddressFromPattern(pattern, soLib);
}

uintptr_t AML::PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen)
{
    return ARMPatch::GetAddressFromPattern(pattern, libStart, scanLen);
}

void AML::HookVtableFunc(void* ptr, unsigned int funcNum, void* func, void** original, bool instantiate)
{
    HookVtableFunc(ptr, funcNum, func, original, instantiate);
}

bool AML::IsGameFaked()
{
    return HasFakeAppName();
}

const char* AML::GetRealCurrentGame()
{
    return g_szAppName;
}

static AML amlLocal;
IAML* aml = (IAML*)&amlLocal;
