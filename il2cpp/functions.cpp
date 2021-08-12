#include <mod/iaml.h>
#include <mod/logger.h>

namespace IL2CPP::Func
{
    static bool m_bInitialized = false;
    void HookFunctions()
    {
        if(m_bInitialized) return;

        uintptr_t pIL2CPP = aml->GetLib("libil2cpp.so");
        if(!pIL2CPP)
        {
            logger->Error("Looks like we dont have IL2CPP?");
            return;
        }

        m_bInitialized = true;
    }
}