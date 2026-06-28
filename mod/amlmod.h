#ifndef _AMLMOD
#define _AMLMOD

#include <stdio.h>
#include <ctype.h>
#include <cstring>
#include <stdlib.h>
#include <signal.h>
#include <math.h>

#if defined(__arm__) || defined(_WIN32)
    #define AML32
    #define BYBIT(__32val, __64val) (__32val)
#elif defined(__aarch64__) || defined(_WIN64)
    #define AML64
    #define BYBIT(__32val, __64val) (__64val)
#else
    #error This lib is supposed to work on ARM only!
#endif



#ifdef AML32
    #define PTRFMT "0x%08X"
    #define PTRNUMFMT "%u"
#else
    #define PTRFMT "0x%016lX"
    #define PTRNUMFMT "%lu"
#endif



#ifdef __clang__
    #define TARGET_ARM __attribute__((target("no-thumb-mode")))
    #define TARGET_THUMB  __attribute__((target("thumb-mode")))
#endif

#ifdef __GNUC__
    #define ASM_NAKED __attribute__((naked))
#else
    #define ASM_NAKED __declspec(naked)
#endif
#define EXPORT JNIEXPORT

// AML Mods, important stuff

#define MYMOD(_guid, _name, _version, _author)                          \
    static ModInfo modinfoLocal(#_guid, #_name, #_version, #_author);   \
    ModInfo* modinfo = &modinfoLocal;                                   \
    extern "C" JNIEXPORT ModInfo* __GetModInfo() { return modinfo; }    \
    IAML* aml = NULL;                                                   \
    struct AMLInitStub {                                                \
        AMLInitStub() {                                                 \
            aml = (IAML*)GetInterface("AMLInterface");                  \
        }                                                               \
    }; AMLInitStub amlStub __attribute__((init_priority(101))); // Highest init prio

#define MYMODCFG(_guid, _name, _version, _author)                       \
    MYMOD(_guid, _name, _version, _author);                             \
    static Config cfgLocal(#_guid);                                     \
    Config* cfg = &cfgLocal;

#define MYMODCFGNAME(_guid, _name, _version, _author, _cfgname)         \
    MYMOD(_guid, _name, _version, _author);                             \
    static Config cfgLocal(#_cfgname);                                  \
    Config* cfg = &cfgLocal;

#define NEEDGAME(_pkg_name)                                             \
    extern "C" JNIEXPORT const char* __INeedASpecificGame() { return #_pkg_name; }

// Dependencies!
#define BEGIN_DEPLIST()                                                 \
    static ModInfoDependency g_listDependencies[] = {

#define ADD_DEPENDENCY(_guid)                                           \
    {#_guid, ""},

#define ADD_DEPENDENCY_VER(_guid, _version)                             \
    {#_guid, #_version},

#define END_DEPLIST()                                                   \
    {"", ""} };                                                         \
    extern "C" JNIEXPORT ModInfoDependency* __GetDepsList() { return &g_listDependencies[0]; }

// Macros to stop forgetting stuff!
#define ON_MOD_PRELOAD()                                                \
    extern "C" JNIEXPORT void OnModPreLoad()

#define ON_MOD_LOAD()                                                   \
    extern "C" JNIEXPORT void OnModLoad()

#define ON_ALL_MODS_LOAD()                                              \
    extern "C" JNIEXPORT void OnAllModsLoaded()

#define ON_MOD_UNLOAD()                                                 \
    extern "C" JNIEXPORT void OnModUnload() /*Not guaranteed*/

#define ON_GAME_CRASH()                                                 \
    extern "C" JNIEXPORT void OnGameCrash(const char* library, int sig, int code, uintptr_t libaddr, mcontext_t* mcontext) /*Not guaranteed*/

#define UPDATER_URL()                                                   \
    extern "C" JNIEXPORT const char* OnUpdaterURLRequested()

#define ON_NEW_INTERFACE()                                              \
    extern "C" JNIEXPORT void OnInterfaceAdded(const char* name, const void* ptr)

// Helpers

#define MINIMUM_MD5_BUF_SIZE ( 32 + 1 )

struct MemChunk_t
{
    char* out;
    size_t out_len;
};
    
struct ModInfoDependency
{
    const char* szGUID;
    const char* szVersion;
};

struct ModVersion
{
    unsigned short major;
    unsigned short minor;
    unsigned short revision;
    unsigned short build;
};

// Should be faster than strncpy?
inline char *strxcpy(char* __restrict__ dst, const char* __restrict__ src, int len)
{
    if(!len) return NULL;
    while(--len && (*dst++ = *src++));
    if(!len)
    {
        *dst++ = 0;
        return (*src ? NULL : dst);
    }
    return dst;
}
inline int strindexof(const char* haystack, const char* needle)
{
    const char* ptr = strstr(haystack, needle);
    return ptr ? (int)(ptr - haystack) : -1;
}
inline bool strcontains(const char* str, const char* sub)
{
    return (strstr(str, sub) != NULL);
}
inline bool strstarts(const char* str, const char* prefix)
{
    size_t lenstr = strlen(str), lenpre = strlen(prefix);
    return (lenstr >= lenpre && strncmp(str, prefix, lenpre) == 0);
}
inline bool strends(const char* str, const char* suffix)
{
    size_t lenstr = strlen(str), lensuf = strlen(suffix);
    return (lenstr >= lensuf && strcmp(str + lenstr - lensuf, suffix) == 0);
}

inline int clampint(int min, int max, int v)
{
    if(v < min) return min;
    else if(v > max) return max;
    return v;
}
inline void clampint(int min, int max, int* v)
{
    if(*v < min) *v = min;
    else if(*v > max) *v = max;
}
inline float clampfloat(float min, float max, float v)
{
    if(v < min) return min;
    else if(v > max) return max;
    return v;
}
inline void clampfloat(float min, float max, float* v)
{
    if(*v < min) *v = min;
    else if(*v > max) *v = max;
}
inline float lerp(float a, float b, float t)
{
    return (1.0f - t) * a + t * b;
}

inline bool randbool()
{
    return ((rand() & 1) == 0);
}
inline bool randchance(float probability) // 0.00 - 1.00
{
    return (( (float)rand() / (float)RAND_MAX ) < probability);
}
inline int randint(int min, int max)
{
    return min + (rand() % (max - min + 1));
}
inline float randfloat(float min, float max)
{
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}
inline float remap(float value, float low1, float high1, float low2, float high2)
{
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}
inline float smoothstep(float edge0, float edge1, float x)
{
    float t = clampfloat(0.0f, 1.0f, (x - edge0) / (edge1 - edge0) );
    return t * t * (3.0f - 2.0f * t);
}
inline float wrapfloat(float value, float min, float max)
{
    float range = max - min;
    float result = fmodf(value - min, range);
    if(result < 0.0f) result += range;
    return result + min;
}

#define ONE_OVER_PI 0.31830988618379067154f
inline float ultra_fastsin(float x) // [-PI; PI], very inaccurate !!!
{
    float y = 1.27323954f * x - 0.405284735f * x * (x < 0.0f ? -x : x);
    return ( 0.225f * (y * (y < 0.0f ? -y : y) - y) + y );
}
inline float ultra_fastcos(float x) // [-PI; PI], very inaccurate !!!
{
    float x_shifted = 1.57079632f - (x < 0.0f ? -x : x);
    float y = 1.27323954f * x_shifted - 0.405284735f * x_shifted * (x_shifted < 0.0f ? -x_shifted : x_shifted);
    return ( 0.225f * (y * (y < 0.0f ? -y : y) - y) + y );
}
inline void ultra_fastsincos(float x, float* out_sin, float* out_cos) // [-PI; PI], very inaccurate !!!
{
    float abs_x = (x < 0.0f) ? -x : x;
    float x_cos = 1.57079632f - abs_x;
    
    float y_sin = 1.27323954f * x - 0.405284735f * x * (x < 0.0f ? -x : x);
    float y_cos = 1.27323954f * x_cos - 0.405284735f * x_cos * (x_cos < 0.0f ? -x_cos : x_cos);
    
    *out_sin = 0.225f * (y_sin * (y_sin < 0.0f ? -y_sin : y_sin) - y_sin) + y_sin;
    *out_cos = 0.225f * (y_cos * (y_cos < 0.0f ? -y_cos : y_cos) - y_cos) + y_cos;
}
inline float fastsin(float x) // much better than above but not guaranteed to be the best!
{
    float q = roundf(x * ONE_OVER_PI);
    float x_reduced = x - q * M_PI;

    float sign = (float)(1 - ((int)q & 1) * 2);
    float x2 = x_reduced * x_reduced;
    float res = x_reduced * (1.0f + x2 * (-0.1666665671f + 
                x2 * (0.0083321510f + 
                x2 * (-0.0001951529f))));
    return res * sign;
}
inline float fastcos(float x) 
{
    float q = roundf((x + (float)M_PI * 0.5f) * ONE_OVER_PI);
    float x_reduced = (x + (float)M_PI * 0.5f) - q * (float)M_PI;

    float sign = (float)(1 - ((int)q & 1) * 2);
    float x2 = x_reduced * x_reduced;
    float res = x_reduced * (1.0f + x2 * (-0.1666665671f + 
                x2 * (0.0083321510f + 
                x2 * (-0.0001951529f))));
    return res * sign;
}
inline void fastsincos(float x, float* out_sin, float* out_cos) 
{
    float q_sin = roundf(x * ONE_OVER_PI);
    float x_sin = x - q_sin * (float)M_PI;
    float sign_sin = (float)(1 - ((int)q_sin & 1) * 2);
    
    float q_cos = roundf((x + (float)M_PI * 0.5f) * ONE_OVER_PI);
    float x_cos = (x + (float)M_PI * 0.5f) - q_cos * (float)M_PI;
    float sign_cos = (float)(1 - ((int)q_cos & 1) * 2);
    
    float x2_sin = x_sin * x_sin;
    float x2_cos = x_cos * x_cos;

    float res_sin = x_sin * (1.0f + x2_sin * (-0.1666665671f + x2_sin * (0.0083321510f + x2_sin * (-0.0001951529f))));
    float res_cos = x_cos * (1.0f + x2_cos * (-0.1666665671f + x2_cos * (0.0083321510f + x2_cos * (-0.0001951529f))));

    *out_sin = res_sin * sign_sin;
    *out_cos = res_cos * sign_cos;
}
template <typename T> int getsign(T val)
{
    return (T(0) < val) - (val < T(0));
}



#define ARRAY_SIZE(__aVar)  ((size_t)( sizeof(__aVar) / sizeof(__aVar[0]) ))
#define RAD_TO_DEG(__f) ( (__f) * (180.0f / M_PI) )
#define DEG_TO_RAD(__f) ( (__f) * (M_PI / 180.0f) )

class ModInfo
{
public:
    ModInfo(const char* szGUID, const char* szName, const char* szVersion, const char* szAuthor)
    {
        /* No buffer overflow! */
        strxcpy(this->szGUID, szGUID, sizeof(ModInfo::szGUID)); this->szGUID[sizeof(ModInfo::szGUID) - 1] = '\0';
        strxcpy(this->szName, szName, sizeof(ModInfo::szName)); this->szName[sizeof(ModInfo::szName) - 1] = '\0';
        strxcpy(this->szVersion, szVersion, sizeof(ModInfo::szVersion)); this->szVersion[sizeof(ModInfo::szVersion) - 1] = '\0';
        strxcpy(this->szAuthor, szAuthor, sizeof(ModInfo::szAuthor)); this->szAuthor[sizeof(ModInfo::szAuthor) - 1] = '\0';

        version.major = 0;
        version.minor = 0;
        version.revision = 0;
        version.build = 0;

        /* GUID should be lowcase */
        for(int i = 0; this->szGUID[i] != '\0'; ++i)
        {
            this->szGUID[i] = (char)tolower((unsigned char)this->szGUID[i]);
        }

        /* Parse version string */
        if(sscanf(this->szVersion, "%hu.%hu.%hu.%hu", &version.major, &version.minor, &version.revision, &version.build) < 4)
        {
            if(sscanf(this->szVersion, "%hu.%hu.%hu", &version.major, &version.minor, &version.revision) < 3)
            {
                if(sscanf(this->szVersion, "%hu.%hu", &version.major, &version.minor) < 2)
                {
                    version.major = (unsigned short)atoi(this->szVersion);
                }
                version.revision = 0;
            }
            version.build = 0;
        }
    }
    inline const char* GUID() { return szGUID; }
    inline const char* Name() { return szName; }
    inline const char* VersionString() { return szVersion; }
    inline const char* Author() { return szAuthor; }
    inline unsigned short Major() { return version.major; }
    inline unsigned short Minor() { return version.minor; }
    inline unsigned short Revision() { return version.revision; }
    inline unsigned short Build() { return version.build; }

private:
    char szGUID[48];
    char szName[48];
    char szVersion[24];
    char szAuthor[48];
    ModVersion version;

    friend class ModsList;
    friend struct Mods;
};

typedef ModInfo* (*GetModInfoFn)();
extern ModInfo* modinfo;


#include "iaml.h"

#endif // _AMLMOD
