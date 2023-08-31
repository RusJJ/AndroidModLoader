#pragma once

#define LIST_START(__cls_name) class __cls_name { \
public: \
    __cls_name *pPrev; \
    __cls_name *pNext; \
    unsigned int nCount; \
    typedef __cls_name MyClass; \
    \
    unsigned int Count() { return !this ? 0 : First()->nCount; } \
    __cls_name *First() \
    { \
        if(!this) return NULL; \
        __cls_name *first = this; \
        while(first->pPrev != NULL) first = first->pPrev; \
        return first; \
    } \
    __cls_name *Last() \
    { \
        if(!this) return NULL; \
        __cls_name *last = this; \
        while(last->pNext != NULL) last = last->pNext; \
        return last; \
    } \
    void Push(__cls_name **listPtr) \
    { \
        if(pPrev || pNext) Remove(); \
        __cls_name*& list = *listPtr; \
        pPrev = NULL; \
        if(list == NULL) { \
            pNext = NULL; \
            nCount = 1; \
        } else { \
            pNext = list; \
            list->pPrev = this; \
            nCount = list->nCount + 1; \
        } \
        list = this; \
    } \
    void Remove() /* risky removal! */ \
    { \
        if(pPrev) { \
            --(First()->nCount); \
            pPrev->pNext = pNext; \
        } else { \
            pNext->nCount = nCount - 1; \
        } \
        if(pNext) pNext->pPrev = pPrev; \
    } \
    bool Remove(__cls_name **listPtr) \
    { \
        if(First() != *listPtr) return false; \
        if(pPrev && pNext) { \
            pPrev->pNext = pNext; \
            pNext->pPrev = pPrev; \
            --(*listPtr)->nCount; \
        } else if(pPrev) { \
            pPrev->pNext = NULL; \
            --(*listPtr)->nCount; \
        } else if(pNext) { \
            *listPtr = pNext; \
            pNext->pPrev = NULL; \
            pNext->nCount = nCount - 1; \
        } else { \
            *listPtr = NULL; \
        } \
        return true; \
    }

#define LIST_END() \
};

#define LIST_INITSTART(__cls_name) \
    __cls_name() {

#define LIST_INITEND() \
        pPrev = NULL; \
        pNext = NULL; \
        nCount = 1; \
    }

#define LIST_FOR(__list) for(auto item = __list; item != NULL; item = item->pNext)
#define LIST_FOR2(__list, __itemname) for(auto __itemname = __list; __itemname != NULL; __itemname = __itemname->pNext)