#include "interfaces.h"
#include <jni.h>

static std::map<std::string, void*> g_hInterfacesMap;

void InterfaceSys::Register(std::string szInterfaceName, void* pInterfacePointer)
{
	g_hInterfacesMap.insert(std::pair<std::string, void*>(szInterfaceName, pInterfacePointer));
}

void* InterfaceSys::Get(std::string szInterfaceName)
{
	auto it = g_hInterfacesMap.begin();
	auto it_end = g_hInterfacesMap.end();

	while (it != it_end)
	{
		if (it->first == szInterfaceName) return (it->second);
		it++;
	}
	return 0;
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