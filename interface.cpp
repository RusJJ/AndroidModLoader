#include <jni.h>
#include "interfaces.h"
#include "mod/logger.h"
#define _IAML // Do not include "interface" twice!
#include "modslist.h"

struct InterfaceStorage;
InterfaceStorage* pInterfaceList = NULL;

struct InterfaceStorage
{
    InterfaceStorage* pPrev;
    InterfaceStorage* pNext;
    void* pInterface;
    char szName[48];

    InterfaceStorage(const char* name, void* interface)
    {
        pInterface = interface;
        strncpy(szName, name, 48);

        pPrev = NULL;
        if(pInterfaceList == NULL) pNext = NULL;
        else
        {
            pNext = pInterfaceList;
            pInterfaceList->pPrev = this;
        }
        pInterfaceList = this;
    }
};

void InterfaceSys::Register(const char* szInterfaceName, void* pInterfacePointer)
{
    if(Get(szInterfaceName) != NULL)
    {
        logger->Error("Failed to add interface %s to the list because it`s already registered!", szInterfaceName);
        return;
    }
    if(pInterfacePointer == NULL)
    {
        logger->Error("Failed to add interface %s to the list because it`s NULL!", szInterfaceName);
        return;
    }
    new InterfaceStorage(szInterfaceName, pInterfacePointer);
    modlist->OnInterfaceAdded(szInterfaceName, pInterfacePointer);
}

void* InterfaceSys::Get(const char* szInterfaceName)
{
    InterfaceStorage* storage = pInterfaceList;
    while (storage != NULL)
    {
        if (!strcmp(storage->szName, szInterfaceName))
        {
            return (storage->pInterface);
        }
        storage = storage->pNext;
    }
    return NULL;
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