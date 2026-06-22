#pragma once

class InterfaceSys
{
public:
    typedef void(*_ListInterfacesCallback)(const char* name, void* pointer, void* data);
    
    void  Register(const char* szInterfaceName, void* pInterfacePointer);
    void* Get(const char* szInterfaceName);
    int   Count();
    void  ListInterfaces(_ListInterfacesCallback cb, void* data = NULL);
};

extern InterfaceSys* interfaces;