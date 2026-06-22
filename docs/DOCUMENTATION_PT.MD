# AndroidModLoader (AML) — Documentação de Desenvolvimento de Mods

> Repositório: https://github.com/RusJJ/AndroidModLoader
> Mods de exemplo: https://github.com/AndroidModLoader?tab=repositories
> Discord: https://discord.gg/2MY7W39kBg
>
> Este documento foi escrito com base nos headers reais dos mods (`amlmod.h`, `iaml.h`, `interface.h`, `config.h`, `icfg.h`, `logger.h`, `localref.h`, `scopeguard.h`, `listitem.h`, `il2cpp.h`). O conjunto de headers corresponde a **IAML_VER `01040000` (AML 1.4.0)**.

---

## 1. Visão geral e ordem de carregamento

O AML carrega mods nativos `.so` dentro de um aplicativo alvo e oferece um sistema de interfaces no estilo da Source Engine para hooking, patch de memória, configuração, acesso a JNI e APIs entre mods.

**Fato importante sobre a ordem de carregamento:** o AML é carregado **depois** que a biblioteca principal do jogo já foi carregada (depois do `JNI_OnLoad` e da própria inicialização do jogo). Isso significa que, quando seus `OnModPreLoad` / `OnModLoad` rodam, `aml->GetLib("libGame.so")` já retorna um endereço base válido. Você não faz hook no `JNI_OnLoad`; você aplica patch na biblioteca já mapeada na memória.

A ABI é apenas ARM. Os headers definem exatamente um destes:
```cpp
#ifdef AML32   // armeabi-v7a (__arm__)
#else          // AML64, arm64-v8a (__aarch64__)
#endif
```
A macro `BYBIT(v32, v64)` escolhe um valor por ABI:
```cpp
uintptr_t off = pGame + BYBIT(0x123456, 0x1234567);
```

### Localização dos mods
```
/sdcard/Android/data/<game.package>/mods/      (externo)
/sdcard/AMLMods/<game>/                          (interno padrão, ver GetInternalModsPath)
```

---

## 2. Compilando um mod

Um mod é uma biblioteca compartilhada do NDK. Você **não** precisa fazer link com o AML — o `interface.h` encontra o AML em tempo de execução via `dlopen("libAML.so")`. Basta compilar com os headers de mod do AML (colocados em `include/mod/`).

`Application.mk`:
```makefile
APP_ABI := armeabi-v7a arm64-v8a
APP_PLATFORM := android-21
APP_STL := c++_static
APP_CPPFLAGS += -std=c++17     # necessário para os helpers de template de JNI
```

`Android.mk`:
```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE     := mymod
LOCAL_SRC_FILES  := main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_LDLIBS     := -llog
# Opcional: desativar todo o log para reduzir o tamanho do mod
# LOCAL_CXXFLAGS += -DNOLOGGING
include $(BUILD_SHARED_LIBRARY)
```
Compile com `ndk-build`.

---

## 3. Declarando um mod

Inclua o header principal e declare os metadados com uma macro.

```cpp
#include <mod/amlmod.h>   // já traz iaml.h -> interface.h, jni.h, etc.
#include <mod/logger.h>
#include <mod/config.h>   // somente se precisar do cfg

MYMOD(net.author.mymod, My Cool Mod, 1.0, AuthorName)
```

### O que o `MYMOD` gera para você
A macro (`amlmod.h`) se expande em:
- um `ModInfo modinfoLocal(...)` estático e um `ModInfo* modinfo` global,
- o export `__GetModInfo()` que o AML chama para ler seus metadados,
- o global `IAML* aml`,
- um stub de inicialização estática (init priority 101) que roda **antes** dos seus callbacks e faz:
  ```cpp
  aml = (IAML*)GetInterface("AMLInterface");
  ```

Portanto, o `aml` já está pronto quando qualquer ponto de entrada roda. Você nunca o obtém manualmente.

### Variantes da macro
```cpp
MYMOD(_guid, _name, _version, _author)
    // base; cria modinfo + aml

MYMODCFG(_guid, _name, _version, _author)
    // MYMOD + um `Config* cfg` global cujo .ini recebe o nome de _guid

MYMODCFGNAME(_guid, _name, _version, _author, _cfgname)
    // MYMOD + `cfg` cujo .ini recebe o nome de _cfgname (nome de arquivo personalizado)
```
Os argumentos são tokens sem aspas (a macro os transforma em string). O GUID é convertido para minúsculas automaticamente. A versão é dividida em `major.minor.revision.build`.

### Restringir a um jogo específico (opcional)
```cpp
NEEDGAME(com.rockstargames.gtasa)   // o mod só carrega dentro deste pacote
```

### Lendo os próprios metadados
`modinfo` expõe: `GUID()`, `Name()`, `VersionString()`, `Author()`, `Major()`, `Minor()`, `Revision()`, `Build()`.

---

## 4. Dependências

Declaradas em um bloco; o AML usa isso para a ordem de carregamento e para falhar de forma definitiva quando um mod obrigatório está ausente.

```cpp
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)      // requer AML >= 1.4.0
    ADD_DEPENDENCY_VER(net.rusjj.cleolib, 2.0.1.6)
    ADD_DEPENDENCY(net.rusjj.basslib)             // qualquer versão
END_DEPLIST()
```
- `ADD_DEPENDENCY(guid)` — requer um mod, qualquer versão.
- `ADD_DEPENDENCY_VER(guid, version)` — requer um mod igual ou acima de uma versão.

**Dependência fraca (soft):** não a coloque na lista; em vez disso, verifique em tempo de execução com `aml->HasMod(...)` / `aml->HasModOfVersion(...)` / `aml->HasModOfBiggerVersion(...)`, ou simplesmente tente `GetInterface(...)` e trate o `NULL`.

---

## 5. Pontos de entrada (callbacks)

Cada macro se expande em um export `extern "C"` que o AML procura. Defina os que você precisar.

```cpp
ON_MOD_PRELOAD()      // void OnModPreLoad()      — o mais cedo; biblioteca já mapeada
ON_MOD_LOAD()         // void OnModLoad()         — entrada principal: hooks, config, símbolos
ON_ALL_MODS_LOAD()    // void OnAllModsLoaded()   — depois que todos os mods terminaram de carregar
ON_MOD_UNLOAD()       // void OnModUnload()       — limpeza (execução não garantida)
ON_GAME_CRASH()       // void OnGameCrash(const char* library, int sig, int code,
                      //                  uintptr_t libaddr, mcontext_t* mcontext)
ON_NEW_INTERFACE()    // void OnInterfaceAdded(const char* name, const void* ptr)
UPDATER_URL()         // const char* OnUpdaterURLRequested()  — retorne a URL de atualização
```

Dentro deles você já tem `aml`, `logger` e (se usar `MYMODCFG`) `cfg`.

Use `ON_ALL_MODS_LOAD` quando precisar da interface de outro mod que pode registrar tarde, ou use `ON_NEW_INTERFACE` para reagir no momento em que uma interface nomeada aparece.

```cpp
ON_MOD_PRELOAD()
{
    logger->SetTag("My Cool Mod");
}

ON_MOD_LOAD()
{
    uintptr_t pGame = aml->GetLib("libGTASA.so");
    if(!pGame) { logger->Error("game lib missing"); return; }
    // hooks / patches / config aqui
}
```

---

## 6. O sistema de interfaces: `GetInterface` / `RegisterInterface`

Este é o mecanismo central de extensibilidade do AML. **São funções independentes definidas em `interface.h`, e não métodos de `aml`.** Internamente, elas fazem `dlopen("libAML.so")` e chamam os exports do AML `GetInterface` / `CreateInterface`.

```cpp
void* GetInterface(const char* szInterfaceName);              // obtém uma interface registrada
void  RegisterInterface(const char* szInterfaceName, void* p);// expõe a sua própria
```

Uma interface é apenas um ponteiro (normalmente para uma struct/classe de ponteiros de função, ou para uma classe com vtable) armazenado sob um nome em string escolhido por você. O provedor e o consumidor só precisam concordar com esse nome exato e com o layout da struct/classe por trás dele. Como nomear as interfaces é totalmente decisão sua.

O próprio `aml` é obtido assim: a macro `MYMOD` chama `GetInterface("AMLInterface")` por você.

### Macro de conveniência
```cpp
#define WRAP_INTERFACE(name, var)  RegisterInterface(#name, var)
WRAP_INTERFACE(IMyApi, &g_MyApi);   // == RegisterInterface("IMyApi", &g_MyApi)
```

### A) Fornecendo uma interface
Disponibilize um header para os consumidores conhecerem o layout, preencha uma struct e registre-a.

```cpp
// imyapi.h  (compartilhado com os consumidores)
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

Uma interface baseada em classe com vtable também funciona (os consumidores chamam os virtuais diretamente):
```cpp
class IMyApi { public: virtual int Add(int,int)=0; virtual void DoThing(const char*)=0; };
```

### B) Consumindo uma interface
```cpp
#include <ibass.h>     // header fornecido pelo mod BASS
IBASS* BASS = NULL;

MYMOD(net.author.user, BASS User, 1.0, AuthorName)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.basslib, 1.2)   // garante que o BASS carregue primeiro
END_DEPLIST()

ON_MOD_LOAD()
{
    BASS = (IBASS*)GetInterface("IBASS");
    if(!BASS) { logger->Error("IBASS not found"); return; }
    // BASS->...
}
```

### C) Reagindo a interfaces tardias
```cpp
ON_NEW_INTERFACE()   // OnInterfaceAdded(name, ptr)
{
    if(!strcmp(name, "IBASS"))
        BASS = (IBASS*)ptr;
}
```

### Observações
- Se você depende de forma rígida do provedor (`ADD_DEPENDENCY*`), a interface já existe no seu `OnModLoad`. Para provedores opcionais, verifique o `NULL`.
- O nome exato da interface (`"IBASS"` acima é só um exemplo) é o que o provedor escolheu. Consulte o header/documentação do provedor e use essa string literalmente.

---

## 7. A interface `aml->` (`IAML`)

`aml` é o núcleo do AML. Abaixo estão os métodos agrupados por finalidade. As assinaturas são exatas conforme `iaml.h`. A disponibilidade está anotada com a versão do AML que introduziu cada bloco.

### Informações do jogo / mods
```cpp
const char* GetCurrentGame();                 // pacote do jogo atual (possivelmente falsificado)
const char* GetRealCurrentGame();             // pacote real
const char* GetAppVersionName();              // versionName do app/jogo alvo
int64_t     GetAppVersionCode();              // long version code do app/jogo alvo
const char* GetApkPath();                     // caminho do APK de origem
const char* GetApkMD5();                      // MD5 do APK de origem, calculado sob demanda
bool        IsGameFaked();
bool        HasMod(const char* guid);
bool        HasModOfVersion(const char* guid, const char* version);
bool        HasModOfBiggerVersion(const char* guid, const char* version);
int         GetModsLoadedCount();
int         GetFailedModsCount();
void        ListMods(ListModsCallback cb, void* data = NULL, bool startWithLatest = false);
const char* GetFeatures();                    // string de recursos do build
```

### Bibliotecas e símbolos
```cpp
uintptr_t   GetLib(const char* szLib);                 // endereço base de uma .so carregada
uintptr_t   GetLibLength(const char* szLib);           // tamanho mapeado
void*       GetLibHandle(const char* soLib);           // handle no estilo dlopen
void*       GetLibHandle(uintptr_t addr);
uintptr_t   GetSym(void* handle, const char* sym);     // resolve por handle
uintptr_t   GetSym(uintptr_t libAddr, const char* sym);// resolve por ENDEREÇO da biblioteca
```
Helpers de xDL (sempre presentes, retornam 0 se o xDL não for usado):
```cpp
bool IsCorrectXDLHandle(void*); uintptr_t GetLibXDL(void*);
uintptr_t GetAddrBaseXDL(uintptr_t); size_t GetSymSizeXDL(void*);
const char* GetSymNameXDL(void*);
```
Macros de atribuição por símbolo:
```cpp
SET_TO(var, addr);              // *(void**)&var = (void*)addr
SET_TO_PTR(var, ptraddr);       // dereferencia e depois atribui
SETSYM_TO(var, lib, "sym");     // var = aml->GetSym(lib, "sym")
SETSYM_TO_PTR(var, lib, "sym");
AS_ADDR(var);                   // *(uintptr_t*)&var
```

### Memória: desproteger / escrever / ler
```cpp
int  Unprot(uintptr_t addr, size_t len = PAGE_SIZE);
void Write(uintptr_t dest, uintptr_t src, size_t size);
void Read (uintptr_t src,  uintptr_t dest, size_t size);
```
Helpers inline tipados (sem tamanho manual):
```cpp
Write8/16/32/64(addr, v);  WriteFloat/WriteDouble(addr, v);
WriteAddr(addr, ptr);      Write(dest, "string");        // e também Write(dest, str, size)
Read8/16/32/64(addr) -> value;  ReadFloat/ReadDouble(addr) -> value;
bool PatchMemory(addr, { 0x00, 0x10, ... });             // desprotege + escreve uma lista de bytes
```
Alocação / cadeias de ponteiros / varredura:
```cpp
uintptr_t AllocateMemory(size_t size, bool executable = false);
bool      FreeMemory(uintptr_t ptr);
uintptr_t ReadPointerChain(uintptr_t base, std::initializer_list<int> offsets);
uintptr_t PatternScan(const char* pattern, const char* soLib);
uintptr_t PatternScan(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
std::vector<uintptr_t> FindAllPatterns(const char* pattern, uintptr_t libStart, uintptr_t scanLen);
bool      ComparePattern(uintptr_t addr, const char* pattern);
```

### Patch de instruções (ARM)
```cpp
int  PlaceNOP(uintptr_t addr, size_t count = 1);
int  PlaceNOP4(uintptr_t addr, size_t count = 1);   // NOPs de 4 bytes
int  PlaceJMP(uintptr_t addr, uintptr_t dest);      // PlaceB é um alias de PlaceJMP
int  PlaceRET(uintptr_t addr);
void PlaceBL (uintptr_t addr, uintptr_t dest);
void PlaceBLX(uintptr_t addr, uintptr_t dest);
int  Redirect(uintptr_t addr, uintptr_t to);        // salto mantendo a mesma pilha+registradores
void PatchForThumb(bool forThumb);                  // modo de patch thumb vs arm
bool IsThumbAddr(uintptr_t addr);
uintptr_t GetBranchDest(uintptr_t addr);
bool HasAddrExecFlag(uintptr_t addr);
// redirect arm64 preservando registrador:
void RedirectReg(uintptr_t addr, uintptr_t to, bool doShortHook = false,
                 GlossRegisters::e_reg targetReg = GlossRegisters::X16);
```

### Hooking (baixo nível — prefira as macros da §8)
```cpp
bool Hook   (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookPLT(void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookB  (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBL (void* targetAddr, void* newFn, void** orgFn = NULL);
bool HookBLX(void* targetAddr, void* newFn, void** orgFn = NULL);
void HookVtableFunc(void* ptr, unsigned funcNum, void* newFn, void** orgFn = NULL, bool instantiate = false);
void HookVtableFunc(void* ptr, unsigned funcNum, unsigned count, void* newFn, void** orgFn = NULL, bool instantiate = false);
// hook inline em nível de registradores:
PHookHandle HookInline(void* fnAddress, HookWithRegistersFn newFn, bool doShortHook = false);
void ToggleHook(PHookHandle hook, bool enable);
void DeHook(PHookHandle hook);
// helpers de vtable:
void** GetVtable(void* ptr);  void SetVtable(void* ptr, void** vtable);
```

### Caminhos
```cpp
const char* GetDataPath();            // /data/data/<pkg>/...
const char* GetConfigPath();
const char* GetAndroidDataPath();     // /sdcard/Android/data/<pkg>/files/...
const char* GetAndroidDataRootPath(); // /sdcard/Android/data/<pkg>/... (sem /files/)
const char* GetInternalPath();        // /sdcard/
const char* GetInternalModsPath();    // /sdcard/AMLMods/<game>/
const char* GetNativeLibsPath();      // /data/app/<...>/lib/arm*/
```

### Arquivos
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

### Assets (assets do APK)
```cpp
AAssetManager* GetAssetManager();
void*  OpenAsset(const char* path, int mode = AASSET_MODE_BUFFER);
void   CloseAsset(void* asset);
size_t GetAssetSize(void* asset);
const void* GetAssetBuffer(void* asset);
void   ReadAsset(void* asset, void* buf, size_t count);
```

### UI / dispositivo / sistema
```cpp
void  ShowToast(bool longerDuration, const char* fmt, ...);
void  ShowDialog(const char* title, const char* message, const char* buttonText = NULL, int styleResource = 0);
void  DoVibro(int msTime);                         // ~20ms = vibração leve
void  DoVibro(jlong* pattern, int patternItems);   // ex.: DEFAULT_VIBRATE_PATTERN, g_VibroPattern_Weak
void  CancelVibro();
float GetBatteryLevel();                           // 0.0 .. 100.0
void  GetDisplaySize(int* w = NULL, int* h = NULL);
int   GetAndroidVersion();                         // nível da API
int   GetAndroidSystemResID(const char* innerClass, const char* fieldName); // ex.: "style","Theme_DeviceDefault_Alert"
void  OpenURL(const char* url);
int   GetCPUCores();
bool  IsMainThread();
void  GetNewsString(char* buf, size_t len);
```

### Rede / downloads
```cpp
bool DownloadFile(const char* url, const char* saveto);
bool DownloadFileToData(const char* url, char* out, size_t outLen);
bool IsFileDownloadsEnabled();
int  GetDownloadTimeout();
int  GetLatestDownloadErrorCode();
```

### Ponte JNI / Java
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
Além de conveniências por template (C++17, definidas em `iaml.h`):
```cpp
T    CallJavaMethod<T>(jobject obj, const char* method, const char* sig, args...);  // Call*Method automático
auto CallVirtual<Ret>(void* instance, int index, args...);                          // chama vtable[index]
```

### MLS — Mod Local Storage (armazenamento persistente chave/valor)
Um armazenamento tipado simples, separado da configuração `.ini`:
```cpp
void MLSSetInt/MLSSetFloat/MLSSetInt64/MLSSetStr(key, val);   void MLSSetBool(key, val);
bool MLSGetInt/MLSGetFloat/MLSGetInt64(key, &out);            bool MLSGetBool(key, &out);
bool MLSGetStr(key, buf, len);
bool MLSHasValue(key);   void MLSDeleteValue(key);   void MLSSaveFile();
bool IsMLSInManualSave();
```

---

## 8. Hooking de funções (macros)

Declare um hook com uma macro `DECL_HOOK*` e depois instale-o com uma macro `HOOK*`. A macro cria duas coisas: um ponteiro para a função **original** (com o nome exato que você escreveu) e o seu corpo de substituição, chamado `HookOf_<name>`. Chame a original de dentro do seu corpo usando esse nome.

### Macros de declaração
```cpp
DECL_HOOK(ret, name, args...)   // retorna `ret`
DECL_HOOKv(name, args...)       // void
DECL_HOOKb(name, args...)       // bool
DECL_HOOKi(name, args...)       // int
DECL_HOOKp(name, args...)       // void*
```
As variantes com prefixo `S` (`SDECL_HOOK*`) tornam os ponteiros `static`; as com prefixo `SI` (`SIDECL_HOOK*`) os tornam `static inline`.

### Macros de instalação
```cpp
HOOK(name, fnAddr)                 // hook inline por endereço
HOOKSYM(name, libHandle, "symbol") // hook inline, resolve o símbolo por você
HOOKPLT(name, fnAddr)              // hook PLT/GOT
HOOKB(name, fnAddr)                // hook de um B
HOOKBL(name, fnAddr)               // hook de um BL
HOOKBLX(name, fnAddr)              // hook de um BLX
```
> Existe uma macro `HOOK_IL2CPP` no header, mas o IL2CPP não está pronto (ver §13); ela é intencionalmente não documentada aqui.

### Exemplo
```cpp
// função void
DECL_HOOKv(InitialiseRenderWare)
{
    InitialiseRenderWare();              // chama a original
    // ... seu código ...
}

// função int com argumentos
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

## 9. Configuração: `cfg->` (`Config`)

Disponível como o global `cfg` quando você declara o mod com `MYMODCFG`/`MYMODCFGNAME` e usa `#include <mod/config.h>`. **A seção padrão é `"Preferences"`.**

### Getters rápidos (sem alocação, sem limpeza)
```cpp
const char* GetString(key, defValue, section = "Preferences");   // o ponteiro é transitório
int         GetInt   (key, defValue, section = "Preferences");
float       GetFloat (key, defValue, section = "Preferences");
bool        GetBool  (key, defValue, section = "Preferences");
rgba_t      GetColor (key, defValue, section = "Preferences");
// cópia segura de string para o seu buffer:
void        GetString(char* out, size_t outLen, key, defValue, section = "Preferences");
```
Exemplo:
```cpp
int   horns  = cfg->GetInt  ("HornsThreshold", 2000, "Settings");
float cruise = cfg->GetFloat("CruiseSpeed",    15.0f, "Settings");
bool  on     = cfg->GetBool ("Enabled",        true,  "Settings");
```

### Cores
```cpp
struct rgba_t { unsigned char r,g,b,a; /* + .value, .v[4], aliases x/y/z/w */ };
rgba_t c = cfg->GetColor("TextColor", rgba_t(255,0,0), "Settings");
```

### Entradas vinculadas (`ConfigEntry`) — para valores que você lê/limita/grava de volta
`Bind(...)` retorna um `ConfigEntry*` no heap (apague você mesmo, ou use `ClearLast()`):
```cpp
ConfigEntry* Bind(key, const char* def, section = "Preferences");
ConfigEntry* Bind(key, int   def,       section = "Preferences");
ConfigEntry* Bind(key, float def,       section = "Preferences");
ConfigEntry* Bind(key, bool  def,       section = "Preferences");
ConfigEntry* Bind(key, rgba_t def,      section = "Preferences");
```
API de `ConfigEntry`:
```cpp
int   GetInt();    void SetInt(int);
float GetFloat();  void SetFloat(float);
bool  GetBool();   void SetBool(bool);
const char* GetString();  void GetString(char* buf, size_t len);  void SetString(const char*);
rgba_t ParseColor();      void SetColor(rgba_t, bool asFloat = false);
int   Clamp(int min, int max);     // retorna o quanto excedeu, marca a config como alterada
float Clamp(float min, float max);
void  Reset();                     // volta ao padrão
bool  LoadedUndefault();           // havia um valor não-padrão no arquivo?
```
Exemplo com clamp + salvar:
```cpp
ConfigEntry* e = cfg->Bind("MaxSpeed", 120, "Settings");
e->Clamp(0, 300);
int maxSpeed = e->GetInt();
delete e;                 // ou cfg->ClearLast();
cfg->Save();              // persiste as alterações
```

### Outros
```cpp
void  Init();  void Save();
bool  HasSection(section);          bool HasKey(section, key);            // AML 1.4.0+
bool  HasSectionComment(section);   bool HasKeyComment(section, key);     // AML 1.4.0+
bool  IsValueChanged();
void  ClearLast();                  // apaga a última entrada de Bind()
```

### Clamps de conveniência (funções livres, `amlmod.h`)
```cpp
int   clampint(min, max, v);    void clampint(min, max, int* v);
float clampfloat(min, max, v);  void clampfloat(min, max, float* v);
```

---

## 10. Configuração leve: `ICFG` (opcional)

`icfg.h` fornece uma interface de INI mais enxuta que evita `<fstream>` (mod ≈370 KB menor). Use somente se você ainda não estiver puxando `fstream`. Obtenha-a via `GetCFGInterface()` (global `icfg`):

```cpp
ICFG* c = GetCFGInterface();
void* ini = c->InitIniPointer();
c->ParseInputStream(ini, "/sdcard/.../my.ini");

char buf[64];
if(c->GetValueFrom(ini, "Settings", "Name", buf, sizeof(buf)))  // overload segura
    logger->Info("Name=%s", buf);

c->SetValueTo(ini, "Settings", "Enabled", "true");
c->GenerateToOutputStream(ini, "/sdcard/.../my.ini");
// também: HasSection / HasKey / HasSectionComment / HasKeyComment   (AML 1.4.0)
```

O `Config` de `config.h` usará o `ICFG` por baixo automaticamente se `_ICFG` estiver definido (ou seja, se `icfg.h` for incluído). Escolha **uma** abordagem de configuração por mod.

---

## 11. Log: `logger->` (`Logger`)

```cpp
logger->SetTag("My Cool Mod");           // tag (máx. 30 caracteres) exibida no logcat
logger->Info ("base = 0x%X", base);      // estilo printf
logger->Error("missing symbol: %s", s);
logger->Print(LogP_Warn, "fmt %d", x);   // prioridade explícita
logger->PrintTag(LogP_Info, "OtherTag", "fmt");
logger->ToggleOutput(false);             // silenciar
bool on = logger->HasOutput();
```
Prioridades: `LogP_Verbose, LogP_Debug, LogP_Info, LogP_Warn, LogP_Error, LogP_Fatal, LogP_Silent`.
Defina `-DNOLOGGING` para remover todo o log na compilação (`HasOutput()` vira `false` constante). O buffer máximo de formatação é de 2048/4096 bytes.

---

## 12. Headers utilitários

### `localref.h` — `LocalRef<T>` (RAII)
Executa um deleter ao sair do escopo. Ótimo para refs locais de JNI, `malloc`, `FILE*`, assets do AML.
```cpp
JNIEnv* env = aml->GetJNIEnvironment();
LocalRef<jclass> cls(env->FindClass("java/lang/String"),
                     [env](jclass c){ env->DeleteLocalRef(c); });
// use cls (converte implicitamente para T); liberado automaticamente no fim do escopo
LocalRef<void*> a(aml->OpenAsset("data.bin"), [](void* x){ aml->CloseAsset(x); });
```
API: `Get()`, `Valid()`, `Detach()` (abre mão da posse), `operator T()`, `explicit operator bool`.

### `scopeguard.h` — `DEFER(...)`
Executa qualquer código de limpeza ao sair do escopo. Diferente do `LocalRef`, ele **não** envolve uma variável.
```cpp
jclass cls = env->FindClass("java/lang/String");
DEFER( env->DeleteLocalRef(cls) );   // roda em qualquer caminho de return
if(bail) return;                      // o DEFER dispara aqui
```

### `listitem.h` — lista encadeada intrusiva
Lista duplamente encadeada construída por macros, usada internamente pelo AML. Defina um tipo de nó com `LIST_START(Name) ... LIST_END()` (+ `LIST_INITSTART/INITEND` para o construtor), e depois `Push(&list)`, `Remove(&list)`, `Count()`, `First()`, `Last()`, `InList(&list)`. Itere com:
```cpp
LIST_FOR(list)        { /* `item` é o nó; seguro para Remove/Push */ }
LIST_FOR_FAST(list)   { /* mais rápido; NÃO modifique a lista */ }
LIST_FOR_REVERSE(list){ /* de trás para frente */ }
```

---

## 13. IL2CPP / mods de Unity

**O suporte a IL2CPP NÃO está pronto e NÃO está incluído no AML.** `il2cpp.h` apenas declara-antecipadamente alguns nomes de tipos do IL2CPP (`IL2Object`, `IL2Class`, `MethodInfo`, `FieldInfo`, etc.) para que código não relacionado compile. Não há dumper, nem resolvedor em tempo de execução, nem SDK incluído no AML, e este documento não fornece um guia de modding de IL2CPP.

Se você quiser modar um jogo Unity/IL2CPP, terá que fazer toda a parte de IL2CPP por conta própria, com as suas próprias ferramentas externas (fazer o dump do jogo, obter headers e endereços de métodos/campos, resolver métodos em tempo de execução, etc.). O AML fornece apenas as primitivas genéricas de hooking e patch de memória descritas neste documento; assim que você resolver um endereço bruto por conta própria, faça o hook como em qualquer outra função nativa.

---

## 14. Mod de exemplo completo

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
    GameUpdate();                 // a original primeiro
    if(gEnabled) { /* lógica por frame */ }
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

    // Config (a seção padrão é "Preferences"; aqui usamos uma seção própria)
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

    // Expor uma interface para outros mods (opcional)
    // RegisterInterface("IExample", &g_Example);

    logger->Info("Example Mod loaded.");
}

ON_MOD_UNLOAD()
{
    logger->Info("Example Mod unloaded.");
}
```

---

## 15. Referência rápida

| Item | Finalidade |
|---|---|
| `MYMOD / MYMODCFG / MYMODCFGNAME` | Declarar o mod (+ `cfg` opcional) |
| `NEEDGAME(pkg)` | Restringir o mod a um pacote |
| `BEGIN_DEPLIST / ADD_DEPENDENCY[_VER] / END_DEPLIST` | Dependências |
| `ON_MOD_PRELOAD / ON_MOD_LOAD / ON_ALL_MODS_LOAD / ON_MOD_UNLOAD` | Ciclo de vida |
| `ON_GAME_CRASH / ON_NEW_INTERFACE / UPDATER_URL` | Callbacks opcionais |
| `GetInterface(name)` / `RegisterInterface(name, ptr)` | Sistema de interfaces (funções globais, de `interface.h`) |
| `WRAP_INTERFACE(name, var)` | Açúcar sintático para RegisterInterface |
| `aml->GetLib / GetLibLength / GetLibHandle` | Busca de biblioteca |
| `aml->GetSym(handle|addr, "sym")` | Resolução de símbolo |
| `aml->Unprot / Write[8..64] / Read[8..64] / PatchMemory` | Leitura/escrita de memória |
| `aml->PlaceNOP / PlaceJMP(=PlaceB) / PlaceRET / PlaceBL / Redirect` | Patch de instruções |
| `aml->Hook / HookPLT / HookB/BL/BLX / HookVtableFunc / HookInline` | Núcleo de hooking |
| `aml->PatternScan / FindAllPatterns / ReadPointerChain` | Varredura |
| `aml->GetJNIEnvironment / CallStaticJavaMethod / GetCurrentActivity` | Ponte JNI |
| `aml->MLS*` | Armazenamento persistente chave/valor |
| `aml->ShowToast / ShowDialog / DoVibro / GetBatteryLevel` | Dispositivo/UI |
| `aml->GetAppVersionName / GetAppVersionCode / GetApkPath / GetApkMD5` | Identidade/versão do app alvo |
| `DECL_HOOK[v/b/i/p]` + `HOOK[SYM/PLT/B/BL/BLX]` | Macros de hook |
| `cfg->GetInt/GetFloat/GetBool/GetString/GetColor(key, def, section="Preferences")` | Ler configuração |
| `cfg->Bind(...) -> ConfigEntry` + `Clamp/Set*/Reset` + `cfg->Save()` | Configuração vinculada |
| `icfg->...` (`GetCFGInterface()`) | INI leve |
| `logger->SetTag/Info/Error/Print` | Log |
| `LocalRef<T>` / `DEFER(...)` | Limpeza RAII |
| `BYBIT(v32, v64)` / `AML32` | Valores por ABI |
| `IAML_VER` | Verificação de versão de header/API |

### Verificação de versão
```cpp
#include <mod/amlmod.h>
#if !defined(IAML_VER) || IAML_VER < 01040000
    #error "You need to update your MOD folder to 1.4.0!"
#endif
```

---

## 16. Lembretes

- O AML carrega **depois** da biblioteca do jogo; aplique patch na biblioteca já mapeada, não faça hook no `JNI_OnLoad`.
- `GetInterface` / `RegisterInterface` são funções globais, **não** métodos de `aml`.
- O `aml` é preenchido automaticamente pela macro `MYMOD` via `GetInterface("AMLInterface")` na init priority 101 — sempre pronto nos callbacks.
- A seção padrão da configuração é `"Preferences"`. Os resultados de `Bind()` são alocados no heap; libere-os com `delete` ou `cfg->ClearLast()`.
- As melhores referências vivas: os mods oficiais em https://github.com/AndroidModLoader?tab=repositories e os próprios headers.
