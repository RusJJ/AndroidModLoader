#pragma once

#include <stddef.h> // use of undeclared identifier 'NULL'

#define LIST_START(__cls_name) struct __cls_name { \
    __cls_name *pPrev; \
    __cls_name *pNext; \
    __cls_name *pLast; \
    unsigned int nCount; \
    typedef __cls_name MyClass; \
    \
    inline unsigned int Count() { return !this ? 0 : First()->nCount; } \
    inline __cls_name *First() \
    { \
        if(!this) return NULL; \
        __cls_name *first = this; \
        if(first->pPrev == this) { first->pPrev = NULL; return this; } \
        while(first->pPrev != NULL) first = first->pPrev; \
        return first; \
    } \
    inline __cls_name *CalcLast() \
    { \
        if(!this) return NULL; \
        __cls_name *last = this; \
        while(last->pNext != NULL) last = last->pNext; \
        return last; \
    } \
    inline __cls_name *Last() \
    { \
        return pLast; \
    } \
    inline void Push(__cls_name **listPtr) \
    { \
        __cls_name*& list = *listPtr; \
        pPrev = NULL; \
        if(list == NULL) { \
            pNext = NULL; \
            pLast = this; \
            nCount = 1; \
        } else { \
            pNext = list; \
            pLast = list->pLast; \
            list->pPrev = this; \
            nCount = list->nCount + 1; \
        } \
        list = this; \
    } \
    inline bool Remove(__cls_name **listPtr) { \
        if(!listPtr || !*listPtr || !pLast) return false; \
        __cls_name*& list = *listPtr; \
        if(list == this) { \
            if(pNext) { \
                list = pNext; \
                list->nCount = nCount - 1; \
                list->pPrev = NULL; \
                list->pLast = pLast; \
            } else { \
                list = NULL; \
            } \
        } \
        else if(list->pLast == this) { \
            list->pLast = pPrev; \
            pPrev->pNext = NULL; \
            --(list->nCount); \
        } else { \
            pPrev->pNext = pNext; \
            if(pNext) pNext->pPrev = pPrev; \
            --(list->nCount); \
        } \
        pNext = NULL; pPrev = NULL; pLast = NULL; \
        return true; \
    } \
    inline bool InList(__cls_name **listPtr) \
    { \
        LIST_FOR(*listPtr) { \
            if(item == this) return true; \
        } \
        return false; \
    }

#define LIST_END() \
};

#define LIST_INITSTART(__cls_name) \
    __cls_name() {

#define LIST_INITEND() \
        pPrev = NULL; \
        pNext = NULL; \
        pLast = NULL; \
        nCount = 1; \
    }

// Never use FAST versions if you do "Remove" or "Push" in a loop! Or maintain it by yourself!

#define LIST_FOR(__list) for(auto item = __list, itemNext = item ? item->pNext : NULL; item != NULL; item = itemNext, itemNext = item ? item->pNext : NULL)
#define LIST_FOR_FAST(__list) for(auto item = __list; item != NULL; item = item->pNext)
#define LIST_FOR2(__list, __itemname) for(auto __itemname = __list, itemNext = __itemname ? __itemname->pNext : NULL; __itemname != NULL; __itemname = itemNext, itemNext = __itemname ? __itemname->pNext : NULL)
#define LIST_FOR2_FAST(__list, __itemname) for(auto __itemname = __list; __itemname != NULL; __itemname = __itemname->pNext)
#define LIST_FOR_REVERSE(__list) for(auto item = __list ? __list->pLast : NULL, itemPrev = item ? item->pPrev : NULL; item != NULL; item = itemPrev, itemPrev = item ? item->pPrev : NULL)
#define LIST_FOR_REVERSE_FAST(__list) for(auto item = __list ? __list->pLast : NULL; item != NULL; item = item->pPrev)
#define LIST_RESET(__list, __resetFunc) LIST_FOR(__list) { __resetFunc(); item->Remove(&__list); }