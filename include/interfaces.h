#pragma once

#include <map>
#include <string>

#define WRAP_INTERFACE(__interface_name, __interface_var)	interfaces->Register(#__interface_name, __interface_var)

class InterfaceSys
{
public:
	void							Register(std::string szInterfaceName, void* pInterfacePointer);
	void*							Get(std::string szInterfaceName);
};

extern InterfaceSys* interfaces;