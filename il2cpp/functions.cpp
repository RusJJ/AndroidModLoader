#include <mod/iaml.h>
#include <mod/logger.h>
#include <dlfcn.h>

namespace IL2CPP::Func
{
    void* pIL2CPP = NULL;
    static bool m_bInitialized = false;
    void HookFunctions()
    {
        if(m_bInitialized) return;

        if(!(pIL2CPP = dlopen("libil2cpp.so", RTLD_LAZY)))
        {
            logger->Error("IL2CPP: Looks like this is not a Unity game?");
            return;
        }

        m_bInitialized = true;
    }
}