# AndroidModLoader (AML) — Dokumentasi Pengembangan Mod

> Repositori: https://github.com/RusJJ/AndroidModLoader
> Contoh mod: https://github.com/AndroidModLoader?tab=repositories
> Discord: https://discord.gg/2MY7W39kBg
>
> Dokumen ini ditulis berdasarkan header mod yang sebenarnya (`amlmod.h`, `iaml.h`, `interface.h`, `config.h`, `icfg.h`, `logger.h`, `localref.h`, `scopeguard.h`, `listitem.h`, `il2cpp.h`). Kumpulan header ini sesuai dengan **IAML_VER `01040000` (AML 1.4.0)**.

---

## 1. Gambaran umum dan urutan pemuatan

AML memuat mod `.so` native ke dalam aplikasi target dan menyediakan sistem interface bergaya Source Engine untuk hooking, patching memori, konfigurasi, akses JNI, dan API antar-mod.

**Fakta penting tentang urutan pemuatan:** AML dimuat **setelah** library utama game sudah dimuat (setelah `JNI_OnLoad` dan inisialisasi game itu sendiri). Artinya, ketika `OnModPreLoad` / `OnModLoad` milikmu berjalan, `aml->GetLib("libGame.so")` sudah mengembalikan alamat basis yang valid. Kamu tidak meng-hook `JNI_OnLoad`; kamu mem-patch library yang sudah ter-mapping di memori.

ABI hanya ARM. Header mendefinisikan tepat salah satu dari:
```cpp
#ifdef AML32   // armeabi-v7a (__arm__)
#else          // AML64, arm64-v8a (__aarch64__)
#endif
```
Makro `BYBIT(v32, v64)` memilih nilai per-ABI:
```cpp
uintptr_t off = pGame + BYBIT(0x123456, 0x1234567);
```

### Lokasi mod
```
/sdcard/Android/data/<game.package>/mods/      (eksternal)
/sdcard/AMLMods/<game>/                          (internal default, lihat GetInternalModsPath)
```

---

## 2. Membangun sebuah mod

Mod adalah shared library NDK. Kamu **tidak perlu** me-link ke AML — `interface.h` menemukan AML saat runtime via `dlopen("libAML.so")`. Cukup kompilasi dengan header mod AML (diletakkan di `include/mod/`).

`Application.mk`:
```makefile
APP_ABI := armeabi-v7a arm64-v8a
APP_PLATFORM := android-21
APP_STL := c++_static
APP_CPPFLAGS += -std=c++17     # diperlukan untuk helper template JNI
```

`Android.mk`:
```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE     := mymod
LOCAL_SRC_FILES  := main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_LDLIBS     := -llog
# Opsional: matikan semua logging untuk memperkecil ukuran mod
# LOCAL_CXXFLAGS += -DNOLOGGING
include $(BUILD_SHARED_LIBRARY)
```
Bangun dengan `ndk-build`.

---

## 3. Mendeklarasikan sebuah mod

Sertakan header inti, lalu deklarasikan metadata dengan satu makro.

```cpp
#include <mod/amlmod.h>   // menarik iaml.h -> interface.h, jni.h, dll.
#include <mod/logger.h>
#include <mod/config.h>   // hanya jika butuh cfg

MYMOD(net.author.mymod, My Cool Mod, 1.0, AuthorName)
```

### Apa yang dihasilkan `MYMOD` untukmu
Makro (`amlmod.h`) berkembang menjadi:
- `ModInfo modinfoLocal(...)` statis dan `ModInfo* modinfo` global,
- export `__GetModInfo()` yang dipanggil AML untuk membaca metadata-mu,
- variabel global `IAML* aml`,
- sebuah stub inisialisasi statis (init priority 101) yang berjalan **sebelum** callback-mu dan melakukan:
  ```cpp
  aml = (IAML*)GetInterface("AMLInterface");
  ```

Jadi `aml` sudah siap saat entry point apa pun berjalan. Kamu tidak perlu mengambilnya secara manual.

### Varian makro
```cpp
MYMOD(_guid, _name, _version, _author)
    // dasar; membuat modinfo + aml

MYMODCFG(_guid, _name, _version, _author)
    // MYMOD + `Config* cfg` global yang file .ini-nya dinamai sesuai _guid

MYMODCFGNAME(_guid, _name, _version, _author, _cfgname)
    // MYMOD + `cfg` yang file .ini-nya dinamai sesuai _cfgname (nama file kustom)
```
Argumen berupa token tanpa tanda kutip (makro menstringifikasinya). GUID otomatis menjadi huruf kecil. Versi diurai menjadi `major.minor.revision.build`.

### Membatasi ke game tertentu (opsional)
```cpp
NEEDGAME(com.rockstargames.gtasa)   // mod hanya dimuat di dalam paket ini
```

### Membaca metadata sendiri
`modinfo` menyediakan: `GUID()`, `Name()`, `VersionString()`, `Author()`, `Major()`, `Minor()`, `Revision()`, `Build()`.

---

## 4. Dependensi

Dideklarasikan dengan sebuah blok; AML memakainya untuk urutan pemuatan dan menggagalkan secara keras bila mod wajib tidak ada.

```cpp
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)      // butuh AML >= 1.4.0
    ADD_DEPENDENCY_VER(net.rusjj.cleolib, 2.0.1.6)
    ADD_DEPENDENCY(net.rusjj.basslib)             // versi apa pun
END_DEPLIST()
```
- `ADD_DEPENDENCY(guid)` — butuh sebuah mod, versi apa pun.
- `ADD_DEPENDENCY_VER(guid, version)` — butuh mod minimal pada versi tersebut.

**Dependensi lunak (soft):** jangan masukkan ke daftar; sebagai gantinya periksa saat runtime dengan `aml->HasMod(...)` / `aml->HasModOfVersion(...)` / `aml->HasModOfBiggerVersion(...)`, atau cukup coba `GetInterface(...)` dan tangani `NULL`.

---

## 5. Entry point (callback)

Tiap makro berkembang menjadi export `extern "C"` yang dicari AML. Definisikan yang kamu perlukan.

```cpp
ON_MOD_PRELOAD()      // void OnModPreLoad()      — paling awal; library sudah ter-mapping
ON_MOD_LOAD()         // void OnModLoad()         — entry utama: hook, konfigurasi, simbol
ON_ALL_MODS_LOAD()    // void OnAllModsLoaded()   — setelah semua mod selesai dimuat
ON_MOD_UNLOAD()       // void OnModUnload()       — pembersihan (tidak dijamin berjalan)
ON_GAME_CRASH()       // void OnGameCrash(const char* library, int sig, int code,
                      //                  uintptr_t libaddr, mcontext_t* mcontext)
ON_NEW_INTERFACE()    // void OnInterfaceAdded(const char* name, const void* ptr)
UPDATER_URL()         // const char* OnUpdaterURLRequested()  — kembalikan URL update-mu
```

Di dalamnya kamu sudah punya `aml`, `logger`, dan (jika `MYMODCFG`) `cfg`.

Gunakan `ON_ALL_MODS_LOAD` saat kamu butuh interface mod lain yang mungkin terdaftar terlambat, atau gunakan `ON_NEW_INTERFACE` untuk bereaksi begitu sebuah interface bernama muncul.

```cpp
ON_MOD_PRELOAD()
{
    logger->SetTag("My Cool Mod");
}

ON_MOD_LOAD()
{
    uintptr_t pGame = aml->GetLib("libGTASA.so");
    if(!pGame) { logger->Error("game lib missing"); return; }
    // hook / patch / konfigurasi di sini
}
```

---

## 6. Sistem interface: `GetInterface` / `RegisterInterface`

Ini adalah mekanisme inti perluasan AML. **Keduanya adalah fungsi mandiri yang didefinisikan di `interface.h`, bukan method dari `aml`.** Secara internal keduanya melakukan `dlopen("libAML.so")` dan memanggil export AML `GetInterface` / `CreateInterface`.

```cpp
void* GetInterface(const char* szInterfaceName);              // ambil interface yang terdaftar
void  RegisterInterface(const char* szInterfaceName, void* p);// ekspos interface milikmu
```

Interface hanyalah sebuah pointer (biasanya ke struct/class berisi pointer fungsi atau ke class dengan vtable) yang disimpan di bawah nama string yang kamu pilih. Penyedia dan pemakai cukup menyepakati nama persis itu dan tata letak struct/class di baliknya. Cara menamai interface sepenuhnya terserah kamu.

`aml` sendiri diperoleh dengan cara ini: makro `MYMOD` memanggil `GetInterface("AMLInterface")` untukmu.

### Makro praktis
```cpp
#define WRAP_INTERFACE(name, var)  RegisterInterface(#name, var)
WRAP_INTERFACE(IMyApi, &g_MyApi);   // == RegisterInterface("IMyApi", &g_MyApi)
```

### A) Menyediakan sebuah interface
Sertakan sebuah header agar pemakai tahu tata letaknya, isi sebuah struct, daftarkan.

```cpp
// imyapi.h  (dibagikan ke pemakai)
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
    RegisterInterface("IMyApi", &g_MyApi);
}
```

Interface berbasis class dengan vtable juga bisa (pemakai memanggil virtual langsung):
```cpp
class IMyApi { public: virtual int Add(int,int)=0; virtual void DoThing(const char*)=0; };
```

### B) Memakai sebuah interface
```cpp
#include <ibass.h>     // header yang dibawa oleh mod BASS
IBASS* BASS = NULL;

MYMOD(net.author.user, BASS User, 1.0, AuthorName)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.basslib, 1.2)   // memastikan BASS dimuat lebih dulu
END_DEPLIST()

ON_MOD_LOAD()
{
    BASS = (IBASS*)GetInterface("IBASS");
    if(!BASS) { logger->Error("IBASS not found"); return; }
    // BASS->...
}
```

### C) Bereaksi terhadap interface yang muncul terlambat
```cpp
ON_NEW_INTERFACE()   // OnInterfaceAdded(name, ptr)
{
    if(!strcmp(name, "IBASS"))
        BASS = (IBASS*)ptr;
}
```

### Catatan
- Jika kamu berdependensi keras pada penyedia (`ADD_DEPENDENCY*`), interface sudah ada saat `OnModLoad`-mu. Untuk penyedia opsional, periksa `NULL`.
- Nama interface yang persis (`"IBASS"` di atas hanyalah contoh) adalah apa pun yang dipilih penyedia. Cek header/dokumen penyedia dan pakai string itu apa adanya.

---

## 7. Interface `aml->` (`IAML`)

`aml` adalah inti AML. Berikut method-nya dikelompokkan menurut tujuan. Signature persis sesuai `iaml.h`. Ketersediaan diberi anotasi versi AML yang memperkenalkan tiap blok.

### Info game / mod
```cpp
const char* GetCurrentGame();                 // paket game saat ini (mungkin dipalsukan)
const char* GetRealCurrentGame();             // paket asli
const char* GetAppVersionName();              // versionName aplikasi/game target
int64_t     GetAppVersionCode();              // long version code aplikasi/game target
const char* GetApkPath();                     // path APK sumber
const char* GetApkMD5();                      // MD5 APK sumber, dihitung saat dibutuhkan
bool        IsGameFaked();
bool        HasMod(const char* guid);
bool        HasModOfVersion(const char* guid, const char* version);
bool        HasModOfBiggerVersion(const char* guid, const char* version);
int         GetModsLoadedCount();
int         GetFailedModsCount();
void        ListMods(ListModsCallback cb, void* data = NULL, bool startWithLatest = false);
const char* GetFeatures();                    // string fitur build
```

### Library dan simbol
```cpp
uintptr_t   GetLib(const char* szLib);                 // alamat basis .so yang dimuat
uintptr_t   GetLibLength(const char* szLib);           // ukuran ter-mapping
void*       GetLibHandle(const char* soLib);           // handle gaya dlopen
void*       GetLibHandle(uintptr_t addr);
uintptr_t   GetSym(void* handle, const char* sym);     // resolusi via handle
uintptr_t   GetSym(uintptr_t libAddr, const char* sym);// resolusi via ALAMAT library
```
Helper xDL (selalu ada, mengembalikan 0 bila xDL tidak dipakai):
```cpp
bool IsCorrectXDLHandle(void*); uintptr_t GetLibXDL(void*);
uintptr_t GetAddrBaseXDL(uintptr_t); size_t GetSymSizeXDL(void*);
const char* GetSymNameXDL(void*);
```
Makro penetapan via simbol:
```cpp
SET_TO(var, addr);              // *(void**)&var = (void*)addr
SET_TO_PTR(var, ptraddr);       // dereference lalu tetapkan
SETSYM_TO(var, lib, "sym");     // var = aml->GetSym(lib, "sym")
SETSYM_TO_PTR(var, lib, "sym");
AS_ADDR(var);                   // *(uintptr_t*)&var
```

### Memori: unprotect / write / read
```cpp
int  Unprot(uintptr_t addr, size_t len = PAGE_SIZE);
void Write(uintptr_t dest, uintptr_t src, size_t size);
void Read (uintptr_t src,  uintptr_t dest, size_t size);
```
Helper inline bertipe (tanpa ukuran manual):
```cpp
Write8/16/32/64(addr, v);  WriteFloat/WriteDouble(addr, v);
WriteAddr(addr, ptr);      Write(dest, "string");        // juga Write(dest, str, size)
Read8/16/32/64(addr) -> value;  ReadFloat/ReadDouble(addr) -> value;
bool PatchMemory(addr, { 0x00, 0x10, ... });             // unprot + tulis daftar byte
```
Alokasi / rantai pointer / pemindaian:
```cpp
uintptr_t AllocateMemory(size_t size, bool executable = false);
bool      FreeMemory(uintptr_t ptr);
uintptr_t ReadPointerChain(uintptr_t base, std::initializer_list<int> offsets);
uintptr_t PatternScan(const char* pattern, const char* soLib);
uintptr_t PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
std::vector<uintptr_t> FindAllPatterns(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
bool      ComparePattern(uintptr_t addr, const char* pattern);
```

### Patching instruksi (ARM)
```cpp
int  PlaceNOP(uintptr_t addr, size_t count = 1);
int  PlaceNOP4(uintptr_t addr, size_t count = 1);   // NOP 4-byte
int  PlaceJMP(uintptr_t addr, uintptr_t dest);      // PlaceB adalah alias PlaceJMP
int  PlaceRET(uintptr_t addr);
void PlaceBL (uintptr_t addr, uintptr_t dest);
void PlaceBLX(uintptr_t addr, uintptr_t dest);
int  Redirect(uintptr_t addr, uintptr_t to);        // lompat dengan stack+register sama
void PatchForThumb(bool forThumb);                  // mode patching thumb vs arm
bool IsThumbAddr(uintptr_t addr);
uintptr_t GetBranchDest(uintptr_t addr);
bool HasAddrExecFlag(uintptr_t addr);
// redirect arm64 dengan menjaga register:
void RedirectReg(uintptr_t addr, uintptr_t to, bool doShortHook = false,
                 GlossRegisters::e_reg targetReg = GlossRegisters::X16);
```

### Hooking (tingkat rendah — lebih disarankan makro di §8)
```cpp
bool Hook   (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookPLT(void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookB  (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBL (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBLX(void* targetAddr, void* newFn, void** orgFn = NULL);
void HookVtableFunc(void* ptr, unsigned funcNum, void* newFn, void** orgFn = NULL, bool instantiate = false);
void HookVtableFunc(void* ptr, unsigned funcNum, unsigned count, void* newFn, void** orgFn = NULL, bool instantiate = false);
// hook inline tingkat register:
PHookHandle HookInline(void* fnAddress, HookWithRegistersFn newFn, bool doShortHook = false);
void ToggleHook(PHookHandle hook, bool enable);
void DeHook(PHookHandle hook);
// helper vtable:
void** GetVtable(void* ptr);  void SetVtable(void* ptr, void** vtable);
```

### Path
```cpp
const char* GetDataPath();            // /data/data/<pkg>/...
const char* GetConfigPath();
const char* GetAndroidDataPath();     // /sdcard/Android/data/<pkg>/files/...
const char* GetAndroidDataRootPath(); // /sdcard/Android/data/<pkg>/... (tanpa /files/)
const char* GetInternalPath();        // /sdcard/
const char* GetInternalModsPath();    // /sdcard/AMLMods/<game>/
const char* GetNativeLibsPath();      // /data/app/<...>/lib/arm*/
```

### File
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

### Asset (asset APK)
```cpp
AAssetManager* GetAssetManager();
void*  OpenAsset(const char* path, int mode = AASSET_MODE_BUFFER);
void   CloseAsset(void* asset);
size_t GetAssetSize(void* asset);
const void* GetAssetBuffer(void* asset);
void   ReadAsset(void* asset, void* buf, size_t count);
```

### UI / perangkat / sistem
```cpp
void  ShowToast(bool longerDuration, const char* fmt, ...);
void  ShowDialog(const char* title, const char* message, const char* buttonText = NULL, int styleResource = 0);
void  DoVibro(int msTime);                         // ~20ms = getaran ringan
void  DoVibro(jlong* pattern, int patternItems);   // mis. DEFAULT_VIBRATE_PATTERN, g_VibroPattern_Weak
void  CancelVibro();
float GetBatteryLevel();                           // 0.0 .. 100.0
void  GetDisplaySize(int* w = NULL, int* h = NULL);
int   GetAndroidVersion();                         // level API
int   GetAndroidSystemResID(const char* innerClass, const char* fieldName); // mis. "style","Theme_DeviceDefault_Alert"
void  OpenURL(const char* url);
int   GetCPUCores();
bool  IsMainThread();
void  GetNewsString(char* buf, size_t len);
```

### Jaringan / unduhan
```cpp
bool DownloadFile(const char* url, const char* saveto);
bool DownloadFileToData(const char* url, char* out, size_t outLen);
bool IsFileDownloadsEnabled();
int  GetDownloadTimeout();
int  GetLatestDownloadErrorCode();
```

### Jembatan JNI / Java
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
Plus kemudahan template (C++17, didefinisikan di `iaml.h`):
```cpp
T    CallJavaMethod<T>(jobject obj, const char* method, const char* sig, args...);  // otomatis Call*Method
auto CallVirtual<Ret>(void* instance, int index, args...);                          // panggil vtable[index]
```

### MLS — Mod Local Storage (penyimpanan key/value persisten)
Penyimpanan bertipe sederhana, terpisah dari konfigurasi `.ini`:
```cpp
void MLSSetInt/MLSSetFloat/MLSSetInt64/MLSSetStr(key, val);   void MLSSetBool(key, val);
bool MLSGetInt/MLSGetFloat/MLSGetInt64(key, &out);            bool MLSGetBool(key, &out);
bool MLSGetStr(key, buf, len);
bool MLSHasValue(key);   void MLSDeleteValue(key);   void MLSSaveFile();
bool IsMLSInManualSave();
```

---

## 8. Hooking fungsi (makro)

Deklarasikan hook dengan makro `DECL_HOOK*`, lalu pasang dengan makro `HOOK*`. Makro membuat dua hal: pointer ke **fungsi asli** (dinamai persis seperti yang kamu tulis) dan body pengganti milikmu bernama `HookOf_<name>`. Panggil fungsi asli dari dalam body-mu memakai nama tersebut.

### Makro deklarasi
```cpp
DECL_HOOK(ret, name, args...)   // mengembalikan `ret`
DECL_HOOKv(name, args...)       // void
DECL_HOOKb(name, args...)       // bool
DECL_HOOKi(name, args...)       // int
DECL_HOOKp(name, args...)       // void*
```
Varian berawalan `S` (`SDECL_HOOK*`) membuat pointer `static`; berawalan `SI` (`SIDECL_HOOK*`) membuatnya `static inline`.

### Makro pemasangan
```cpp
HOOK(name, fnAddr)                 // hook inline via alamat
HOOKSYM(name, libHandle, "symbol") // hook inline, simbol diresolusi untukmu
HOOKPLT(name, fnAddr)              // hook PLT/GOT
HOOKB(name, fnAddr)                // hook B
HOOKBL(name, fnAddr)               // hook BL
HOOKBLX(name, fnAddr)              // hook BLX
```
> Makro `HOOK_IL2CPP` ada di header, tapi IL2CPP belum siap (lihat §13); sengaja tidak didokumentasikan di sini.

### Contoh
```cpp
// fungsi void
DECL_HOOKv(InitialiseRenderWare)
{
    InitialiseRenderWare();              // panggil fungsi asli
    // ... kode-mu ...
}

// fungsi int dengan argumen
DECL_HOOK(int, CalcDamage, void* self, int raw)
{
    int dmg = CalcDamage(self, raw);     // fungsi asli
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

## 9. Konfigurasi: `cfg->` (`Config`)

Tersedia sebagai `cfg` global bila kamu mendeklarasikan mod dengan `MYMODCFG`/`MYMODCFGNAME` dan `#include <mod/config.h>`. **Section default adalah `"Preferences"`.**

### Getter cepat (tanpa alokasi, tanpa pembersihan)
```cpp
const char* GetString(key, defValue, section = "Preferences");   // pointer bersifat sementara
int         GetInt   (key, defValue, section = "Preferences");
float       GetFloat (key, defValue, section = "Preferences");
bool        GetBool  (key, defValue, section = "Preferences");
rgba_t      GetColor (key, defValue, section = "Preferences");
// salin string aman ke buffer-mu:
void        GetString(char* out, size_t outLen, key, defValue, section = "Preferences");
```
Contoh:
```cpp
int   horns  = cfg->GetInt  ("HornsThreshold", 2000, "Settings");
float cruise = cfg->GetFloat("CruiseSpeed",    15.0f, "Settings");
bool  on     = cfg->GetBool ("Enabled",        true,  "Settings");
```

### Warna
```cpp
struct rgba_t { unsigned char r,g,b,a; /* + .value, .v[4], alias x/y/z/w */ };
rgba_t c = cfg->GetColor("TextColor", rgba_t(255,0,0), "Settings");
```

### Entri terikat (`ConfigEntry`) — untuk nilai yang kamu baca/klamp/tulis kembali
`Bind(...)` mengembalikan `ConfigEntry*` di heap (hapus sendiri, atau pakai `ClearLast()`):
```cpp
ConfigEntry* Bind(key, const char* def, section = "Preferences");
ConfigEntry* Bind(key, int   def,       section = "Preferences");
ConfigEntry* Bind(key, float def,       section = "Preferences");
ConfigEntry* Bind(key, bool  def,       section = "Preferences");
ConfigEntry* Bind(key, rgba_t def,      section = "Preferences");
```
API `ConfigEntry`:
```cpp
int   GetInt();    void SetInt(int);
float GetFloat();  void SetFloat(float);
bool  GetBool();   void SetBool(bool);
const char* GetString();  void GetString(char* buf, size_t len);  void SetString(const char*);
rgba_t ParseColor();      void SetColor(rgba_t, bool asFloat = false);
int   Clamp(int min, int max);     // mengembalikan jumlah overflow, menandai config berubah
float Clamp(float min, float max);
void  Reset();                     // kembali ke default
bool  LoadedUndefault();           // apakah ada nilai non-default di file?
```
Contoh dengan klamp + simpan:
```cpp
ConfigEntry* e = cfg->Bind("MaxSpeed", 120, "Settings");
e->Clamp(0, 300);
int maxSpeed = e->GetInt();
delete e;                 // atau cfg->ClearLast();
cfg->Save();              // simpan perubahan
```

### Lainnya
```cpp
void  Init();  void Save();
bool  HasSection(section);          bool HasKey(section, key);            // AML 1.4.0+
bool  HasSectionComment(section);   bool HasKeyComment(section, key);     // AML 1.4.0+
bool  IsValueChanged();
void  ClearLast();                  // hapus entri Bind() terakhir
```

### Klamp praktis (fungsi bebas, `amlmod.h`)
```cpp
int   clampint(min, max, v);    void clampint(min, max, int* v);
float clampfloat(min, max, v);  void clampfloat(min, max, float* v);
```

---

## 10. Konfigurasi ringan: `ICFG` (opsional)

`icfg.h` menyediakan interface INI yang lebih ramping yang menghindari `<fstream>` (mod ≈370 KB lebih kecil). Pakai hanya jika kamu belum menarik `fstream`. Diperoleh via `GetCFGInterface()` (global `icfg`):

```cpp
ICFG* c = GetCFGInterface();
void* ini = c->InitIniPointer();
c->ParseInputStream(ini, "/sdcard/.../my.ini");

char buf[64];
if(c->GetValueFrom(ini, "Settings", "Name", buf, sizeof(buf)))  // overload aman
    logger->Info("Name=%s", buf);

c->SetValueTo(ini, "Settings", "Enabled", "true");
c->GenerateToOutputStream(ini, "/sdcard/.../my.ini");
// juga: HasSection / HasKey / HasSectionComment / HasKeyComment   (AML 1.4.0)
```

`Config` dari `config.h` akan otomatis memakai `ICFG` di baliknya bila `_ICFG` terdefinisi (yaitu `icfg.h` disertakan). Pilih **satu** pendekatan konfigurasi per mod.

---

## 11. Logging: `logger->` (`Logger`)

```cpp
logger->SetTag("My Cool Mod");           // tag (maks. 30 karakter) yang muncul di logcat
logger->Info ("base = 0x%X", base);      // gaya printf
logger->Error("missing symbol: %s", s);
logger->Print(LogP_Warn, "fmt %d", x);   // prioritas eksplisit
logger->PrintTag(LogP_Info, "OtherTag", "fmt");
logger->ToggleOutput(false);             // bisukan
bool on = logger->HasOutput();
```
Prioritas: `LogP_Verbose, LogP_Debug, LogP_Info, LogP_Warn, LogP_Error, LogP_Fatal, LogP_Silent`.
Definisikan `-DNOLOGGING` untuk menghilangkan seluruh logging (`HasOutput()` menjadi konstan `false`). Buffer format maksimum 2048/4096 byte.

---

## 12. Header utilitas

### `localref.h` — `LocalRef<T>` (RAII)
Menjalankan deleter saat keluar dari scope. Bagus untuk local ref JNI, `malloc`, `FILE*`, asset AML.
```cpp
JNIEnv* env = aml->GetJNIEnvironment();
LocalRef<jclass> cls(env->FindClass("java/lang/String"),
                     [env](jclass c){ env->DeleteLocalRef(c); });
// pakai cls (implisit dikonversi ke T); otomatis dilepas di akhir scope
LocalRef<void*> a(aml->OpenAsset("data.bin"), [](void* x){ aml->CloseAsset(x); });
```
API: `Get()`, `Valid()`, `Detach()` (lepaskan kepemilikan), `operator T()`, `explicit operator bool`.

### `scopeguard.h` — `DEFER(...)`
Menjalankan kode pembersihan apa pun saat keluar scope. Berbeda dari `LocalRef`, ia **tidak** membungkus sebuah variabel.
```cpp
jclass cls = env->FindClass("java/lang/String");
DEFER( env->DeleteLocalRef(cls) );   // berjalan di setiap jalur return
if(bail) return;                      // DEFER terpicu di sini
```

### `listitem.h` — linked list intrusif
Linked list ganda berbasis makro yang dipakai internal oleh AML. Definisikan tipe node dengan `LIST_START(Name) ... LIST_END()` (+ `LIST_INITSTART/INITEND` untuk konstruktor), lalu `Push(&list)`, `Remove(&list)`, `Count()`, `First()`, `Last()`, `InList(&list)`. Iterasi dengan:
```cpp
LIST_FOR(list)        { /* `item` adalah node; aman untuk Remove/Push */ }
LIST_FOR_FAST(list)   { /* lebih cepat; JANGAN modifikasi list */ }
LIST_FOR_REVERSE(list){ /* dari belakang ke depan */ }
```

---

## 13. IL2CPP / mod Unity

**Dukungan IL2CPP BELUM siap dan TIDAK disertakan dalam AML.** `il2cpp.h` hanya mendeklarasikan-maju beberapa nama tipe IL2CPP (`IL2Object`, `IL2Class`, `MethodInfo`, `FieldInfo`, dll.) agar kode yang tidak terkait bisa dikompilasi. Tidak ada dumper, resolver runtime, maupun SDK yang dibawa AML, dan dokumen ini tidak menyediakan panduan modding IL2CPP.

Jika ingin memod game Unity/IL2CPP, kamu harus mengerjakan sisi IL2CPP sepenuhnya sendiri dengan tooling eksternal milikmu (dump game, memperoleh header serta alamat method/field, meresolusi method saat runtime, dll.). AML hanya menyediakan primitif hooking dan patching memori umum yang dijelaskan di dokumen ini; setelah kamu meresolusi alamat mentah sendiri, hook ia seperti fungsi native lainnya.

---

## 14. Contoh mod lengkap

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
    GameUpdate();                 // fungsi asli dulu
    if(gEnabled) { /* logika per-frame */ }
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

    // Konfigurasi (section default "Preferences"; di sini memakai section sendiri)
    gEnabled  = cfg->GetBool("Enabled",  true, "Settings");
    ConfigEntry* spd = cfg->Bind("MaxSpeed", 120, "Settings");
    spd->Clamp(0, 300);
    gMaxSpeed = spd->GetInt();
    delete spd;
    cfg->Save();
    logger->Info("Enabled=%d MaxSpeed=%d", gEnabled, gMaxSpeed);

    // Patch + hook
    aml->PlaceNOP(pGame + BYBIT(0x100000, 0x1000000), 1);
    HOOK(GameUpdate, pGame + BYBIT(0x200000, 0x2000000));
    HOOK(CalcScore,  pGame + BYBIT(0x300000, 0x3000000));

    // Sediakan interface untuk mod lain (opsional)
    // RegisterInterface("IExample", &g_Example);

    logger->Info("Example Mod loaded.");
}

ON_MOD_UNLOAD()
{
    logger->Info("Example Mod unloaded.");
}
```

---

## 15. Referensi singkat

| Item | Tujuan |
|---|---|
| `MYMOD / MYMODCFG / MYMODCFGNAME` | Deklarasi mod (+ `cfg` opsional) |
| `NEEDGAME(pkg)` | Membatasi mod ke satu paket |
| `BEGIN_DEPLIST / ADD_DEPENDENCY[_VER] / END_DEPLIST` | Dependensi |
| `ON_MOD_PRELOAD / ON_MOD_LOAD / ON_ALL_MODS_LOAD / ON_MOD_UNLOAD` | Siklus hidup |
| `ON_GAME_CRASH / ON_NEW_INTERFACE / UPDATER_URL` | Callback opsional |
| `GetInterface(name)` / `RegisterInterface(name, ptr)` | Sistem interface (fungsi global, dari `interface.h`) |
| `WRAP_INTERFACE(name, var)` | Pemanis untuk RegisterInterface |
| `aml->GetLib / GetLibLength / GetLibHandle` | Pencarian library |
| `aml->GetSym(handle|addr, "sym")` | Resolusi simbol |
| `aml->Unprot / Write[8..64] / Read[8..64] / PatchMemory` | Baca/tulis memori |
| `aml->PlaceNOP / PlaceJMP(=PlaceB) / PlaceRET / PlaceBL / Redirect` | Patching instruksi |
| `aml->Hook / HookPLT / HookB/BL/BLX / HookVtableFunc / HookInline` | Inti hooking |
| `aml->PatternScan / FindAllPatterns / ReadPointerChain` | Pemindaian |
| `aml->GetJNIEnvironment / CallStaticJavaMethod / GetCurrentActivity` | Jembatan JNI |
| `aml->MLS*` | Penyimpanan key/value persisten |
| `aml->ShowToast / ShowDialog / DoVibro / GetBatteryLevel` | Perangkat/UI |
| `aml->GetAppVersionName / GetAppVersionCode / GetApkPath / GetApkMD5` | Identitas/versi aplikasi target |
| `DECL_HOOK[v/b/i/p]` + `HOOK[SYM/PLT/B/BL/BLX]` | Makro hook |
| `cfg->GetInt/GetFloat/GetBool/GetString/GetColor(key, def, section="Preferences")` | Baca konfigurasi |
| `cfg->Bind(...) -> ConfigEntry` + `Clamp/Set*/Reset` + `cfg->Save()` | Konfigurasi terikat |
| `icfg->...` (`GetCFGInterface()`) | INI ringan |
| `logger->SetTag/Info/Error/Print` | Logging |
| `LocalRef<T>` / `DEFER(...)` | Pembersihan RAII |
| `BYBIT(v32, v64)` / `AML32` | Nilai per-ABI |
| `IAML_VER` | Pengecekan versi header/API |

### Pengecekan versi
```cpp
#include <mod/amlmod.h>
#if !defined(IAML_VER) || IAML_VER < 01040000
    #error "You need to update your MOD folder to 1.4.0!"
#endif
```

---

## 16. Pengingat

- AML dimuat **setelah** library game; patch library yang sudah ter-mapping, jangan hook `JNI_OnLoad`.
- `GetInterface` / `RegisterInterface` adalah fungsi global, **bukan** method `aml`.
- `aml` diisi otomatis oleh makro `MYMOD` via `GetInterface("AMLInterface")` pada init priority 101 — selalu siap di callback.
- Section konfigurasi default adalah `"Preferences"`. Hasil `Bind()` dialokasikan di heap; bebaskan dengan `delete` atau `cfg->ClearLast()`.
- Referensi hidup terbaik: mod resmi di https://github.com/AndroidModLoader?tab=repositories dan header itu sendiri.
