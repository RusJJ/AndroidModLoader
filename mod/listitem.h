#pragma once

#define LIST_START(__cls_name) class __cls_name { \
public: \
    __cls_name *pPrev; \
    __cls_name *pNext; \
    unsigned int nCount; \
    typedef __cls_name MyClass; \
    \
    unsigned int Count() { return First()->nCount; } \
    __cls_name *First() \
    { \
        __cls_name *first = this; \
        while(first->pPrev != NULL) first = first->pPrev; \
        return first; \
    } \
    __cls_name *Last() \
    { \
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
    void Remove(__cls_name **listPtr) \
    { \
        if(First() != *listPtr) return; \
        if(First() == this && Last() == this) { *listPtr = NULL; return; } \
        if(pPrev) { \
            --(First()->nCount); \
            pPrev->pNext = pNext; \
        } else { \
            pNext->nCount = nCount - 1; \
        } \
        if(pNext) pNext->pPrev = pPrev; \
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