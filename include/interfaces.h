#pragma once

class InterfaceSys
{
public:
	void							Register(const char* szInterfaceName, void* pInterfacePointer);
	void*							Get(const char* szInterfaceName);
};

extern InterfaceSys* interfaces;