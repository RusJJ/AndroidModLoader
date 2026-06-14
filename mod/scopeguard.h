#ifndef __SCOPEGUARD_H
#define __SCOPEGUARD_H

// Similar to LocalRef. But completely different.
// It calls a function inside DEFER(...) when its out of scope.
// Yes, LocalRef does the same. ScopeGuard DOES NOT instantiate your variables.
// You can use it to safely cleanup repeating stuff when it ends the function/scope.

// Example:
//ON_MOD_LOAD()
//{
//    JNIEnv* env = aml->GetJNIEnvironment();
//    jclass cls = env->FindClass("java/lang/String");
//    DEFER( env->DeleteLocalRef(cls) ); // Declare our ScopeGuard (should be after the variable)
//    if(g_bThisMightBeFalse)
//    {
//        // Need to call env->DeleteLocalRef(cls) but...
//        return; // THIS CALLS DEFER(...)
//    }
//    // The function is ended. Calling DEFER(...) here.
//    // You do not need to copy-paste env->DeleteLocalRef(cls) twice because of DEFER
//}

template<typename F>
class ScopeGuard
{
public:
    explicit ScopeGuard(F fn) : m_fn(std::move(fn)), m_active(true) {}
    ~ScopeGuard() { if(m_active) m_fn(); }
    ScopeGuard(ScopeGuard&& o) noexcept : m_fn(std::move(o.m_fn)), m_active(o.m_active) { o.m_active = false; }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    void Dismiss() { m_active = false; }
    
private:
    F    m_fn;
    bool m_active;
};
template<typename F>
ScopeGuard<F> MakeScopeGuard(F f)
{
    return ScopeGuard<F>(std::move(f));
}

#define __DEFER_CAT(a,b) a##b
#define __DEFER_CAT2(a,b) __DEFER_CAT(a,b)
#define DEFER(code) auto __DEFER_CAT2(__defer_, __COUNTER__) = MakeScopeGuard([&](){ code; })

#endif // __SCOPEGUARD_H