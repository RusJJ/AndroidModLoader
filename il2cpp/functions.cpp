#include <mod/iaml.h>
#include <mod/logger.h>
#include <dlfcn.h>

namespace IL2CPP::Func
{
    static bool m_bInitialized = false;
    void HookFunctions()
    {
        if(m_bInitialized) return;

        void* pIL2CPP = dlopen("libil2cpp.so", RTLD_LAZY);
        if(!pIL2CPP)
        {
            logger->Error("Looks like we dont have IL2CPP?");
            return;
        }

        m_bInitialized = true;
    }
}