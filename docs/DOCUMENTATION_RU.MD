# AndroidModLoader (AML) — Документация по разработке модов

> Репозиторий: https://github.com/RusJJ/AndroidModLoader
> Примеры модов: https://github.com/AndroidModLoader?tab=repositories
> Discord: https://discord.gg/2MY7W39kBg
>
> Документ написан на основе реальных заголовков модов (`amlmod.h`, `iaml.h`, `interface.h`, `config.h`, `icfg.h`, `logger.h`, `localref.h`, `scopeguard.h`, `listitem.h`, `il2cpp.h`). Набор заголовков соответствует **IAML_VER `01040000` (AML 1.4.0)**.

---

## 1. Обзор и порядок загрузки

AML загружает нативные `.so`-моды в целевое приложение и предоставляет систему интерфейсов в стиле Source Engine для хуков, патчинга памяти, конфигурации, доступа к JNI и обмена API между модами.

**Важный факт о порядке загрузки:** AML загружается **после** того, как основная библиотека игры уже загружена (после `JNI_OnLoad` и собственной инициализации игры). Это значит, что к моменту вызова ваших `OnModPreLoad` / `OnModLoad` функция `aml->GetLib("libGame.so")` уже возвращает корректный базовый адрес. Вы не хукаете `JNI_OnLoad` — вы патчите уже отображённую в памяти библиотеку.

ABI — только ARM. Заголовки определяют ровно один из вариантов:
```cpp
#ifdef AML32   // armeabi-v7a (__arm__)
#else          // AML64, arm64-v8a (__aarch64__)
#endif
```
Макрос `BYBIT(v32, v64)` выбирает значение в зависимости от ABI:
```cpp
uintptr_t off = pGame + BYBIT(0x123456, 0x1234567);
```

### Расположение модов
```
/sdcard/Android/data/<game.package>/mods/      (внешнее)
/sdcard/AMLMods/<game>/                          (внутреннее по умолчанию, см. GetInternalModsPath)
```

---

## 2. Сборка мода

Мод — это разделяемая библиотека NDK. Линковать AML **не нужно** — `interface.h` находит AML во время выполнения через `dlopen("libAML.so")`. Достаточно компилироваться с заголовками модов AML (размещёнными в `include/mod/`).

`Application.mk`:
```makefile
APP_ABI := armeabi-v7a arm64-v8a
APP_PLATFORM := android-21
APP_STL := c++_static
APP_CPPFLAGS += -std=c++17     # требуется для шаблонных JNI-хелперов
```

`Android.mk`:
```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE     := mymod
LOCAL_SRC_FILES  := main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_LDLIBS     := -llog
# Необязательно: отключить всё логирование ради уменьшения размера мода
# LOCAL_CXXFLAGS += -DNOLOGGING
include $(BUILD_SHARED_LIBRARY)
```
Сборка командой `ndk-build`.

---

## 3. Объявление мода

Подключите основной заголовок, затем объявите метаданные одним макросом.

```cpp
#include <mod/amlmod.h>   // тянет за собой iaml.h -> interface.h, jni.h и т.д.
#include <mod/logger.h>
#include <mod/config.h>   // только если нужен cfg

MYMOD(net.author.mymod, My Cool Mod, 1.0, AuthorName)
```

### Что генерирует `MYMOD`
Макрос (`amlmod.h`) разворачивается в:
- статический `ModInfo modinfoLocal(...)` и глобальный `ModInfo* modinfo`,
- экспорт `__GetModInfo()`, который AML вызывает, чтобы прочитать ваши метаданные,
- глобальный `IAML* aml`,
- статический инициализационный «заглушечный» объект (приоритет инициализации 101), который выполняется **до** ваших колбэков и делает:
  ```cpp
  aml = (IAML*)GetInterface("AMLInterface");
  ```

Так что `aml` готов к моменту запуска любой вашей точки входа. Вручную его получать не нужно.

### Варианты макроса
```cpp
MYMOD(_guid, _name, _version, _author)
    // базовый; создаёт modinfo + aml

MYMODCFG(_guid, _name, _version, _author)
    // MYMOD + глобальный `Config* cfg`, чей .ini назван по _guid

MYMODCFGNAME(_guid, _name, _version, _author, _cfgname)
    // MYMOD + `cfg`, чей .ini назван по _cfgname (своё имя файла)
```
Аргументы — это токены без кавычек (макрос сам делает из них строки). GUID автоматически приводится к нижнему регистру. Версия разбирается на `major.minor.revision.build`.

### Ограничение конкретной игрой (необязательно)
```cpp
NEEDGAME(com.rockstargames.gtasa)   // мод загружается только внутри этого пакета
```

### Чтение собственных метаданных
`modinfo` предоставляет: `GUID()`, `Name()`, `VersionString()`, `Author()`, `Major()`, `Minor()`, `Revision()`, `Build()`.

---

## 4. Зависимости

Объявляются блоком; AML использует его для порядка загрузки и жёсткого отказа при отсутствии обязательных модов.

```cpp
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)      // требуется AML >= 1.4.0
    ADD_DEPENDENCY_VER(net.rusjj.cleolib, 2.0.1.6)
    ADD_DEPENDENCY(net.rusjj.basslib)             // любая версия
END_DEPLIST()
```
- `ADD_DEPENDENCY(guid)` — требуется мод, любая версия.
- `ADD_DEPENDENCY_VER(guid, version)` — требуется мод не ниже указанной версии.

**Мягкая зависимость:** не указывайте мод в списке; вместо этого проверяйте во время выполнения через `aml->HasMod(...)` / `aml->HasModOfVersion(...)` / `aml->HasModOfBiggerVersion(...)`, либо просто попробуйте `GetInterface(...)` и обработайте `NULL`.

---

## 5. Точки входа (колбэки)

Каждый макрос разворачивается в экспорт `extern "C"`, который ищет AML. Определяйте те, что нужны.

```cpp
ON_MOD_PRELOAD()      // void OnModPreLoad()      — самый ранний; библиотека уже в памяти
ON_MOD_LOAD()         // void OnModLoad()         — основная точка входа: хуки, конфиг, символы
ON_ALL_MODS_LOAD()    // void OnAllModsLoaded()   — после того, как все моды загрузились
ON_MOD_UNLOAD()       // void OnModUnload()       — очистка (вызов не гарантирован)
ON_GAME_CRASH()       // void OnGameCrash(const char* library, int sig, int code,
                      //                  uintptr_t libaddr, mcontext_t* mcontext)
ON_NEW_INTERFACE()    // void OnInterfaceAdded(const char* name, const void* ptr)
UPDATER_URL()         // const char* OnUpdaterURLRequested()  — верните URL обновления
```

Внутри них доступны `aml`, `logger` и (при `MYMODCFG`) `cfg`.

Используйте `ON_ALL_MODS_LOAD`, когда нужен интерфейс другого мода, который может зарегистрироваться поздно, или `ON_NEW_INTERFACE`, чтобы среагировать в момент появления именованного интерфейса.

```cpp
ON_MOD_PRELOAD()
{
    logger->SetTag("My Cool Mod");
}

ON_MOD_LOAD()
{
    uintptr_t pGame = aml->GetLib("libGTASA.so");
    if(!pGame) { logger->Error("game lib missing"); return; }
    // хуки / патчи / конфиг здесь
}
```

---

## 6. Система интерфейсов: `GetInterface` / `RegisterInterface`

Это ключевой механизм расширяемости AML. **Это самостоятельные функции, определённые в `interface.h`, а НЕ методы `aml`.** Внутри они делают `dlopen("libAML.so")` и вызывают экспорты AML `GetInterface` / `CreateInterface`.

```cpp
void* GetInterface(const char* szInterfaceName);              // получить зарегистрированный интерфейс
void  RegisterInterface(const char* szInterfaceName, void* p);// предоставить свой
```

Интерфейс — это просто указатель (обычно на структуру/класс с указателями на функции или на класс с vtable), хранящийся под выбранным вами строковым именем. Провайдер и потребитель должны лишь договориться об этом точном имени и о компоновке структуры/класса за ним. Как именовать интерфейсы — решаете только вы.

Сам `aml` получается так же: макрос `MYMOD` вызывает за вас `GetInterface("AMLInterface")`.

### Удобный макрос
```cpp
#define WRAP_INTERFACE(name, var)  RegisterInterface(#name, var)
WRAP_INTERFACE(IMyApi, &g_MyApi);   // == RegisterInterface("IMyApi", &g_MyApi)
```

### A) Предоставление интерфейса
Поставьте заголовок, чтобы потребители знали компоновку, заполните структуру, зарегистрируйте её.

```cpp
// imyapi.h  (общий с потребителями)
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

Интерфейс на основе класса с vtable тоже работает (потребители вызывают виртуальные методы напрямую):
```cpp
class IMyApi { public: virtual int Add(int,int)=0; virtual void DoThing(const char*)=0; };
```

### B) Потребление интерфейса
```cpp
#include <ibass.h>     // заголовок, поставляемый модом BASS
IBASS* BASS = NULL;

MYMOD(net.author.user, BASS User, 1.0, AuthorName)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.basslib, 1.2)   // гарантирует, что BASS загрузится первым
END_DEPLIST()

ON_MOD_LOAD()
{
    BASS = (IBASS*)GetInterface("IBASS");
    if(!BASS) { logger->Error("IBASS not found"); return; }
    // BASS->...
}
```

### C) Реакция на поздние интерфейсы
```cpp
ON_NEW_INTERFACE()   // OnInterfaceAdded(name, ptr)
{
    if(!strcmp(name, "IBASS"))
        BASS = (IBASS*)ptr;
}
```

### Примечания
- Если вы жёстко зависите от провайдера (`ADD_DEPENDENCY*`), интерфейс уже существует к вашему `OnModLoad`. Для необязательных провайдеров проверяйте `NULL`.
- Точное имя интерфейса (`"IBASS"` выше — лишь пример) определяет провайдер. Смотрите его заголовок/документацию и используйте эту строку дословно.

---

## 7. Интерфейс `aml->` (`IAML`)

`aml` — это ядро AML. Ниже методы, сгруппированные по назначению. Сигнатуры точны по `iaml.h`. Для каждого блока указана версия AML, в которой он появился.

### Информация об игре / модах
```cpp
const char* GetCurrentGame();                 // пакет текущей (возможно поддельной) игры
const char* GetRealCurrentGame();             // настоящий пакет
const char* GetAppVersionName();              // versionName целевого приложения/игры
int64_t     GetAppVersionCode();              // long version code целевого приложения/игры
const char* GetApkPath();                     // путь к исходному APK
const char* GetApkMD5();                      // MD5 исходного APK, вычисляется лениво
bool        IsGameFaked();
bool        HasMod(const char* guid);
bool        HasModOfVersion(const char* guid, const char* version);
bool        HasModOfBiggerVersion(const char* guid, const char* version);
int         GetModsLoadedCount();
int         GetFailedModsCount();
void        ListMods(ListModsCallback cb, void* data = NULL, bool startWithLatest = false);
const char* GetFeatures();                    // строка возможностей сборки
```

### Библиотеки и символы
```cpp
uintptr_t   GetLib(const char* szLib);                 // базовый адрес загруженной .so
uintptr_t   GetLibLength(const char* szLib);           // размер в памяти
void*       GetLibHandle(const char* soLib);           // хендл в стиле dlopen
void*       GetLibHandle(uintptr_t addr);
uintptr_t   GetSym(void* handle, const char* sym);     // поиск по хендлу
uintptr_t   GetSym(uintptr_t libAddr, const char* sym);// поиск по АДРЕСУ библиотеки
```
Хелперы xDL (всегда присутствуют, возвращают 0, если xDL не используется):
```cpp
bool IsCorrectXDLHandle(void*); uintptr_t GetLibXDL(void*);
uintptr_t GetAddrBaseXDL(uintptr_t); size_t GetSymSizeXDL(void*);
const char* GetSymNameXDL(void*);
```
Макросы присваивания по символу:
```cpp
SET_TO(var, addr);              // *(void**)&var = (void*)addr
SET_TO_PTR(var, ptraddr);       // разыменовать, затем присвоить
SETSYM_TO(var, lib, "sym");     // var = aml->GetSym(lib, "sym")
SETSYM_TO_PTR(var, lib, "sym");
AS_ADDR(var);                   // *(uintptr_t*)&var
```

### Память: снятие защиты / запись / чтение
```cpp
int  Unprot(uintptr_t addr, size_t len = PAGE_SIZE);
void Write(uintptr_t dest, uintptr_t src, size_t size);
void Read (uintptr_t src,  uintptr_t dest, size_t size);
```
Типизированные inline-хелперы (без ручного размера):
```cpp
Write8/16/32/64(addr, v);  WriteFloat/WriteDouble(addr, v);
WriteAddr(addr, ptr);      Write(dest, "string");        // а также Write(dest, str, size)
Read8/16/32/64(addr) -> value;  ReadFloat/ReadDouble(addr) -> value;
bool PatchMemory(addr, { 0x00, 0x10, ... });             // снять защиту + записать список байт
```
Выделение / цепочки указателей / сканирование:
```cpp
uintptr_t AllocateMemory(size_t size, bool executable = false);
bool      FreeMemory(uintptr_t ptr);
uintptr_t ReadPointerChain(uintptr_t base, std::initializer_list<int> offsets);
uintptr_t PatternScan(const char* pattern, const char* soLib);
uintptr_t PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
std::vector<uintptr_t> FindAllPatterns(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
bool      ComparePattern(uintptr_t addr, const char* pattern);
```

### Патчинг инструкций (ARM)
```cpp
int  PlaceNOP(uintptr_t addr, size_t count = 1);
int  PlaceNOP4(uintptr_t addr, size_t count = 1);   // 4-байтные NOP
int  PlaceJMP(uintptr_t addr, uintptr_t dest);      // PlaceB — псевдоним PlaceJMP
int  PlaceRET(uintptr_t addr);
void PlaceBL (uintptr_t addr, uintptr_t dest);
void PlaceBLX(uintptr_t addr, uintptr_t dest);
int  Redirect(uintptr_t addr, uintptr_t to);        // переход с сохранением стека+регистров
void PatchForThumb(bool forThumb);                  // режим патчинга thumb или arm
bool IsThumbAddr(uintptr_t addr);
uintptr_t GetBranchDest(uintptr_t addr);
bool HasAddrExecFlag(uintptr_t addr);
// arm64 redirect с сохранением регистра:
void RedirectReg(uintptr_t addr, uintptr_t to, bool doShortHook = false,
                 GlossRegisters::e_reg targetReg = GlossRegisters::X16);
```

### Хуки (низкоуровневые — предпочтительнее макросы из §8)
```cpp
bool Hook   (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookPLT(void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookB  (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBL (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBLX(void* targetAddr, void* newFn, void** orgFn = NULL);
void HookVtableFunc(void* ptr, unsigned funcNum, void* newFn, void** orgFn = NULL, bool instantiate = false);
void HookVtableFunc(void* ptr, unsigned funcNum, unsigned count, void* newFn, void** orgFn = NULL, bool instantiate = false);
// inline-хук на уровне регистров:
PHookHandle HookInline(void* fnAddress, HookWithRegistersFn newFn, bool doShortHook = false);
void ToggleHook(PHookHandle hook, bool enable);
void DeHook(PHookHandle hook);
// хелперы vtable:
void** GetVtable(void* ptr);  void SetVtable(void* ptr, void** vtable);
```

### Пути
```cpp
const char* GetDataPath();            // /data/data/<pkg>/...
const char* GetConfigPath();
const char* GetAndroidDataPath();     // /sdcard/Android/data/<pkg>/files/...
const char* GetAndroidDataRootPath(); // /sdcard/Android/data/<pkg>/... (без /files/)
const char* GetInternalPath();        // /sdcard/
const char* GetInternalModsPath();    // /sdcard/AMLMods/<game>/
const char* GetNativeLibsPath();      // /data/app/<...>/lib/arm*/
```

### Файлы
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

### Ассеты (ассеты APK)
```cpp
AAssetManager* GetAssetManager();
void*  OpenAsset(const char* path, int mode = AASSET_MODE_BUFFER);
void   CloseAsset(void* asset);
size_t GetAssetSize(void* asset);
const void* GetAssetBuffer(void* asset);
void   ReadAsset(void* asset, void* buf, size_t count);
```

### Интерфейс / устройство / система
```cpp
void  ShowToast(bool longerDuration, const char* fmt, ...);
void  ShowDialog(const char* title, const char* message, const char* buttonText = NULL, int styleResource = 0);
void  DoVibro(int msTime);                         // ~20мс = лёгкая вибрация
void  DoVibro(jlong* pattern, int patternItems);   // например DEFAULT_VIBRATE_PATTERN, g_VibroPattern_Weak
void  CancelVibro();
float GetBatteryLevel();                           // 0.0 .. 100.0
void  GetDisplaySize(int* w = NULL, int* h = NULL);
int   GetAndroidVersion();                         // уровень API
int   GetAndroidSystemResID(const char* innerClass, const char* fieldName); // напр. "style","Theme_DeviceDefault_Alert"
void  OpenURL(const char* url);
int   GetCPUCores();
bool  IsMainThread();
void  GetNewsString(char* buf, size_t len);
```

### Сеть / загрузки
```cpp
bool DownloadFile(const char* url, const char* saveto);
bool DownloadFileToData(const char* url, char* out, size_t outLen);
bool IsFileDownloadsEnabled();
int  GetDownloadTimeout();
int  GetLatestDownloadErrorCode();
```

### JNI / мост в Java
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
Плюс шаблонные удобства (C++17, определены в `iaml.h`):
```cpp
T    CallJavaMethod<T>(jobject obj, const char* method, const char* sig, args...);  // авто Call*Method
auto CallVirtual<Ret>(void* instance, int index, args...);                          // вызов vtable[index]
```

### MLS — Mod Local Storage (постоянное хранилище ключ/значение)
Простое типизированное хранилище отдельно от `.ini`-конфигурации:
```cpp
void MLSSetInt/MLSSetFloat/MLSSetInt64/MLSSetStr(key, val);   void MLSSetBool(key, val);
bool MLSGetInt/MLSGetFloat/MLSGetInt64(key, &out);            bool MLSGetBool(key, &out);
bool MLSGetStr(key, buf, len);
bool MLSHasValue(key);   void MLSDeleteValue(key);   void MLSSaveFile();
bool IsMLSInManualSave();
```

---

## 8. Хуки функций (макросы)

Объявите хук макросом `DECL_HOOK*`, затем установите его макросом `HOOK*`. Макрос создаёт две вещи: указатель на **оригинал** (с тем именем, что вы написали) и ваше тело-замену с именем `HookOf_<name>`. Вызывайте оригинал внутри своего тела по этому имени.

### Макросы объявления
```cpp
DECL_HOOK(ret, name, args...)   // возвращает `ret`
DECL_HOOKv(name, args...)       // void
DECL_HOOKb(name, args...)       // bool
DECL_HOOKi(name, args...)       // int
DECL_HOOKp(name, args...)       // void*
```
Варианты с префиксом `S` (`SDECL_HOOK*`) делают указатели `static`; с префиксом `SI` (`SIDECL_HOOK*`) — `static inline`.

### Макросы установки
```cpp
HOOK(name, fnAddr)                 // inline-хук по адресу
HOOKSYM(name, libHandle, "symbol") // inline-хук, символ резолвится за вас
HOOKPLT(name, fnAddr)              // хук PLT/GOT
HOOKB(name, fnAddr)                // хук B
HOOKBL(name, fnAddr)              // хук BL
HOOKBLX(name, fnAddr)              // хук BLX
```
> В заголовке существует макрос `HOOK_IL2CPP`, но IL2CPP не готов (см. §13); он намеренно не документируется здесь.

### Пример
```cpp
// функция void
DECL_HOOKv(InitialiseRenderWare)
{
    InitialiseRenderWare();              // вызов оригинала
    // ... ваш код ...
}

// функция int с аргументами
DECL_HOOK(int, CalcDamage, void* self, int raw)
{
    int dmg = CalcDamage(self, raw);     // оригинал
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

## 9. Конфигурация: `cfg->` (`Config`)

Доступна как глобальный `cfg`, если объявить мод через `MYMODCFG`/`MYMODCFGNAME` и подключить `#include <mod/config.h>`. **Секция по умолчанию — `"Preferences"`.**

### Быстрые геттеры (без выделения памяти, без очистки)
```cpp
const char* GetString(key, defValue, section = "Preferences");   // указатель временный
int         GetInt   (key, defValue, section = "Preferences");
float       GetFloat (key, defValue, section = "Preferences");
bool        GetBool  (key, defValue, section = "Preferences");
rgba_t      GetColor (key, defValue, section = "Preferences");
// безопасное копирование строки в ваш буфер:
void        GetString(char* out, size_t outLen, key, defValue, section = "Preferences");
```
Пример:
```cpp
int   horns  = cfg->GetInt  ("HornsThreshold", 2000, "Settings");
float cruise = cfg->GetFloat("CruiseSpeed",    15.0f, "Settings");
bool  on     = cfg->GetBool ("Enabled",        true,  "Settings");
```

### Цвета
```cpp
struct rgba_t { unsigned char r,g,b,a; /* + .value, .v[4], псевдонимы x/y/z/w */ };
rgba_t c = cfg->GetColor("TextColor", rgba_t(255,0,0), "Settings");
```

### Привязанные записи (`ConfigEntry`) — для значений, которые читаете/ограничиваете/пишете обратно
`Bind(...)` возвращает `ConfigEntry*` в куче (удаляйте сами или используйте `ClearLast()`):
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
int   Clamp(int min, int max);     // возвращает величину переполнения, помечает конфиг изменённым
float Clamp(float min, float max);
void  Reset();                     // вернуть к значению по умолчанию
bool  LoadedUndefault();           // было ли в файле не-дефолтное значение?
```
Пример с ограничением и сохранением:
```cpp
ConfigEntry* e = cfg->Bind("MaxSpeed", 120, "Settings");
e->Clamp(0, 300);
int maxSpeed = e->GetInt();
delete e;                 // или cfg->ClearLast();
cfg->Save();              // сохранить изменения
```

### Прочее
```cpp
void  Init();  void Save();
bool  HasSection(section);          bool HasKey(section, key);            // AML 1.4.0+
bool  HasSectionComment(section);   bool HasKeyComment(section, key);     // AML 1.4.0+
bool  IsValueChanged();
void  ClearLast();                  // удалить последнюю запись Bind()
```

### Удобные ограничители (свободные функции, `amlmod.h`)
```cpp
int   clampint(min, max, v);    void clampint(min, max, int* v);
float clampfloat(min, max, v);  void clampfloat(min, max, float* v);
```

---

## 10. Лёгкая конфигурация: `ICFG` (необязательно)

`icfg.h` предоставляет более компактный INI-интерфейс, который избегает `<fstream>` (мод меньше примерно на 370 КБ). Используйте его только если вы и так не тянете `fstream`. Получается через `GetCFGInterface()` (глобальный `icfg`):

```cpp
ICFG* c = GetCFGInterface();
void* ini = c->InitIniPointer();
c->ParseInputStream(ini, "/sdcard/.../my.ini");

char buf[64];
if(c->GetValueFrom(ini, "Settings", "Name", buf, sizeof(buf)))  // безопасная перегрузка
    logger->Info("Name=%s", buf);

c->SetValueTo(ini, "Settings", "Enabled", "true");
c->GenerateToOutputStream(ini, "/sdcard/.../my.ini");
// также: HasSection / HasKey / HasSectionComment / HasKeyComment   (AML 1.4.0)
```

`Config` из `config.h` автоматически использует `ICFG` под капотом, если определён `_ICFG` (т.е. подключён `icfg.h`). Выбирайте **один** подход к конфигурации на мод.

---

## 11. Логирование: `logger->` (`Logger`)

```cpp
logger->SetTag("My Cool Mod");           // тег (макс. 30 символов), виден в logcat
logger->Info ("base = 0x%X", base);      // в стиле printf
logger->Error("missing symbol: %s", s);
logger->Print(LogP_Warn, "fmt %d", x);   // явный приоритет
logger->PrintTag(LogP_Info, "OtherTag", "fmt");
logger->ToggleOutput(false);             // отключить вывод
bool on = logger->HasOutput();
```
Приоритеты: `LogP_Verbose, LogP_Debug, LogP_Info, LogP_Warn, LogP_Error, LogP_Fatal, LogP_Silent`.
Определите `-DNOLOGGING`, чтобы вырезать всё логирование (`HasOutput()` станет константой `false`). Максимальный буфер форматирования — 2048/4096 байт.

---

## 12. Вспомогательные заголовки

### `localref.h` — `LocalRef<T>` (RAII)
Вызывает деинициализатор при выходе из области видимости. Отлично для локальных ссылок JNI, `malloc`, `FILE*`, ассетов AML.
```cpp
JNIEnv* env = aml->GetJNIEnvironment();
LocalRef<jclass> cls(env->FindClass("java/lang/String"),
                     [env](jclass c){ env->DeleteLocalRef(c); });
// используйте cls (неявно приводится к T); освобождается в конце области
LocalRef<void*> a(aml->OpenAsset("data.bin"), [](void* x){ aml->CloseAsset(x); });
```
API: `Get()`, `Valid()`, `Detach()` (отдать владение), `operator T()`, `explicit operator bool`.

### `scopeguard.h` — `DEFER(...)`
Выполняет произвольный код очистки при выходе из области. В отличие от `LocalRef`, **не** оборачивает переменную.
```cpp
jclass cls = env->FindClass("java/lang/String");
DEFER( env->DeleteLocalRef(cls) );   // выполнится на любом пути выхода
if(bail) return;                      // DEFER срабатывает здесь
```

### `listitem.h` — интрузивный связный список
Список на макросах, используемый внутри AML. Определите тип узла через `LIST_START(Name) ... LIST_END()` (+ `LIST_INITSTART/INITEND` для конструктора), затем `Push(&list)`, `Remove(&list)`, `Count()`, `First()`, `Last()`, `InList(&list)`. Итерация:
```cpp
LIST_FOR(list)        { /* `item` — узел; безопасно для Remove/Push */ }
LIST_FOR_FAST(list)   { /* быстрее; НЕ изменяйте список */ }
LIST_FOR_REVERSE(list){ /* с конца к началу */ }
```

---

## 13. IL2CPP / моды для Unity

**Поддержка IL2CPP НЕ готова и НЕ включена в AML.** `il2cpp.h` лишь предварительно объявляет несколько имён типов IL2CPP (`IL2Object`, `IL2Class`, `MethodInfo`, `FieldInfo` и т.д.), чтобы посторонний код компилировался. В составе AML нет ни дампера, ни рантайм-резолвера, ни SDK, и этот документ не даёт руководства по моддингу IL2CPP.

Если хотите модить игру на Unity/IL2CPP, всю IL2CPP-часть нужно делать самостоятельно своими внешними инструментами (дамп игры, получение заголовков и адресов методов/полей, резолв методов во время выполнения и т.д.). AML предоставляет только обобщённые примитивы хуков и патчинга памяти, описанные в этом документе; как только вы сами получили сырой адрес, хукайте его как любую другую нативную функцию.

---

## 14. Полный пример мода

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
    GameUpdate();                 // сначала оригинал
    if(gEnabled) { /* логика на кадр */ }
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

    // Конфиг (секция по умолчанию "Preferences"; здесь используется своя секция)
    gEnabled  = cfg->GetBool("Enabled",  true, "Settings");
    ConfigEntry* spd = cfg->Bind("MaxSpeed", 120, "Settings");
    spd->Clamp(0, 300);
    gMaxSpeed = spd->GetInt();
    delete spd;
    cfg->Save();
    logger->Info("Enabled=%d MaxSpeed=%d", gEnabled, gMaxSpeed);

    // Патч + хуки
    aml->PlaceNOP(pGame + BYBIT(0x100000, 0x1000000), 1);
    HOOK(GameUpdate, pGame + BYBIT(0x200000, 0x2000000));
    HOOK(CalcScore,  pGame + BYBIT(0x300000, 0x3000000));

    // Предоставить интерфейс другим модам (необязательно)
    // RegisterInterface("IExample", &g_Example);

    logger->Info("Example Mod loaded.");
}

ON_MOD_UNLOAD()
{
    logger->Info("Example Mod unloaded.");
}
```

---

## 15. Краткий справочник

| Элемент | Назначение |
|---|---|
| `MYMOD / MYMODCFG / MYMODCFGNAME` | Объявление мода (+ опциональный `cfg`) |
| `NEEDGAME(pkg)` | Ограничить мод одним пакетом |
| `BEGIN_DEPLIST / ADD_DEPENDENCY[_VER] / END_DEPLIST` | Зависимости |
| `ON_MOD_PRELOAD / ON_MOD_LOAD / ON_ALL_MODS_LOAD / ON_MOD_UNLOAD` | Жизненный цикл |
| `ON_GAME_CRASH / ON_NEW_INTERFACE / UPDATER_URL` | Опциональные колбэки |
| `GetInterface(name)` / `RegisterInterface(name, ptr)` | Система интерфейсов (глобальные функции, из `interface.h`) |
| `WRAP_INTERFACE(name, var)` | Сахар для RegisterInterface |
| `aml->GetLib / GetLibLength / GetLibHandle` | Поиск библиотеки |
| `aml->GetSym(handle|addr, "sym")` | Резолв символа |
| `aml->Unprot / Write[8..64] / Read[8..64] / PatchMemory` | Чтение/запись памяти |
| `aml->PlaceNOP / PlaceJMP(=PlaceB) / PlaceRET / PlaceBL / Redirect` | Патчинг инструкций |
| `aml->Hook / HookPLT / HookB/BL/BLX / HookVtableFunc / HookInline` | Ядро хуков |
| `aml->PatternScan / FindAllPatterns / ReadPointerChain` | Сканирование |
| `aml->GetJNIEnvironment / CallStaticJavaMethod / GetCurrentActivity` | Мост JNI |
| `aml->MLS*` | Постоянное хранилище ключ/значение |
| `aml->ShowToast / ShowDialog / DoVibro / GetBatteryLevel` | Устройство/интерфейс |
| `aml->GetAppVersionName / GetAppVersionCode / GetApkPath / GetApkMD5` | Идентификация/версия целевого приложения |
| `DECL_HOOK[v/b/i/p]` + `HOOK[SYM/PLT/B/BL/BLX]` | Макросы хуков |
| `cfg->GetInt/GetFloat/GetBool/GetString/GetColor(key, def, section="Preferences")` | Чтение конфига |
| `cfg->Bind(...) -> ConfigEntry` + `Clamp/Set*/Reset` + `cfg->Save()` | Привязанный конфиг |
| `icfg->...` (`GetCFGInterface()`) | Лёгкий INI |
| `logger->SetTag/Info/Error/Print` | Логирование |
| `LocalRef<T>` / `DEFER(...)` | Очистка RAII |
| `BYBIT(v32, v64)` / `AML32` | Значения по ABI |
| `IAML_VER` | Проверка версии заголовков/API |

### Проверка версии
```cpp
#include <mod/amlmod.h>
#if !defined(IAML_VER) || IAML_VER < 01040000
    #error "You need to update your MOD folder to 1.4.0!"
#endif
```

---

## 16. Памятка

- AML загружается **после** библиотеки игры; патчите уже отображённую библиотеку, не хукайте `JNI_OnLoad`.
- `GetInterface` / `RegisterInterface` — глобальные функции, **а не** методы `aml`.
- `aml` заполняется автоматически макросом `MYMOD` через `GetInterface("AMLInterface")` с приоритетом инициализации 101 — всегда готов в колбэках.
- Секция конфига по умолчанию — `"Preferences"`. Результаты `Bind()` размещены в куче; освобождайте через `delete` или `cfg->ClearLast()`.
- Лучшие живые примеры: официальные моды на https://github.com/AndroidModLoader?tab=repositories и сами заголовки.
