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
    bool        HookPLT(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    int         Unprot(uintptr_t handle, size_t len = PAGE_SIZE);
    void        Write(uintptr_t dest, uintptr_t src, size_t size);
    void        Read(uintptr_t src, uintptr_t dest, size_t size);
    int         PlaceNOP(uintptr_t addr, size_t count = 1);
    int         PlaceJMP(uintptr_t addr, uintptr_t dest);
    int         PlaceRET(uintptr_t addr);
    /* AML 1.0.0.4 */
    const char* GetDataPath();
    /* AML 1.0.0.5 */
    const char* GetAndroidDataPath();
    uintptr_t   GetSym(uintptr_t libAddr, const char* sym);
    /* AML 1.0.0.6 */
    uintptr_t   GetLibLength(const char* szLib);
    int         Redirect(uintptr_t addr, uintptr_t to);
    void        PlaceBL(uintptr_t addr, uintptr_t dest);
    void        PlaceBLX(uintptr_t addr, uintptr_t dest);
    uintptr_t   PatternScan(const char* pattern, const char* soLib);
    uintptr_t   PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
    /* AML 1.0.1 */
    void        PatchForThumb(bool forThumb);
    const char* GetFeatures();
    void        AddFeature(const char* feature); // Not in interface
    void        HookVtableFunc(void* ptr, unsigned int funcNum, void* fnAddress, void** orgFnAddress = NULL, bool instantiate = false);
    bool        IsGameFaked();
    const char* GetRealCurrentGame();
    void*       GetLibHandle(const char* soLib);
    void*       GetLibHandle(uintptr_t addr);
    // xDL
    bool        IsCorrectXDLHandle(void* ptr);
    uintptr_t   GetLibXDL(void* ptr);
    uintptr_t   GetAddrBaseXDL(uintptr_t addr);
    size_t      GetSymSizeXDL(void* ptr);
    const char* GetSymNameXDL(void* ptr);
    /* AML 1.0.2 */
    void        ShowToast(bool longerDuration, const char* fmt, ...);
    bool        DownloadFile(const char* url, const char* saveto);
    bool        DownloadFileToData(const char* url, char* out, size_t outLen);
    void        FileMD5(const char* path, char* out, size_t out_len);
    int         GetModsLoadedCount();
    JNIEnv*     GetJNIEnvironment();
    jobject     GetAppContextObject();
    /* AML 1.0.2.1 */
    bool        HasModOfBiggerVersion(const char* szGUID, const char* szVersion);
    /* AML 1.0.4 */
    void        HookVtableFunc(void* ptr, unsigned int funcNum, unsigned int count, void* fnAddress, void** orgFnAddress = NULL, bool instantiate = false);
    int         PlaceNOP4(uintptr_t addr, size_t count = 1);
    const char* GetAndroidDataRootPath();
    // GlossHook
    bool        HookB(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    bool        HookBL(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    bool        HookBLX(void* handle, void* fnAddress, void** orgFnAddress = NULL);
    /* AML 1.2 */
    void        MLSSaveFile();
    bool        MLSHasValue(const char* key);
    void        MLSDeleteValue(const char* key);
    void        MLSSetInt(const char* key, int32_t val);
    void        MLSSetFloat(const char* key, float val);
    void        MLSSetInt64(const char* key, int64_t val);
    void        MLSSetStr(const char* key, const char *val);
    bool        MLSGetInt(const char* key, int32_t *val);
    bool        MLSGetFloat(const char* key, float *val);
    bool        MLSGetInt64(const char* key, int64_t *val);
    bool        MLSGetStr(const char* key, char *val, size_t len);
    /* AML 1.2.1 */
    bool        IsThumbAddr(uintptr_t addr);
    uintptr_t   GetBranchDest(uintptr_t addr);
    /* AML 1.2.2 */
    int         GetAndroidVersion();
    bool        CopyFile(const char* file, const char* dest);
    void        RedirectReg(uintptr_t addr, uintptr_t to, bool doShortHook, GlossRegisters::e_reg targetReg);
    bool        HasAddrExecFlag(uintptr_t addr);
    void        ToggleHook(PHookHandle hook, bool enable);
    void        DeHook(PHookHandle hook);
    PHookHandle HookInline(void* fnAddress, HookWithRegistersFn newFn, bool doShortHook = false);
    /* AML 1.2.3 */
    bool        HasFastmanAPKModified();
    const char* GetInternalPath();
    const char* GetInternalModsPath();
    /* AML 1.3.0 */
    JavaVM*     GetJavaVM();
    jobject     GetCurrentContext();
    void        DoVibro(int msTime);
    void        DoVibro(jlong* pattern, int patternItems);
    void        CancelVibro();
    float       GetBatteryLevel();
    /* AML 1.4.0 */
    const char* GetNativeLibsPath();
    bool        PushToJavaUIThread(void (*fn)(void*), void* data);
    AAssetManager* GetAssetManager();
    void*       OpenAsset(const char* path, int mode = AASSET_MODE_BUFFER);
    void        CloseAsset(void* asset);
    size_t      GetAssetSize(void* asset);
    const void* GetAssetBuffer(void* asset);
    void        ReadAsset(void* asset, void* buf, size_t count);
    jobject     InjectSmaliDEX(const uint8_t* dexBytes, size_t dexSize, const char* classToInit);
    jobject     GetInjectedSmaliDEX(const char* className);
    void        GetDisplaySize(int* w = NULL, int* h = NULL);
    uintptr_t   AllocateMemory(size_t size, bool executable = false);
    bool        FreeMemory(uintptr_t pointer);
    uintptr_t   ReadPointerChain(uintptr_t baseAddr, std::initializer_list<int> offsets);
    std::vector<uintptr_t> FindAllPatterns(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
    bool        ComparePattern(uintptr_t addr, const char* pattern);
    void        ShowDialog(const char* title, const char* message, const char* buttonText = NULL);
    bool        FileExists(const char* path);
    size_t      FileSize(const char* path);
    bool        IsDirectory(const char* path);
    bool        RemoveFile(const char* path);
    bool        RemoveDir(const char* path, bool recursive = false);
    bool        CreateDirRecursive(const char* path);
    jobject     GetCurrentActivity();
};

extern char g_szAMLFeatures[2048];
extern AML* g_pAML;
extern int g_nAndroidSDKVersion;