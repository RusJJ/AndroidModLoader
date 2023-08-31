#include <jni.h>
#include "interfaces.h"
#include "mod/logger.h"
#define _IAML // Do not include "interface" twice!
#include "modslist.h"
#include "mod/listitem.h"

class Interfaces;
Interfaces* listInterfaces = NULL;
LIST_START(Interfaces)

    LIST_INITSTART(Interfaces)
        pInterface = NULL;
        szName[0] = 0;
    LIST_INITEND()

    static void AddNew(const char* name, void* interface)
    {
        Interfaces* newItem = new Interfaces;
        newItem->pInterface = interface;
        strxcpy(newItem->szName, name, 48);
        newItem->Push(&listInterfaces);
    }
    static void* Get(const char* name)
    {
        LIST_FOR(listInterfaces)
        {
            if (!strcmp(item->szName, name)) return item->pInterface;
        }
        return NULL;
    }

    void* pInterface;
    char szName[48];
LIST_END()

void InterfaceSys::Register(const char* szInterfaceName, void* pInterfacePointer)
{
    if(szInterfaceName == NULL)
    {
        logger->Error("Failed to add unknown interface to the list because it`s NULL!");
        return;
    }
    if(pInterfacePointer == NULL)
    {
        logger->Error("Failed to add interface %s to the list because it`s NULL!", szInterfaceName);
        return;
    }
    if(Interfaces::Get(szInterfaceName) != NULL)
    {
        logger->Error("Failed to add interface %s to the list because it`s already registered!", szInterfaceName);
        return;
    }
    Interfaces::AddNew(szInterfaceName, pInterfacePointer);
    modlist->OnInterfaceAdded(szInterfaceName, pInterfacePointer);
}

void* InterfaceSys::Get(const char* szInterfaceName)
{
    return Interfaces::Get(szInterfaceName);
}

static InterfaceSys interfacesLocal;
InterfaceSys* interfaces = &interfacesLocal;

extern "C"
{
    JNIEXPORT void* GetInterface(const char* szInterfaceName)
    {
        return interfacesLocal.Get(szInterfaceName);
    }
    JNIEXPORT void CreateInterface(const char* szInterfaceName, void* pInterface)
    {
        return interfacesLocal.Register(szInterfaceName, pInterface);
    }
}