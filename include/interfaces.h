#pragma once

#include <map>
#include <string>

class InterfaceSys
{
public:
	void							Register(std::string szInterfaceName, void* pInterfacePointer);
	void*							Get(std::string szInterfaceName);
};

extern InterfaceSys* interfaces;