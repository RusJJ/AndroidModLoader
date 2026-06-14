#ifndef __LOCALREF_H
#define __LOCALREF_H

#include <functional>

// When LocalRef is instantiated, it calls your FIRST function
// When LocalRef gets deleted, it calls your SECOND function
// It automatically cleans up your memory when it leaves the current scope
// Detailed example is THE LATEST

// Example:
// LocalRef<void*> buf(malloc(1024), [](void* p){ free(p); });
// LocalRef<FILE*> f(fopen("/sdcard/x.txt", "r"), [](FILE* fp){ fclose(fp); });
// LocalRef<void*> asset(aml->OpenAsset("data.bin"), [](void* a){ aml->CloseAsset(a); });

// JNIEnv* env = aml->GetJNIEnvironment();
// LocalRef<jclass> cls(env->FindClass("java/lang/String"), [env](jclass c){ env->DeleteLocalRef(c); });

//ON_MOD_LOAD()
//{
//    JNIEnv* env = aml->GetJNIEnvironment();
//    LocalRef<jclass> cls(env->FindClass("java/lang/String"), [env](jclass c){ env->DeleteLocalRef(c); });
//    // *using your cls variable here*
//
//    // You NEED to delete reference when its done.
//    // LocalRef is out of the function's scope after it's ended,
//    //   that means it calls LocalRef's destruction and cls' ref is cleared.
//    // You dont need to call env->DeleteLocalRef(cls); automatically.
//}

template<typename T>
class LocalRef
{
    T    m_obj;
    bool m_owns;
    std::function<void(T)> m_deleter;

public:
    LocalRef() : m_obj(T()), m_owns(false) {}
    LocalRef(T obj, std::function<void(T)> del) : m_obj(obj), m_owns(true), m_deleter(std::move(del)) {}

    ~LocalRef() { Release(); }

    LocalRef(const LocalRef&) = delete;
    LocalRef& operator=(const LocalRef&) = delete;

    LocalRef(LocalRef&& o) noexcept : m_obj(o.m_obj), m_owns(o.m_owns), m_deleter(std::move(o.m_deleter)) { o.m_owns = false; }
    LocalRef& operator=(LocalRef&& o) noexcept
    {
        if(this != &o)
        {
            Release();
            m_obj = o.m_obj; m_owns = o.m_owns; m_deleter = std::move(o.m_deleter);
            o.m_owns = false;
        }
        return *this;
    }

    void Release()      { if(m_owns && m_deleter) m_deleter(m_obj); m_owns = false; }
    T    Get() const    { return m_obj; }
    bool Valid() const  { return m_owns; }
    operator T() const  { return m_obj; }
    T Detach()          { m_owns = false; return m_obj; }
    explicit operator bool() const { return m_owns; }
};

#endif