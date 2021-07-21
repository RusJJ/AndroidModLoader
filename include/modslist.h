#include <vector>
#include <mod/amlmod.h>

class ModsList
{
public:
    bool AddMod(ModInfo* modinfo, uintptr_t modhandle);
    bool RemoveMod(ModInfo* modinfo);
    bool RemoveMod(const char* szGUID);
    bool HasMod(const char* szGUID);
    bool HasModOfVersion(const char* szGUID, const char* szVersion);
    void ProcessDependencies();
    void ProcessPreLoading();
    void ProcessLoading();

private:
    std::vector<ModInfo*> m_vecModInfo;
};

extern ModsList* modlist;