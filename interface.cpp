#include "interfaces.h"
#include "mod/logger.h"
#include <jni.h>

static std::map<std::string, void*> g_hInterfacesMap;

void InterfaceSys::Register(std::string szInterfaceName, void* pInterfacePointer)
{
	if(pInterfacePointer == nullptr)
	{
		logger->Error("Failed to add interface %s to list because it's NULLPTR!", szInterfaceName.c_str());
		return;
	}
	auto ret = g_hInterfacesMap.insert(std::pair<std::string, void*>(szInterfaceName, pInterfacePointer));
	if(!ret.second)
	{
		logger->Error("Failed to add interface %s to list!", szInterfaceName.c_str());
	}
}

void* InterfaceSys::Get(std::string szInterfaceName)
{
	auto it = g_hInterfacesMap.begin();
	auto it_end = g_hInterfacesMap.end();

	while (it != it_end)
	{
		if (it->first == szInterfaceName)
		{
			return (it->second);
		}
		++it;
	}
	return nullptr;
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