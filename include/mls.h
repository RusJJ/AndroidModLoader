#include <cstdint>

#define MAX_KEY_LEN 12 // Maximum KEY NAME length!
#define MAX_VAL_LEN 24 // Maximum STR VALUE length!

class MLS
{
public:

// Generic
    static void LoadFile();
    static void SaveFile();
    static bool HasValue(const char* key);
    static void DeleteValue(const char* key);

// Set
    static void SetInt(const char* key, int32_t val);
    static void SetFloat(const char* key, float val);
    static void SetInt64(const char* key, int64_t val);
    static void SetStr(const char* key, const char *val);

// Get
    static bool GetInt(const char* key, int32_t *val);
    static bool GetFloat(const char* key, float *val);
    static bool GetInt64(const char* key, int64_t *val);
    static bool GetStr(const char* key, char *val, size_t len);
};