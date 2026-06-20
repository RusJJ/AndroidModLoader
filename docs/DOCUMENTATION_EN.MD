# AndroidModLoader (AML) — Mod Development Documentation

> Repository: https://github.com/RusJJ/AndroidModLoader
> Example mods: https://github.com/AndroidModLoader?tab=repositories
> Discord: https://discord.gg/2MY7W39kBg
>
> This document is written against the real mod headers (`amlmod.h`, `iaml.h`, `interface.h`, `config.h`, `icfg.h`, `logger.h`, `localref.h`, `scopeguard.h`, `listitem.h`, `il2cpp.h`). Header set corresponds to **IAML_VER `01040000` (AML 1.4.0)**.

---

## 1. Overview and load order

AML loads native `.so` mods into a target app and exposes a Source-Engine–style **interface system** for hooking, memory patching, config, JNI access, and mod-to-mod APIs.

**Important load-order fact:** AML is loaded **after** the game's main library is already loaded (after `JNI_OnLoad` and the game's own init). This means by the time your `OnModPreLoad` / `OnModLoad` run, `aml->GetLib("libGame.so")` already returns a valid base address. You do not hook `JNI_OnLoad`; you patch the already-mapped library.

ABI is ARM-only. The headers define exactly one of:
```cpp
#ifdef AML32   // armeabi-v7a (__arm__)
#else          // AML64, arm64-v8a (__aarch64__)
#endif
```
Helper macro `BYBIT(v32, v64)` picks a value per ABI:
```cpp
uintptr_t off = pGame + BYBIT(0x123456, 0x1234567);
```

### Mods location
```
/sdcard/Android/data/<game.package>/mods/      (external)
/sdcard/AMLMods/<game>/                          (internal default, see GetInternalModsPath)
```

---

## 2. Building a mod

A mod is an NDK shared library. You do **not** link against AML — `interface.h` resolves AML at runtime via `dlopen("libAML.so")`. Just compile against the AML mod headers (placed under `include/mod/`).

`Application.mk`:
```makefile
APP_ABI := armeabi-v7a arm64-v8a
APP_PLATFORM := android-21
APP_STL := c++_static
APP_CPPFLAGS += -std=c++17     # required for the JNI template helpers
```

`Android.mk`:
```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE     := mymod
LOCAL_SRC_FILES  := main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_LDLIBS     := -llog
# Optional: disable all logging to shrink the mod
# LOCAL_CXXFLAGS += -DNOLOGGING
include $(BUILD_SHARED_LIBRARY)
```
Build with `ndk-build`.

---

## 3. Declaring a mod

Include the core header, then declare metadata with one macro.

```cpp
#include <mod/amlmod.h>   // pulls in iaml.h -> interface.h, jni.h, etc.
#include <mod/logger.h>
#include <mod/config.h>   // only if you need cfg

MYMOD(net.author.mymod, My Cool Mod, 1.0, AuthorName)
```

### What `MYMOD` generates for you
The macro (`amlmod.h`) expands to:
- a static `ModInfo modinfoLocal(...)` and global `ModInfo* modinfo`,
- the export `__GetModInfo()` AML calls to read your metadata,
- the global `IAML* aml`,
- a static init stub (init priority 101) that runs **before** your callbacks and does:
  ```cpp
  aml = (IAML*)GetInterface("AMLInterface");
  ```

So `aml` is ready by the time any of your entry points run. You never fetch it manually.

### Macro variants
```cpp
MYMOD(_guid, _name, _version, _author)
    // base; creates modinfo + aml

MYMODCFG(_guid, _name, _version, _author)
    // MYMOD + a global `Config* cfg` whose .ini is named after _guid

MYMODCFGNAME(_guid, _name, _version, _author, _cfgname)
    // MYMOD + `cfg` whose .ini is named after _cfgname (custom file name)
```
Arguments are unquoted tokens (the macro stringifies them). The GUID is lowercased automatically. Version is parsed into `major.minor.revision.build`.

### Restrict to a specific game (optional)
```cpp
NEEDGAME(com.rockstargames.gtasa)   // mod only loads inside this package
```

### Reading your own metadata
`modinfo` exposes: `GUID()`, `Name()`, `VersionString()`, `Author()`, `Major()`, `Minor()`, `Revision()`, `Build()`.

---

## 4. Dependencies

Declared with a block; AML uses it for load ordering and hard-failing on missing required mods.

```cpp
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)      // require AML >= 1.4.0
    ADD_DEPENDENCY_VER(net.rusjj.cleolib, 2.0.1.6)
    ADD_DEPENDENCY(net.rusjj.basslib)             // any version
END_DEPLIST()
```
- `ADD_DEPENDENCY(guid)` — require a mod, any version.
- `ADD_DEPENDENCY_VER(guid, version)` — require a mod at/above a version.

**Soft dependency:** don't list it; instead query at runtime with `aml->HasMod(...)` / `aml->HasModOfVersion(...)` / `aml->HasModOfBiggerVersion(...)`, or just try `GetInterface(...)` and handle `NULL`.

---

## 5. Entry points (callbacks)

Each macro expands to an `extern "C"` export AML looks up. Define what you need.

```cpp
ON_MOD_PRELOAD()      // void OnModPreLoad()      — earliest; lib already mapped
ON_MOD_LOAD()         // void OnModLoad()         — main entry: hooks, config, symbols
ON_ALL_MODS_LOAD()    // void OnAllModsLoaded()   — after every mod finished loading
ON_MOD_UNLOAD()       // void OnModUnload()       — cleanup (not guaranteed to run)
ON_GAME_CRASH()       // void OnGameCrash(const char* library, int sig, int code,
                      //                  uintptr_t libaddr, mcontext_t* mcontext)
ON_NEW_INTERFACE()    // void OnInterfaceAdded(const char* name, const void* ptr)
UPDATER_URL()         // const char* OnUpdaterURLRequested()  — return your update URL
```

Inside them you have `aml`, `logger`, and (if `MYMODCFG`) `cfg` ready.

Use `ON_ALL_MODS_LOAD` when you need another mod's interface that might register late, or use `ON_NEW_INTERFACE` to react the moment a named interface appears.

```cpp
ON_MOD_PRELOAD()
{
    logger->SetTag("My Cool Mod");
}

ON_MOD_LOAD()
{
    uintptr_t pGame = aml->GetLib("libGTASA.so");
    if(!pGame) { logger->Error("game lib missing"); return; }
    // hooks / patches / config here
}
```

---

## 6. The interface system: `GetInterface` / `RegisterInterface`

This is AML's core extensibility mechanism. **These are free-standing functions defined in `interface.h`, not methods of `aml`.** Internally they `dlopen("libAML.so")` and call AML's `GetInterface` / `CreateInterface` exports.

```cpp
void* GetInterface(const char* szInterfaceName);              // fetch a registered interface
void  RegisterInterface(const char* szInterfaceName, void* p);// expose your own
```

An interface is just a pointer (usually to a struct/class of function pointers or a vtable class) stored under a **versioned string name**. The name *is* the ABI contract — include a version number and bump it when the layout changes.

`aml` itself is obtained this way: the `MYMOD` macro calls `GetInterface("AMLInterface")` for you.

### Convenience macro
```cpp
#define WRAP_INTERFACE(name, var)  RegisterInterface(#name, var)
WRAP_INTERFACE(IMyApi001, &g_MyApi);   // == RegisterInterface("IMyApi001", &g_MyApi)
```

### A) Providing an interface
Ship a header so consumers know the layout, fill a struct, register it.

```cpp
// imyapi.h  (shared with consumers)
struct IMyApi
{
    int  (*Add)(int a, int b);
    void (*DoThing)(const char* what);
};

// main.cpp
static int  MyApi_Add(int a, int b) { return a + b; }
static void MyApi_DoThing(const char* w) { logger->Info("doing %s", w); }
static IMyApi g_MyApi = { MyApi_Add, MyApi_DoThing };

MYMOD(net.author.myapi, My API Provider, 1.0, AuthorName)

ON_MOD_LOAD()
{
    RegisterInterface("IMyApi001", &g_MyApi);
}
```

A vtable-class interface also works (consumers call virtuals directly):
```cpp
class IMyApi { public: virtual int Add(int,int)=0; virtual void DoThing(const char*)=0; };
```

### B) Consuming an interface
```cpp
#include <ibass.h>     // header shipped by the BASS mod
IBASS* BASS = NULL;

MYMOD(net.author.user, BASS User, 1.0, AuthorName)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.basslib, 1.2)   // ensures BASS loads first
END_DEPLIST()

ON_MOD_LOAD()
{
    BASS = (IBASS*)GetInterface("IBASS001");
    if(!BASS) { logger->Error("IBASS001 not found"); return; }
    // BASS->...
}
```

### C) Reacting to late interfaces
```cpp
ON_NEW_INTERFACE()   // OnInterfaceAdded(name, ptr)
{
    if(!strcmp(name, "IBASS001"))
        BASS = (IBASS*)ptr;
}
```

### Notes
- If you hard-depend on the provider (`ADD_DEPENDENCY*`), the interface exists by your `OnModLoad`. For optional providers, check `NULL`.
- Keep the version suffix in the name (`IBASS001`) so mismatched clients fail cleanly instead of calling a wrong layout.

---

## 7. The `aml->` interface (`IAML`)

`aml` is the AML core. Below are the methods grouped by purpose. Signatures are exact from `iaml.h`. Availability is annotated with the AML version that introduced each block.

### Game / mod info
```cpp
const char* GetCurrentGame();                 // package of the (possibly faked) current game
const char* GetRealCurrentGame();             // real package
bool        IsGameFaked();
bool        HasMod(const char* guid);
bool        HasModOfVersion(const char* guid, const char* version);
bool        HasModOfBiggerVersion(const char* guid, const char* version);
int         GetModsLoadedCount();
int         GetFailedModsCount();
void        ListMods(ListModsCallback cb, void* data = NULL, bool startWithLatest = false);
const char* GetFeatures();                    // build feature string
```

### Libraries and symbols
```cpp
uintptr_t   GetLib(const char* szLib);                 // base address of a loaded .so
uintptr_t   GetLibLength(const char* szLib);           // mapped size
void*       GetLibHandle(const char* soLib);           // dlopen-style handle
void*       GetLibHandle(uintptr_t addr);
uintptr_t   GetSym(void* handle, const char* sym);     // resolve by handle
uintptr_t   GetSym(uintptr_t libAddr, const char* sym);// resolve by library ADDRESS
```
xDL helpers (always present, return 0 if xDL unused):
```cpp
bool IsCorrectXDLHandle(void*); uintptr_t GetLibXDL(void*);
uintptr_t GetAddrBaseXDL(uintptr_t); size_t GetSymSizeXDL(void*);
const char* GetSymNameXDL(void*);
```
Symbol-assignment helper macros:
```cpp
SET_TO(var, addr);              // *(void**)&var = (void*)addr
SET_TO_PTR(var, ptraddr);       // dereference then assign
SETSYM_TO(var, lib, "sym");     // var = aml->GetSym(lib, "sym")
SETSYM_TO_PTR(var, lib, "sym");
AS_ADDR(var);                   // *(uintptr_t*)&var
```

### Memory: unprotect / write / read
```cpp
int  Unprot(uintptr_t addr, size_t len = PAGE_SIZE);
void Write(uintptr_t dest, uintptr_t src, size_t size);
void Read (uintptr_t src,  uintptr_t dest, size_t size);
```
Typed inline helpers (no manual size):
```cpp
Write8/16/32/64(addr, v);  WriteFloat/WriteDouble(addr, v);
WriteAddr(addr, ptr);      Write(dest, "string");        // and Write(dest, str, size)
Read8/16/32/64(addr) -> value;  ReadFloat/ReadDouble(addr) -> value;
bool PatchMemory(addr, { 0x00, 0x10, ... });             // unprot + write a byte list
```
Allocation / pointer chains / scanning:
```cpp
uintptr_t AllocateMemory(size_t size, bool executable = false);
bool      FreeMemory(uintptr_t ptr);
uintptr_t ReadPointerChain(uintptr_t base, std::initializer_list<int> offsets);
uintptr_t PatternScan(const char* pattern, const char* soLib);
uintptr_t PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
std::vector<uintptr_t> FindAllPatterns(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
bool      ComparePattern(uintptr_t addr, const char* pattern);
```

### Instruction patching (ARM)
```cpp
int  PlaceNOP(uintptr_t addr, size_t count = 1);
int  PlaceNOP4(uintptr_t addr, size_t count = 1);   // 4-byte NOPs
int  PlaceJMP(uintptr_t addr, uintptr_t dest);      // PlaceB is an alias of PlaceJMP
int  PlaceRET(uintptr_t addr);
void PlaceBL (uintptr_t addr, uintptr_t dest);
void PlaceBLX(uintptr_t addr, uintptr_t dest);
int  Redirect(uintptr_t addr, uintptr_t to);        // jump keeping stack+registers
void PatchForThumb(bool forThumb);                  // thumb vs arm patching mode
bool IsThumbAddr(uintptr_t addr);
uintptr_t GetBranchDest(uintptr_t addr);
bool HasAddrExecFlag(uintptr_t addr);
// arm64 register-preserving redirect:
void RedirectReg(uintptr_t addr, uintptr_t to, bool doShortHook = false,
                 GlossRegisters::e_reg targetReg = GlossRegisters::X16);
```

### Hooking (low level — prefer the macros in §8)
```cpp
bool Hook   (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookPLT(void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookB  (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBL (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBLX(void* targetAddr, void* newFn, void** orgFn = NULL);
void HookVtableFunc(void* ptr, unsigned funcNum, void* newFn, void** orgFn = NULL, bool instantiate = false);
void HookVtableFunc(void* ptr, unsigned funcNum, unsigned count, void* newFn, void** orgFn = NULL, bool instantiate = false);
// register-level inline hook:
PHookHandle HookInline(void* fnAddress, HookWithRegistersFn newFn, bool doShortHook = false);
void ToggleHook(PHookHandle hook, bool enable);
void DeHook(PHookHandle hook);
// vtable helpers:
void** GetVtable(void* ptr);  void SetVtable(void* ptr, void** vtable);
```

### Paths
```cpp
const char* GetDataPath();            // /data/data/<pkg>/...
const char* GetConfigPath();
const char* GetAndroidDataPath();     // /sdcard/Android/data/<pkg>/files/...
const char* GetAndroidDataRootPath(); // /sdcard/Android/data/<pkg>/... (no /files/)
const char* GetInternalPath();        // /sdcard/
const char* GetInternalModsPath();    // /sdcard/AMLMods/<game>/
const char* GetNativeLibsPath();      // /data/app/<...>/lib/arm*/
```

### Files
```cpp
bool   FileExists(const char* path);   size_t FileSize(const char* path);
bool   IsDirectory(const char* path);  bool   IsFile(const char* path);
bool   RemoveFile(const char* path);   bool   RemoveDir(const char* path, bool recursive = false);
bool   CreateDirRecursive(const char* path);
bool   CopyFile(const char* file, const char* dest);
bool   MoveFile(const char* src, const char* dst);
time_t GetFileModTime(const char* path);
int    ListDir(const char* path, ListDirCallback cb, void* data);
int    ReadFileToBuffer(const char* path, char* out, size_t maxLen);
bool   WriteBufferToFile(const char* path, const void* buf, size_t len);
void   FileMD5(const char* path, char* out, size_t out_len);   // >= MINIMUM_MD5_BUF_SIZE (33)
void   DataMD5(void* data, size_t len, char* out, size_t out_len);
```

### Assets (APK assets)
```cpp
AAssetManager* GetAssetManager();
void*  OpenAsset(const char* path, int mode = AASSET_MODE_BUFFER);
void   CloseAsset(void* asset);
size_t GetAssetSize(void* asset);
const void* GetAssetBuffer(void* asset);
void   ReadAsset(void* asset, void* buf, size_t count);
```

### UI / device / system
```cpp
void  ShowToast(bool longerDuration, const char* fmt, ...);
void  ShowDialog(const char* title, const char* message, const char* buttonText = NULL, int styleResource = 0);
void  DoVibro(int msTime);                         // ~20ms = light
void  DoVibro(jlong* pattern, int patternItems);   // e.g. DEFAULT_VIBRATE_PATTERN, g_VibroPattern_Weak
void  CancelVibro();
float GetBatteryLevel();                           // 0.0 .. 100.0
void  GetDisplaySize(int* w = NULL, int* h = NULL);
int   GetAndroidVersion();                         // API level
int   GetAndroidSystemResID(const char* innerClass, const char* fieldName); // e.g. "style","Theme_DeviceDefault_Alert"
void  OpenURL(const char* url);
int   GetCPUCores();
bool  IsMainThread();
void  GetNewsString(char* buf, size_t len);
```

### Networking / downloads
```cpp
bool DownloadFile(const char* url, const char* saveto);
bool DownloadFileToData(const char* url, char* out, size_t outLen);
bool IsFileDownloadsEnabled();
int  GetDownloadTimeout();
int  GetLatestDownloadErrorCode();
```

### JNI / Java bridge
```cpp
JNIEnv*  GetJNIEnvironment();   JavaVM* GetJavaVM();
jobject  GetAppContextObject(); jobject GetCurrentContext();
jobject  GetCurrentActivity();
bool     PushToJavaUIThread(void (*fn)(void*), void* data = NULL);
jobject  CallStaticJavaMethod(const char* cls, const char* method, const char* sig, ...);
jobject  GetStaticJavaField(const char* cls, const char* field, const char* sig);
bool     SetStaticJavaField(const char* cls, const char* field, const char* sig, jobject value);
jobject  InjectSmaliDEX(const uint8_t* dexBytes, size_t dexSize, const char* classToInit);
jobject  GetInjectedSmaliDEX(const char* className);
```
Plus convenience templates (C++17, defined in `iaml.h`):
```cpp
T    CallJavaMethod<T>(jobject obj, const char* method, const char* sig, args...);  // auto Call*Method
auto CallVirtual<Ret>(void* instance, int index, args...);                          // call vtable[index]
```

### MLS — Mod Local Storage (persistent key/value)
A simple typed store separate from `.ini` config:
```cpp
void MLSSetInt/MLSSetFloat/MLSSetInt64/MLSSetStr(key, val);   void MLSSetBool(key, val);
bool MLSGetInt/MLSGetFloat/MLSGetInt64(key, &out);            bool MLSGetBool(key, &out);
bool MLSGetStr(key, buf, len);
bool MLSHasValue(key);   void MLSDeleteValue(key);   void MLSSaveFile();
bool IsMLSInManualSave();
```

---

## 8. Hooking functions (macros)

Declare a hook with a `DECL_HOOK*` macro, then install it with a `HOOK*` macro. The macro creates two things: a function pointer to the **original** (named exactly as you wrote) and your replacement body named `HookOf_<name>`. Call the original from inside your body using the name.

### Declaration macros
```cpp
DECL_HOOK(ret, name, args...)   // returns `ret`
DECL_HOOKv(name, args...)       // void
DECL_HOOKb(name, args...)       // bool
DECL_HOOKi(name, args...)       // int
DECL_HOOKp(name, args...)       // void*
```
`S`-prefixed variants (`SDECL_HOOK*`) make the pointers `static`; `SI`-prefixed (`SIDECL_HOOK*`) make them `static inline`.

### Install macros
```cpp
HOOK(name, fnAddr)                 // inline hook by address
HOOKSYM(name, libHandle, "symbol") // inline hook, resolves symbol for you
HOOKPLT(name, fnAddr)              // PLT/GOT hook
HOOKB(name, fnAddr)                // hook a B
HOOKBL(name, fnAddr)               // hook a BL
HOOKBLX(name, fnAddr)              // hook a BLX
HOOK_IL2CPP(name, methodInfo)      // hook via IL2CPP MethodInfo->methodPointer
```

### Example
```cpp
// void function
DECL_HOOKv(InitialiseRenderWare)
{
    InitialiseRenderWare();              // call original
    // ... your code ...
}

// int function with args
DECL_HOOK(int, CalcDamage, void* self, int raw)
{
    int dmg = CalcDamage(self, raw);     // original
    return dmg * 2;
}

ON_MOD_LOAD()
{
    uintptr_t pGame = aml->GetLib("libGTASA.so");
    HOOK(InitialiseRenderWare, pGame + BYBIT(0x111111, 0x2222222));
    HOOKSYM(CalcDamage, aml->GetLibHandle("libGTASA.so"), "_Z10CalcDamagePvi");
}
```

---

## 9. Configuration: `cfg->` (`Config`)

Available as the global `cfg` when you declared with `MYMODCFG`/`MYMODCFGNAME` and `#include <mod/config.h>`. **The default section is `"Preferences"`.**

### Fast getters (no allocation, no cleanup)
```cpp
const char* GetString(key, defValue, section = "Preferences");   // pointer is transient
int         GetInt   (key, defValue, section = "Preferences");
float       GetFloat (key, defValue, section = "Preferences");
bool        GetBool  (key, defValue, section = "Preferences");
rgba_t      GetColor (key, defValue, section = "Preferences");
// safe string copy into your buffer:
void        GetString(char* out, size_t outLen, key, defValue, section = "Preferences");
```
Example:
```cpp
int   horns  = cfg->GetInt  ("HornsThreshold", 2000, "Settings");
float cruise = cfg->GetFloat("CruiseSpeed",    15.0f, "Settings");
bool  on     = cfg->GetBool ("Enabled",        true,  "Settings");
```

### Colors
```cpp
struct rgba_t { unsigned char r,g,b,a; /* + .value, .v[4], x/y/z/w aliases */ };
rgba_t c = cfg->GetColor("TextColor", rgba_t(255,0,0), "Settings");
```

### Bound entries (`ConfigEntry`) — for values you read/clamp/write back
`Bind(...)` returns a heap `ConfigEntry*` (delete it yourself, or use `ClearLast()`):
```cpp
ConfigEntry* Bind(key, const char* def, section = "Preferences");
ConfigEntry* Bind(key, int   def,       section = "Preferences");
ConfigEntry* Bind(key, float def,       section = "Preferences");
ConfigEntry* Bind(key, bool  def,       section = "Preferences");
ConfigEntry* Bind(key, rgba_t def,      section = "Preferences");
```
`ConfigEntry` API:
```cpp
int   GetInt();    void SetInt(int);
float GetFloat();  void SetFloat(float);
bool  GetBool();   void SetBool(bool);
const char* GetString();  void GetString(char* buf, size_t len);  void SetString(const char*);
rgba_t ParseColor();      void SetColor(rgba_t, bool asFloat = false);
int   Clamp(int min, int max);     // returns overflow amount, marks config changed
float Clamp(float min, float max);
void  Reset();                     // back to default
bool  LoadedUndefault();           // was a non-default value present in the file?
```
Example with clamp + save:
```cpp
ConfigEntry* e = cfg->Bind("MaxSpeed", 120, "Settings");
e->Clamp(0, 300);
int maxSpeed = e->GetInt();
delete e;                 // or cfg->ClearLast();
cfg->Save();              // persist changes
```

### Other
```cpp
void  Init();  void Save();
bool  HasSection(section);          bool HasKey(section, key);            // AML 1.4.0+
bool  HasSectionComment(section);   bool HasKeyComment(section, key);     // AML 1.4.0+
bool  IsValueChanged();
void  ClearLast();                  // delete the last Bind() entry
```

### Convenience clamps (free functions, `amlmod.h`)
```cpp
int   clampint(min, max, v);    void clampint(min, max, int* v);
float clampfloat(min, max, v);  void clampfloat(min, max, float* v);
```

---

## 10. Lightweight config: `ICFG` (optional)

`icfg.h` provides a slimmer INI interface that avoids `<fstream>` (≈370 KB smaller mod). Use it only if you are not already pulling in `fstream`. Get it via `GetCFGInterface()` (global `icfg`):

```cpp
ICFG* c = GetCFGInterface();
void* ini = c->InitIniPointer();
c->ParseInputStream(ini, "/sdcard/.../my.ini");

char buf[64];
if(c->GetValueFrom(ini, "Settings", "Name", buf, sizeof(buf)))  // safe overload
    logger->Info("Name=%s", buf);

c->SetValueTo(ini, "Settings", "Enabled", "true");
c->GenerateToOutputStream(ini, "/sdcard/.../my.ini");
// also: HasSection / HasKey / HasSectionComment / HasKeyComment   (AML 1.4.0)
```

`config.h`'s `Config` will use `ICFG` under the hood automatically if `_ICFG` is defined (i.e. `icfg.h` included). Pick **one** config approach per mod.

---

## 11. Logging: `logger->` (`Logger`)

```cpp
logger->SetTag("My Cool Mod");           // tag (max 30 chars) shown in logcat
logger->Info ("base = 0x%X", base);      // printf-style
logger->Error("missing symbol: %s", s);
logger->Print(LogP_Warn, "fmt %d", x);   // explicit priority
logger->PrintTag(LogP_Info, "OtherTag", "fmt");
logger->ToggleOutput(false);             // mute
bool on = logger->HasOutput();
```
Priorities: `LogP_Verbose, LogP_Debug, LogP_Info, LogP_Warn, LogP_Error, LogP_Fatal, LogP_Silent`.
Define `-DNOLOGGING` to compile out all logging (`HasOutput()` becomes constant `false`). Max formatted buffer is 2048/4096 bytes.

---

## 12. Utility headers

### `localref.h` — `LocalRef<T>` (RAII)
Runs a deleter when it leaves scope. Great for JNI local refs, `malloc`, `FILE*`, AML assets.
```cpp
JNIEnv* env = aml->GetJNIEnvironment();
LocalRef<jclass> cls(env->FindClass("java/lang/String"),
                     [env](jclass c){ env->DeleteLocalRef(c); });
// use cls (implicitly converts to T); auto-released at scope end
LocalRef<void*> a(aml->OpenAsset("data.bin"), [](void* x){ aml->CloseAsset(x); });
```
API: `Get()`, `Valid()`, `Detach()` (release ownership), `operator T()`, `explicit operator bool`.

### `scopeguard.h` — `DEFER(...)`
Runs arbitrary cleanup code at scope exit. Unlike `LocalRef`, it does **not** wrap a variable.
```cpp
jclass cls = env->FindClass("java/lang/String");
DEFER( env->DeleteLocalRef(cls) );   // runs on every return path
if(bail) return;                      // DEFER fires here
```

### `listitem.h` — intrusive linked list
Macro-built doubly-linked list used internally by AML. Define a node type with `LIST_START(Name) ... LIST_END()` (+ `LIST_INITSTART/INITEND` for the ctor), then `Push(&list)`, `Remove(&list)`, `Count()`, `First()`, `Last()`, `InList(&list)`. Iterate with:
```cpp
LIST_FOR(list)        { /* `item` is the node; safe for Remove/Push */ }
LIST_FOR_FAST(list)   { /* faster; do NOT modify the list */ }
LIST_FOR_REVERSE(list){ /* back to front */ }
```

---

## 13. IL2CPP / Unity mods

`il2cpp.h` only forward-declares IL2CPP types (`IL2Object`, `IL2Class`, `MethodInfo`, `FieldInfo`, etc.). AML does not bundle a full IL2CPP toolkit — you supply your own dumped headers/SDK. Once you have a `MethodInfo*`, hook the native method with:
```cpp
DECL_HOOK(void*, SomeManagedMethod, void* self /*, args */)
{
    return SomeManagedMethod(self /*, args */);
}
ON_MOD_LOAD()
{
    MethodInfo* mi = /* resolve from your IL2CPP dump */;
    HOOK_IL2CPP(SomeManagedMethod, mi);   // hooks mi->methodPointer
}
```

---

## 14. Complete example mod

```cpp
#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

MYMODCFG(net.author.example, Example Mod, 1.0, AuthorName)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()

uintptr_t pGame = 0;
int   gMaxSpeed = 0;
bool  gEnabled  = true;

DECL_HOOKv(GameUpdate)
{
    GameUpdate();                 // original first
    if(gEnabled) { /* per-frame logic */ }
}

DECL_HOOK(int, CalcScore, void* self, int base)
{
    return CalcScore(self, base) + 100;
}

ON_MOD_PRELOAD()
{
    logger->SetTag("Example Mod");
}

ON_MOD_LOAD()
{
    pGame = aml->GetLib("libGTASA.so");
    if(!pGame) { logger->Error("game lib not found"); return; }

    // Config (section defaults to "Preferences"; using a custom section here)
    gEnabled  = cfg->GetBool("Enabled",  true, "Settings");
    ConfigEntry* spd = cfg->Bind("MaxSpeed", 120, "Settings");
    spd->Clamp(0, 300);
    gMaxSpeed = spd->GetInt();
    delete spd;
    cfg->Save();
    logger->Info("Enabled=%d MaxSpeed=%d", gEnabled, gMaxSpeed);

    // Patch + hooks
    aml->PlaceNOP(pGame + BYBIT(0x100000, 0x1000000), 1);
    HOOK(GameUpdate, pGame + BYBIT(0x200000, 0x2000000));
    HOOK(CalcScore,  pGame + BYBIT(0x300000, 0x3000000));

    // Expose an interface for other mods (optional)
    // RegisterInterface("IExample001", &g_Example);

    logger->Info("Example Mod loaded.");
}

ON_MOD_UNLOAD()
{
    logger->Info("Example Mod unloaded.");
}
```

---

## 15. Quick reference

| Item | Purpose |
|---|---|
| `MYMOD / MYMODCFG / MYMODCFGNAME` | Declare mod (+ optional `cfg`) |
| `NEEDGAME(pkg)` | Restrict mod to one package |
| `BEGIN_DEPLIST / ADD_DEPENDENCY[_VER] / END_DEPLIST` | Dependencies |
| `ON_MOD_PRELOAD / ON_MOD_LOAD / ON_ALL_MODS_LOAD / ON_MOD_UNLOAD` | Lifecycle |
| `ON_GAME_CRASH / ON_NEW_INTERFACE / UPDATER_URL` | Optional hooks |
| `GetInterface(name)` / `RegisterInterface(name, ptr)` | Interface system (global funcs, from `interface.h`) |
| `WRAP_INTERFACE(name, var)` | Sugar for RegisterInterface |
| `aml->GetLib / GetLibLength / GetLibHandle` | Library lookup |
| `aml->GetSym(handle|addr, "sym")` | Symbol resolution |
| `aml->Unprot / Write[8..64] / Read[8..64] / PatchMemory` | Memory R/W |
| `aml->PlaceNOP / PlaceJMP(=PlaceB) / PlaceRET / PlaceBL / Redirect` | Instruction patching |
| `aml->Hook / HookPLT / HookB/BL/BLX / HookVtableFunc / HookInline` | Hooking core |
| `aml->PatternScan / FindAllPatterns / ReadPointerChain` | Scanning |
| `aml->GetJNIEnvironment / CallStaticJavaMethod / GetCurrentActivity` | JNI bridge |
| `aml->MLS*` | Persistent key/value store |
| `aml->ShowToast / ShowDialog / DoVibro / GetBatteryLevel` | Device/UI |
| `DECL_HOOK[v/b/i/p]` + `HOOK[SYM/PLT/B/BL/BLX]` / `HOOK_IL2CPP` | Hook macros |
| `cfg->GetInt/GetFloat/GetBool/GetString/GetColor(key, def, section="Preferences")` | Read config |
| `cfg->Bind(...) -> ConfigEntry` + `Clamp/Set*/Reset` + `cfg->Save()` | Bound config |
| `icfg->...` (`GetCFGInterface()`) | Lightweight INI |
| `logger->SetTag/Info/Error/Print` | Logging |
| `LocalRef<T>` / `DEFER(...)` | RAII cleanup |
| `BYBIT(v32, v64)` / `AML32` | Per-ABI values |
| `IAML_VER` | Header/API version guard |

### Version guard
```cpp
#include <mod/amlmod.h>
#if !defined(IAML_VER) || IAML_VER < 01040000
    #error "You need to update your MOD folder to 1.4.0!"
#endif
```

---

## 16. Reminders

- AML loads **after** the game library; patch the already-mapped lib, don't hook `JNI_OnLoad`.
- `GetInterface` / `RegisterInterface` are global functions, **not** `aml->` methods.
- `aml` is auto-filled by `MYMOD` via `GetInterface("AMLInterface")` at init priority 101 — always ready in callbacks.
- Config default section is `"Preferences"`. `Bind()` results are heap-allocated; free with `delete` or `cfg->ClearLast()`.
- Best living references: the official mods at https://github.com/AndroidModLoader?tab=repositories and the headers themselves.
