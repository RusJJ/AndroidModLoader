#include <stdio.h>

#define MYMOD(_name, _version, _author)                         \
    static ModInfo modinfoLocal(#_name, #_version, #_author);   \
    ModInfo* modinfo = &modinfoLocal;                           \
    extern "C" ModInfo* GetModInfo() { return modinfo; }

class ModInfo
{
public:
    ModInfo(const char* szName, const char* szVersion, const char* szAuthor)
    {
        this->szName = szName;
        this->szVersion = szVersion;
        this->szAuthor = szAuthor;

        if(sscanf(szVersion, "%hu.%hu.%hu.%hu", &major, &minor, &revision, &build) < 4)
        {
            if(sscanf(szVersion, "%hu.%hu.%hu", &major, &minor, &revision) < 3)
            {
                if(sscanf(szVersion, "%hu.%hu", &major, &minor) < 2)
                {
                    major = (unsigned short)atoi(szVersion);
                }
                revision = 0;
            }
            build = 0;
        }
    }
    inline const char* GetName() { return szName; }
    inline const char* GetVersion() { return szVersion; }
    inline const char* GetAuthor() { return szAuthor; }
    inline unsigned short Major() { return major; }
    inline unsigned short Minor() { return minor; }
    inline unsigned short Revision() { return revision; }
    inline unsigned short Build() { return build; }
private:
    const char* szName;
    const char* szVersion;
    const char* szAuthor;
    unsigned short major;
    unsigned short minor;
    unsigned short revision;
    unsigned short build;
};

typedef ModInfo* (*GetModInfoFn)();