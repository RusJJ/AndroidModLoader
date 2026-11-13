#ifndef _IYML
#define _IYML

// Not ready for use.

#include <string>

#define YML_INTERFACE_STR "AMLConfigYML"

typedef struct JYML* YMLHandle;

class IYML
{
public:
    virtual YMLHandle* InitYmlPointer() = 0;
    virtual bool Open(YMLHandle* yml, const char* szFile) = 0;
    virtual bool Save(YMLHandle* yml) = 0;
    virtual bool SaveAs(YMLHandle* yml, const char* szFile) = 0;
    virtual void ParseFrom(YMLHandle* yml, std::string szText) = 0;
};

#endif // _IYML