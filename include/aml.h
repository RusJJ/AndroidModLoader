#include <mod/iaml.h>

class AML : public IAML
{
public:
    /* AML 1.0.0.0 */
    const char* GetCurrentGame();
    const char* GetConfigPath();
    bool        HasMod(const char* szGUID);
    bool        HasModOfVersion(const char* szGUID, const char* szVersion);
    uintptr_t   GetLib(const char* szLib);
    uintptr_t   GetSym(void* handle, const char* sym);
    bool        Hook(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    void        HookPLT(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    int         Unprot(uintptr_t handle, size_t len = PAGE_SIZE);
    void        Write(uintptr_t dest, uintptr_t src, size_t size);
    void        Read(uintptr_t src, uintptr_t dest, size_t size);
    void        PlaceNOP(uintptr_t addr, size_t count = 1);
    void        PlaceJMP(uintptr_t addr, uintptr_t dest);
    void        PlaceRET(uintptr_t addr);
    /* AML 1.0.0.4 */
    const char* GetDataPath();
    /* AML 1.0.0.5 */
    const char* GetAndroidDataPath();
    uintptr_t   GetSym(uintptr_t libAddr, const char* sym);
    /* AML 1.0.0.6 */
    uintptr_t   GetLibLength(const char* szLib);
    void        Redirect(uintptr_t addr, uintptr_t to);
    void        PlaceBL(uintptr_t addr, uintptr_t dest);
    void        PlaceBLX(uintptr_t addr, uintptr_t dest);
    uintptr_t   PatternScan(const char* pattern, const char* soLib);
    uintptr_t   PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
    /* AML 1.0.1 */
    void        HookVtableFunc(void* ptr, unsigned int funcNum, void* fnAddress, void** orgFnAddress = (void**)0, bool instantiate = false);
    bool        IsGameFaked();
    const char* GetRealCurrentGame();
};
