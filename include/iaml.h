class IAML
{
public:

};

#ifdef __AML
    extern IAML* aml;
    inline IAML* GetAMLInterface()
    {
        return aml;
    }
#else
    #include <interface.h>
    inline IAML* GetAMLInterface()
    {
        return (IAML*)GetInterface("AMLInterface");
    }
#endif