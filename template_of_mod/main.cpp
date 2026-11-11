#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/flogger.h>
#include <mod/config.h>

MYMODCFG(net.rusjj.mymod.guid, AML Mod Template, 1.0, RusJJ)

//MYMOD(net.rusjj.mymod.guid, AML Mod Template Without Config, 1.0, RusJJ)

//NEEDGAME(net.rusjj.mygame)

//BEGIN_DEPLIST()
//    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.0)
//END_DEPLIST()

uintptr_t pGameLibrary = 0;
ConfigEntry* pCfgMyBestEntry;
FLogger file_log("warns.txt"); // /sdcard/Documents/net.rusjj.mymod.guid/warns.txt
FLogger file_log2("baa/errors.log"); // /sdcard/Documents/net.rusjj.mymod.guid/baa/errors.log

ON_MOD_LOAD()
{
    logger->SetTag("Mod Template");
    
    file_log.Write("Loading the mod...");
    
    pGameLibrary = aml->GetLib("libMyGame.so");
    if(pGameLibrary)
    {
        logger->Info("MyGame mod is loaded!");
    }
    else
    {
        logger->Error("MyGame mod is not loaded :(");
        file_log2.Error("Mod is not loaded!");
        return; // Do not load our mod?
    }

    pCfgMyBestEntry = cfg->Bind("mySetting", "DefaultValue is 0?", "MyUniqueSection");
    pCfgMyBestEntry->SetString("DefaultValue is unchanged");
    pCfgMyBestEntry->SetInt(1);
    pCfgMyBestEntry->Reset();
    delete pCfgMyBestEntry; // Clean-up memory
    
    bool bEnabled = cfg->Bind("Enable", true)->GetBool();
    delete Config::pLastEntry; // Clean-up of the latest ConfigEntry*
    
    cfg->Save(); // Will only save if something was changed
    
    file_log.Info("Mod is loaded, config saved.");
}
