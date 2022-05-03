#include <map>
#include <jni.h>
#include "include/interfaces.h"
#include "mod/logger.h"
#define _IAML // Do not include "interface" twice!
#include "include/modslist.h"

static std::map<const char*, void*> g_hInterfacesMap;

void InterfaceSys::Register(const char* szInterfaceName, void* pInterfacePointer)
{
	if(pInterfacePointer == NULL)
	{
		logger->Error("Failed to add interface %s to list because it's NULL!", szInterfaceName);
		return;
	}
	auto ret = g_hInterfacesMap.insert(std::pair<const char*, void*>(szInterfaceName, pInterfacePointer));
	if(!ret.second)
	{
		logger->Error("Failed to add interface %s to list!", szInterfaceName);
		return;
	}
	modlist->OnInterfaceAdded(szInterfaceName, pInterfacePointer);
}

void* InterfaceSys::Get(const char* szInterfaceName)
{
	auto it = g_hInterfacesMap.begin();
	auto end = g_hInterfacesMap.end();

	while (it != end)
	{
		if (!strcmp(it->first, szInterfaceName))
		{
			return (it->second);
		}
		++it;
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
